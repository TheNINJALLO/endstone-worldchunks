"""Check vanilla loading/simulation and structure commands in the disposable lab."""

import json
import time
from lab_console import PLATFORM, ROOT, check, checks, command, inspect


def opt(args="status"):
    return json.loads(command("wco " + args, "WORLDCHUNKS_OPTIMIZER "))


def wait(predicate, timeout=45):
    deadline = time.monotonic() + timeout
    while not predicate():
        assert time.monotonic() < deadline, "condition timed out"
        time.sleep(0.2)


check("no players online", not command("wc players"))
original = opt()["config"]
policy = command("wc policy")
area_added = False
structure_saved = False
try:
    check(
        "legacy config migrated to compatibility mode", original["compatibility_mode"]
    )
    opt("off")
    command("wc reset")
    native_offsets = command("wc status")["effective_tick_offsets"]
    opt("set compatibility_mode false")
    opt("set cleanup_interval_seconds 0")
    opt("set tps_threshold 0")
    opt("set simulation_radius 1")
    command("tickingarea add circle 648 0 8 2 wc_compat true", "Added ticking area")
    area_added = True
    wait(
        lambda: inspect(40, 0).get("state") == 11 and inspect(42, 0).get("state") == 11
    )
    command("setblock 648 20 8 gold_block", "Block placed")
    command("setblock 648 20 8 diamond_block", "Block placed")
    opt("on")
    opt("run")
    wait(lambda: not inspect(40, 0)["resident"])
    check("aggressive mode reproduces remote ticking-area eviction", True)
    check(
        "aggressive simulation cap applied",
        command("wc status")["effective_tick_offsets"] < native_offsets,
    )
    # A manual cap must not silently remain active in compatibility mode.
    command("wc radius 1")
    opt("set compatibility_mode true")
    native = opt()["native"]
    check("compatibility clears automatic denies", native["temporary_denies"] == 0)
    check(
        "compatibility cancels pending cleanup",
        native["pending_chunks"] == 0 and not native["cleanup_requested"],
    )
    check(
        "compatibility disables forced cleanup", not native["automatic_cleanup_active"]
    )
    check(
        "compatibility restores vanilla simulation despite saved manual cap",
        command("wc status")["effective_tick_offsets"] == native_offsets,
    )
    wait(
        lambda: inspect(40, 0).get("state") == 11 and inspect(42, 0).get("state") == 11
    )
    check(
        "compatibility restores the ticking area without a protection lease",
        not inspect(40, 0)["protected"],
    )
    # Exercise both trigger conditions, with a real TPS reading at threshold 20.
    opt("set cleanup_interval_seconds 1")
    opt("set tps_threshold 20")
    opt("set low_tps_seconds 1")
    before = opt()
    first_tick = inspect(40, 0)["last_tick"]
    time.sleep(7)
    after = opt()
    check(
        "interval and TPS triggers suppressed in compatibility mode",
        after["interval_triggers"] == before["interval_triggers"]
        and after["tps_triggers"] == before["tps_triggers"],
    )
    check(
        "ticking-area chunk stays loaded and ticking",
        inspect(40, 0).get("state") == 11 and inspect(40, 0)["last_tick"] > first_tick,
    )
    command("wco run", "forced cleanup is inactive")
    check(
        "manual optimizer run cannot bypass compatibility",
        opt()["native"]["temporary_denies"] == 0,
    )
    command(
        "structure save wc_compat_probe 648 20 8 648 20 8 false memory true",
        "Saved a structure",
    )
    structure_saved = True
    command("setblock 680 20 8 gold_block", "Block placed")
    command("structure load wc_compat_probe 680 20 8", "Loaded a structure")
    command(
        "execute if block 680 20 8 diamond_block run say WC_COMPAT_STRUCTURE_OK",
        "WC_COMPAT_STRUCTURE_OK",
    )
    check("structure save/load succeeds remotely with compatibility mode enabled", True)
    opt("reload")
    check("compatibility survives config reload", opt()["native"]["compatibility_mode"])
    command("wco set compatibility_mode 1", "Invalid type for compatibility_mode")
    check(
        "nonboolean mode rejected without disabling protection",
        opt()["native"]["compatibility_mode"],
    )
    command("wc unload Overworld 42 0")
    check(
        "explicit manual unload remains an operator override", inspect(42, 0)["denied"]
    )
    command("wc allow Overworld 42 0")
finally:
    opt("off")
    if structure_saved:
        command("structure delete wc_compat_probe", "deleted.")
    if area_added:
        command("tickingarea remove wc_compat", "Removed")
    command("wc reset")
    command(f"wc radius {policy['simulation_radius_cap']}")
    for field, operation in (("denied", "unload"), ("pins", "load")):
        for entry in policy[field]:
            command(f"wc {operation} {entry['dimension']} {entry['x']} {entry['z']}")
    for key, value in original.items():
        if key not in {"schema", "enabled"}:
            opt(f"set {key} {json.dumps(value)}")
    opt("on" if original["enabled"] else "off")

evidence = ROOT / "evidence" / "compatibility-mode" / PLATFORM
evidence.mkdir(parents=True, exist_ok=True)
(evidence / "live.json").write_text(
    json.dumps({"passed": len(checks), "checks": checks}, indent=2) + "\n"
)
print(json.dumps({"passed": len(checks)}), flush=True)

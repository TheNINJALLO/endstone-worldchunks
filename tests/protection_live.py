"""Protection regression on the disposable lab only; no players may be connected."""

import json
import time
from lab_console import PLATFORM, ROOT, check, checks, command, inspect

EVIDENCE = ROOT / "evidence" / "integration-protection" / PLATFORM
EVIDENCE.mkdir(parents=True, exist_ok=True)


def opt(args):
    return json.loads(command("wco " + args, "WORLDCHUNKS_OPTIMIZER "))


def wait(predicate, timeout=40):
    deadline = time.monotonic() + timeout
    while not predicate():
        assert time.monotonic() < deadline, "condition timed out"
        time.sleep(0.15)


check("no players online", not command("wc players"))
original = opt("status")["config"]
original_policy = command("wc policy")
try:
    opt("off")
    opt("set compatibility_mode false")
    command("wc reset")
    opt("set cleanup_interval_seconds 0")
    opt("set tps_threshold 0")
    opt("set batch_size 1")
    command("tickingarea add circle 8 0 8 2 wc_protect true", "Added ticking area")
    command(
        "tickingarea add circle 328 0 8 1 wc_unprotected true", "Added ticking area"
    )
    wait(lambda: inspect(0, 0).get("state") == 11 and inspect(20, 0).get("state") == 11)
    command("setblock 8 20 8 gold_block", "Block placed")
    command("setblock 8 20 8 diamond_block", "Block placed")
    opt("on")
    opt("run")
    wait(lambda: not inspect(0, 0)["resident"])
    check("reproduced automatic eviction of a ticking-area chunk", True)
    opt("protect first Overworld 0 0 0 0 2400")
    check(
        "protection lifts previous automatic deny immediately",
        not inspect(0, 0)["optimizer_denied"],
    )
    wait(lambda: inspect(0, 0).get("state") == 11)
    command(
        "execute if block 8 20 8 diamond_block run say WC_PROTECTION_SAVE_OK",
        "WC_PROTECTION_SAVE_OK",
    )
    check("restored chunk retains saved blocks", True)
    opt("protect second Overworld 0 0 0 0 2400")
    opt("unprotect first")
    start = opt("status")["native"]["cleanup_runs"]
    opt("run")
    wait(lambda: opt("status")["native"]["cleanup_runs"] > start)
    wait(lambda: opt("status")["native"]["pending_chunks"] == 0)
    check(
        "overlapping lease survives another job completing",
        inspect(0, 0).get("state") == 11,
    )
    check("protection includes generation neighbors", inspect(2, 0).get("state") == 11)
    check("unrelated ticking area still cleaned", not inspect(20, 0)["resident"])
    # Real Endstone TPS sampling, threshold 20 for a deterministic trigger.
    opt("set low_tps_seconds 1")
    opt("set tps_cooldown_seconds 1")
    opt("set tps_threshold 20")
    before = opt("status")["tps_triggers"]
    wait(lambda: opt("status")["tps_triggers"] > before)
    check(
        "TPS-triggered cleanup retains protected chunks",
        inspect(0, 0).get("state") == 11,
    )
    check("configuration changes preserve job leases", inspect(0, 0)["protected"])
    opt("set tps_threshold 0")
    opt("unprotect second")
    opt("run")
    wait(lambda: not inspect(0, 0)["resident"])
    check("completed jobs become eligible for cleanup", True)
    opt("protect expires Overworld 0 0 0 0 20")
    wait(lambda: not inspect(0, 0)["protected"])
    check("abandoned protection expires in server ticks", True)
    command("wc unload Overworld 0 0")
    command(
        "wco protect conflict Overworld 0 0 0 0 200",
        "Protection overlaps a manual unload rule",
    )
    check(
        "manual deny rejects protection without changing policy",
        inspect(0, 0)["denied"] and not inspect(0, 0)["protected"],
    )
    command("wc allow Overworld 0 0")
finally:
    opt("off")
    for lease in opt("status")["native"]["protections"]:
        opt("unprotect " + lease["id"])
    command("tickingarea remove wc_protect", "Removed")
    command("tickingarea remove wc_unprotected", "Removed")
    command("wc reset")
    command(f"wc radius {original_policy['simulation_radius_cap']}")
    for field, operation in (("denied", "unload"), ("pins", "load")):
        for entry in original_policy[field]:
            command(f"wc {operation} {entry['dimension']} {entry['x']} {entry['z']}")
    for key, value in original.items():
        if key not in {"schema", "enabled"}:
            opt(f"set {key} {json.dumps(value)}")
    opt("on" if original["enabled"] else "off")

(EVIDENCE / "protection-live.json").write_text(
    json.dumps({"passed": len(checks), "checks": checks}, indent=2) + "\n"
)
print(json.dumps({"passed": len(checks)}), flush=True)

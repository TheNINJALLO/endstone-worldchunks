"""Integration test: ONLY run against the disposable lab with one player at origin."""

import json
import math
import time
from lab_console import ROOT, EVIDENCE, command, check, checks, inspect


def opt(args="status"):
    return json.loads(command("wco " + args, "WORLDCHUNKS_OPTIMIZER "))


def wait_for(test, timeout=35):
    deadline = time.monotonic() + timeout
    while True:
        result = test()
        if result:
            return result
        assert time.monotonic() < deadline, "condition timed out"
        time.sleep(0.3)


players = command("wc players")
check(
    "one player at origin",
    len(players) == 1
    and players[0]["dimension"] == "Overworld"
    and math.floor(players[0]["x"] / 16) == 0
    and math.floor(players[0]["z"] / 16) == 0,
)
opt("off")
opt("set compatibility_mode false")
command("wc reset")
opt("set cleanup_interval_seconds 0")
opt("set tps_threshold 0")
opt("set keep_radius 2")
opt("set simulation_radius 1")
opt("set batch_size 8")
wait_for(lambda: inspect(3, 0).get("state") == 11)
command("setblock 56 20 8 gold_block", "Block placed")
command("setblock 56 20 8 emerald_block", "Block placed")
command("wc radius 2")
command("wc load Overworld 6 0")
wait_for(lambda: inspect(6, 0).get("state") == 11)
command("wc unload Overworld -2 0")
opt("on")
wait_for(lambda: not opt()["native"]["paused_for_players"])
opt("run")
wait_for(lambda: not inspect(3, 0)["resident"])
check(
    "outside radius evicted inside vanilla radius four", not inspect(3, 0)["resident"]
)
check(
    "center and radius boundary retained",
    inspect(0, 0)["resident"] and inspect(2, 0)["resident"],
)
check("manual pin outside radius preserved", inspect(6, 0).get("state") == 11)
check(
    "manual deny inside radius preserved",
    inspect(-2, 0)["denied"] and not inspect(-2, 0)["resident"],
)
time.sleep(2)
check("evicted chunk stays absent", not inspect(3, 0)["resident"])
check(
    "simulation radius one active", command("wc status")["effective_tick_offsets"] == 9
)
a = inspect(0, 0)
time.sleep(1)
b = inspect(0, 0)
check("retained center continues ticking", b["last_tick"] > a["last_tick"])
check(
    "temporary rules absent from saved policy", len(command("wc policy")["denied"]) == 1
)
opt("set keep_radius 1")
opt("run")
wait_for(lambda: not inspect(2, 0)["resident"])
check(
    "keep radius one unloads chunk two",
    inspect(1, 0)["resident"] and not inspect(2, 0)["resident"],
)
opt("set keep_radius 2")
opt("run")
wait_for(
    lambda: (
        inspect(2, 0).get("state") == 11 and not opt()["native"]["paused_for_players"]
    )
)
check("increasing radius restores its new boundary", inspect(2, 0)["resident"])

# Teleport enters a chunk that the optimizer previously removed. It must recover.
command("tp @a 72 -60 8", "Teleported")
wait_for(lambda: inspect(4, 0).get("state") == 11)
wait_for(lambda: not opt()["native"]["paused_for_players"])
opt("run")
wait_for(lambda: not inspect(0, 0)["resident"])
check("movement restores new occupied chunk", inspect(4, 0).get("state") == 11)
check("movement cleanup removes old player area", not inspect(0, 0)["resident"])
command(
    "execute if block 56 20 8 emerald_block run say WCO_SAVE_VERIFIED",
    "WCO_SAVE_VERIFIED",
)
check("cleanup preserves saved blocks", True)
command("wc allow Overworld -2 0")  # A manual deny can block a new area's generation.
command("tp @a -8 -60 -8", "Teleported")
wait_for(
    lambda: (
        inspect(-1, -1).get("state") == 11 and not opt()["native"]["paused_for_players"]
    )
)
opt("run")
wait_for(lambda: not inspect(2, 0)["resident"])
check(
    "negative coordinates use floor chunk division",
    inspect(-1, -1)["resident"]
    and inspect(-3, -1)["resident"]
    and not inspect(2, 0)["resident"],
)
command("tp @a 72 -60 8", "Teleported")
wait_for(
    lambda: (
        inspect(4, 0).get("state") == 11 and not opt()["native"]["paused_for_players"]
    )
)
command("wc unload Overworld -2 0")

# Disabling clears only the companion policy and restores the manual simulation cap.
opt("off")
wait_for(lambda: inspect(0, 0).get("state") == 11)
check(
    "disable releases temporary restrictions", opt()["native"]["temporary_denies"] == 0
)
check(
    "disable preserves manual deny and pin",
    inspect(-2, 0)["denied"] and inspect(6, 0)["resident"],
)
check(
    "disable restores manual radius", command("wc status")["effective_radius_cap"] == 2
)
command("wc allow Overworld -2 0")
command("wc release Overworld 6 0")

# Independent interval trigger, without TPS cleanup.
opt("set cleanup_interval_seconds 2")
opt("on")
before = opt()["interval_triggers"]
wait_for(lambda: opt()["interval_triggers"] > before)
wait_for(lambda: not inspect(0, 0)["resident"])
check("timer triggers real native eviction", True)

# Use inclusive 20 TPS threshold against real Endstone sampling; unit tests cover
# low-TPS dips, recovery, sustained duration and cooldown with a deterministic clock.
opt("set cleanup_interval_seconds 0")
opt("set low_tps_seconds 1")
opt("set tps_cooldown_seconds 3")
opt("set tps_threshold 20")
before = opt()["tps_triggers"]
wait_for(lambda: opt()["tps_triggers"] > before)
wait_for(lambda: not inspect(0, 0)["resident"])
status = opt()
check(
    "TPS-only trigger uses real server TPS",
    status["sampled_tps"] <= 20 and status["native"]["last_reason"] == "low_tps",
)

old = opt()["config"]
command("wco set keep_radius 0", "WorldChunksOptimizer:")
command("wco set simulation_radius 3", "WorldChunksOptimizer:")
command("wco set tps_threshold 21", "WorldChunksOptimizer:")
check("invalid settings rejected without mutation", opt()["config"] == old)
opt("reload")
check("config reload retains settings", opt()["config"] == old)

# Leave the lab at origin, with the documented defaults enabled.
opt("off")
command("tp @a 8 -60 8", "Teleported")
opt("set simulation_radius 2")
opt("set cleanup_interval_seconds 30")
opt("set tps_threshold 18")
opt("set low_tps_seconds 5")
opt("set tps_cooldown_seconds 30")
opt("set batch_size 32")
opt("set compatibility_mode true")
opt("on")
result = {"passed": len(checks), "checks": checks, "final_config": opt()["config"]}
(EVIDENCE / "optimizer-live.json").write_text(json.dumps(result, indent=2) + "\n")
print(json.dumps({"passed": len(checks)}), flush=True)

"""Destructive ONLY to the disposable flat lab world. Run inside its container.

Requires scripts/lab_server.py and one fully joined Minecraft player.
Verifies real BDS state, saved block contents, radius changes, and command errors.
"""

import json
import math
import time
from lab_console import ROOT, EVIDENCE, checks, command, check, inspect

players = command("wc players")
check(
    "one live player",
    len(players) == 1,
    [{k: v for k, v in p.items() if k != "name"} for p in players],
)
p = players[0]
check(
    "player at test origin",
    p["dimension"] == "Overworld"
    and math.floor(p["x"] / 16) == 0
    and math.floor(p["z"] / 16) == 0,
)
until = time.monotonic() + 120
while command("wc status")["spawn_pending"]:
    assert time.monotonic() < until, "Player never completed spawning"
    time.sleep(1)
time.sleep(6)
status = command("wc status")
check(
    "saved radius active after spawning",
    status["simulation_radius_cap"] == 2 and status["effective_radius_cap"] == 2,
    status,
)
command("wc reset")
time.sleep(1)
a = inspect(3, 0)
time.sleep(1)
b = inspect(3, 0)
check("vanilla radius ticks distant chunk", b["last_tick"] > a["last_tick"], [a, b])
for radius, near, far, count in [(2, 1, 3, 25), (1, 0, 2, 9)]:
    command(f"wc radius {radius}")
    time.sleep(1)
    a = [inspect(near, 0), inspect(far, 0)]
    time.sleep(2)
    b = [inspect(near, 0), inspect(far, 0)]
    check(
        f"radius {radius} near chunk ticks",
        b[0]["last_tick"] > a[0]["last_tick"],
        [a[0], b[0]],
    )
    check(
        f"radius {radius} far chunk stays loaded without ticking",
        b[1]["resident"] and b[1]["last_tick"] == a[1]["last_tick"],
        [a[1], b[1]],
    )
    check(
        f"radius {radius} offset count",
        command("wc players")[0]["effective_tick_offsets"] == count,
    )
command("wc radius 0")
time.sleep(1)
command("setblock 40 20 8 gold_block", "Block placed")
command("setblock 40 20 8 emerald_block", "Block placed")
before = command("wc status")
command("wc unload Overworld 2 0")
time.sleep(2)
check("player-near chunk physically absent", not inspect(2, 0)["resident"])
time.sleep(2)
check("player-near chunk stays absent", not inspect(2, 0)["resident"])
after = command("wc status")
check(
    "native reload attempts blocked",
    after["blocked_requests"] > before["blocked_requests"],
    [before, after],
)
check("player remains connected", len(command("wc players")) == 1)
command("wc allow Overworld 2 0")
until = time.monotonic() + 20
while inspect(2, 0).get("state") != 11:
    assert time.monotonic() < until, "Chunk did not finish loading"
    time.sleep(0.2)
command(
    "execute if block 40 20 8 emerald_block run say WC_SAVE_VERIFIED",
    "WC_SAVE_VERIFIED",
)
check("saved block survives unload and reload", True)
command("wc unload Overworld 0 0")
time.sleep(2)
check("occupied chunk physically absent", not inspect(0, 0)["resident"])
check("occupied unload retains player connection", len(command("wc players")) == 1)
command("wc allow Overworld 0 0")
time.sleep(2)
check("occupied chunk restored", inspect(0, 0).get("state") == 11)
command("wc load Overworld 100 100")
until = time.monotonic() + 30
while inspect(100, 100).get("state") != 11:
    assert time.monotonic() < until, "Pinned chunk did not load"
    time.sleep(0.2)
check("remote chunk can be pinned", inspect(100, 100)["resident"])
command("wc release Overworld 100 100")
until = time.monotonic() + 30
while inspect(100, 100)["resident"]:
    assert time.monotonic() < until, "Chunk remained retained after pin release"
    time.sleep(0.5)
check("remote pin release permits unload", not inspect(100, 100)["resident"])
command("wc inspect Invalid 0 0", "Unknown dimension")
command("wc radius -1", "Radius must be")
check(
    "invalid commands rejected without server failure", command("wc status")["native"]
)
command("wc reset")
result = {"passed": len(checks), "checks": checks}
(EVIDENCE / "live-regression.json").write_text(json.dumps(result, indent=2) + "\n")
print(json.dumps({"passed": len(checks)}), flush=True)

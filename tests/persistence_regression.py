"""Two phases separated by a clean lab restart. Uses only the disposable world."""

import json
import sys
import time
from lab_console import ROOT, EVIDENCE, checks, command, check


def loaded(x, z):
    deadline = time.monotonic() + 60
    while True:
        result = command(f"wc inspect Overworld {x} {z}")
        if result.get("state") == 11:
            return result
        assert time.monotonic() < deadline, result
        time.sleep(0.25)


phase = sys.argv[1]
if phase == "prepare":
    command("wc reset")
    command("wc radius 2")
    command("wc unload Overworld 5 5")
    command("wc load Overworld 500 100")
    check("fresh distant chunk finishes generation", loaded(500, 100)["resident"])
    time.sleep(0.5)
    check(
        "temporary generation neighbors released",
        command("wc status")["pending_generation"] == 0,
    )
    check(
        "remote generation neighbor absent",
        not command("wc inspect Overworld 506 106")["resident"],
    )
    (EVIDENCE / "persistence-prepare.json").write_text(
        json.dumps(checks, indent=2) + "\n"
    )
elif phase == "verify":
    policy = command("wc policy")
    check("requested radius restored", policy["simulation_radius_cap"] == 2)
    check(
        "deny rule restored",
        policy["denied"] == [{"dimension": "Overworld", "x": 5, "z": 5}],
    )
    check(
        "pin restored",
        policy["pins"] == [{"dimension": "Overworld", "x": 500, "z": 100}],
    )
    check("restored pin fully loads", loaded(500, 100)["resident"])
    command("wc load Overworld 5 5", "Allow this chunk before loading it")
    check(
        "restored deny rejects pin", not command("wc inspect Overworld 5 5")["resident"]
    )
    command("wc release Overworld 500 100")
    time.sleep(2)
    check(
        "restored pin releases cleanly",
        not command("wc inspect Overworld 500 100")["resident"],
    )
    command("wc reset")
    command("wc radius 2")
    initial = json.loads((EVIDENCE / "persistence-prepare.json").read_text())
    result = {"passed": len(initial) + len(checks), "checks": initial + checks}
    (EVIDENCE / "persistence-regression.json").write_text(
        json.dumps(result, indent=2) + "\n"
    )
    print(json.dumps({"passed": result["passed"]}), flush=True)
else:
    raise SystemExit("Use prepare or verify")

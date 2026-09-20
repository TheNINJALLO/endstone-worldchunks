"""No-player cleanup test in the disposable lab; creates/removes one ticking area."""

import json
import time
from lab_console import ROOT, EVIDENCE, command, check, checks, inspect


def opt(args):
    return json.loads(command("wco " + args, "WORLDCHUNKS_OPTIMIZER "))


def wait(test):
    deadline = time.monotonic() + 35
    while not test():
        assert time.monotonic() < deadline, "condition timed out"
        time.sleep(0.3)


check("no players online", not command("wc players"))
opt("off")
original_compatibility = opt("status")["config"]["compatibility_mode"]
opt("set compatibility_mode false")
command("wc reset")
command("tickingarea add circle 8 0 8 2 wco_empty true", "Added ticking area")
wait(lambda: inspect(0, 0).get("state") == 11 and inspect(2, 0).get("state") == 11)
command("wc load Overworld 0 0")
opt("on")
opt("run")
wait(lambda: not inspect(2, 0)["resident"])
check(
    "no-player cleanup evicts explicit ticking-area chunk",
    not inspect(2, 0)["resident"],
)
check("no-player cleanup keeps manual pin", inspect(0, 0).get("state") == 11)
opt("off")
wait(lambda: inspect(2, 0).get("state") == 11)
check("disable restores explicit ticking-area chunk", True)
command("wc release Overworld 0 0")
command("tickingarea remove wco_empty", "Removed")
command("wc radius 2")
opt("set compatibility_mode " + json.dumps(original_compatibility))
opt("on")
(EVIDENCE / "optimizer-empty.json").write_text(
    json.dumps({"passed": len(checks), "checks": checks}, indent=2) + "\n"
)
print(json.dumps({"passed": len(checks)}), flush=True)

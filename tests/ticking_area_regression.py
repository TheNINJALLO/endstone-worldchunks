"""Requires the disposable wc_final ticking area centered at block 8,0,8."""

import json
import time
from lab_console import ROOT, EVIDENCE, checks, command, check, inspect

check("no connected players", not command("wc players"))
check("explicit ticking area loads center", inspect(0, 0).get("state") == 11)
command("setblock 8 20 8 gold_block", "Block placed")
command("setblock 8 20 8 diamond_block", "Block placed")
before = command("wc status")
command("wc unload Overworld 0 0")
time.sleep(3)
check("explicit ticking area center physically absent", not inspect(0, 0)["resident"])
time.sleep(3)
check(
    "explicit ticking area cannot reload denied center", not inspect(0, 0)["resident"]
)
check(
    "explicit ticking area reload attempts blocked",
    command("wc status")["blocked_requests"] > before["blocked_requests"],
)
command("wc allow Overworld 0 0")
deadline = time.monotonic() + 30
while inspect(0, 0).get("state") != 11:
    assert time.monotonic() < deadline, "Restoration did not finish"
    time.sleep(0.2)
command(
    "execute if block 8 20 8 diamond_block run say WC_AREA_SAVE_OK", "WC_AREA_SAVE_OK"
)
check("explicit ticking area restore preserves saved block", True)
command("tickingarea remove wc_final", "Removed ticking area(s)")
result = {"passed": len(checks), "checks": checks}
(EVIDENCE / "ticking-area-regression.json").write_text(
    json.dumps(result, indent=2) + "\n"
)
print(json.dumps({"passed": len(checks)}), flush=True)

"""Check operator-friendly output and the scripting format in the live lab."""

import json
from lab_console import EVIDENCE, command, check, checks

command("wc status", "Native hooks: active", json_output=False)
check("native status has a readable summary", True)
command("wco status", "Current TPS:", json_output=False)
check("optimizer status has a readable summary", True)
check("native JSON format remains available", command("wc status")["native"])
check(
    "optimizer JSON format remains available",
    "config" in json.loads(command("wco status", "WORLDCHUNKS_OPTIMIZER ")),
)
(EVIDENCE / "command-output.json").write_text(
    json.dumps({"passed": len(checks), "checks": checks}, indent=2) + "\n"
)

"""Verify saved optimizer settings after a clean lab restart, before players join."""

import json
from lab_console import ROOT, EVIDENCE, PLATFORM, command, check, checks

saved = json.loads(
    (
        ROOT
        / "lab"
        / ("windows-server" if PLATFORM == "windows" else "server")
        / "plugins/worldchunks_optimizer/config.json"
    ).read_text()
)
status = json.loads(command("wco status", "WORLDCHUNKS_OPTIMIZER "))
check("saved optimizer configuration restored after restart", status["config"] == saved)
check(
    "saved optimizer enabled state applied",
    status["native"]["enabled"] == saved["enabled"],
)
check(
    "saved radii and batch applied",
    all(
        status["native"][key] == saved[key]
        for key in ["keep_radius", "simulation_radius", "batch_size"]
    ),
)
check(
    "automatic unload history not persisted", status["native"]["temporary_denies"] == 0
)
(EVIDENCE / "optimizer-restart.json").write_text(
    json.dumps(
        {"passed": len(checks), "checks": checks, "restored_config": saved}, indent=2
    )
    + "\n"
)
print(json.dumps({"passed": len(checks)}), flush=True)

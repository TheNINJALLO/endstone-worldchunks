"""Record aggressive-mode residency with one player, then restore compatibility mode."""

import hashlib
import json
import time
from lab_console import ROOT, EVIDENCE, PLATFORM, command


def opt(args):
    return json.loads(command("wco " + args, "WORLDCHUNKS_OPTIMIZER "))


opt("off")
opt("set compatibility_mode false")
command("wc reset")
command("wc radius 2")
opt("on")
end = time.monotonic() + 40
while opt("status")["native"]["paused_for_players"]:
    assert time.monotonic() < end, "Player grace did not finish"
    time.sleep(0.3)
opt("run")
while True:
    status = command("wc status")
    if sum(d["loaded"] for d in status["dimensions"]) == 25:
        break
    assert time.monotonic() < end, status
    time.sleep(0.3)
build = ROOT / ("build-windows" if PLATFORM == "windows" else "build")
extension = ".dll" if PLATFORM == "windows" else ".so"
hashes = {
    p.name: hashlib.sha256(p.read_bytes()).hexdigest()
    for p in build.glob("endstone_worldchunks*" + extension)
}
result = {
    "version": "0.3.1",
    "one_player_loaded_chunks": 25,
    "native": status,
    "optimizer": opt("status"),
    "binary_sha256": hashes,
}
(EVIDENCE / "final-state.json").write_text(json.dumps(result, indent=2) + "\n")
print("PASS final residency: 25 chunks around one player")
opt("set compatibility_mode true")

"""Reject a modified executable in the stopped, disposable Windows lab."""

from pathlib import Path
import hashlib
import json
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
LAB = ROOT / "lab"
EXE = LAB / "windows-server/bedrock_server.exe"
EXPECTED = "76d547f82e02c18d0986c30b47132c9cc4171d0f2df1c00649e50ff35788b321"
assert sys.platform == "win32"
assert hashlib.sha256(EXE.read_bytes()).hexdigest() == EXPECTED
assert "Quit correctly" in (LAB / "windows-server.log").read_text(), (
    "Stop the Windows lab first"
)
size = EXE.stat().st_size
process = None
try:
    with EXE.open("ab") as out:
        out.write(b"WorldChunks fingerprint test\n")
    with (LAB / "fingerprint-driver.log").open("w") as out:
        process = subprocess.Popen(
            [sys.executable, str(ROOT / "scripts/lab_server.py"), "--wheel"],
            cwd=ROOT,
            stdout=out,
            stderr=subprocess.STDOUT,
            creationflags=subprocess.CREATE_NO_WINDOW,
        )
        end = time.monotonic() + 90
        while True:
            log = (LAB / "windows-server.log").read_text(errors="replace")
            if (
                "Unsupported BDS binary; expected Windows 1.26.51.1" in log
                and "Server started." in log
            ):
                break
            assert process.poll() is None, "Lab stopped before checking the fingerprint"
            assert time.monotonic() < end, "Fingerprint check timed out"
            time.sleep(0.5)
        with (LAB / "windows-commands.txt").open("a") as commands:
            commands.write("wc status --json\nstop\n")
        assert process.wait(timeout=45) == 0
        log = (LAB / "windows-server.log").read_text(errors="replace")
        assert "Native adapter is unavailable; see startup log" in log
        assert "Quit correctly" in log
finally:
    if process is not None and process.poll() is None:
        with (LAB / "windows-commands.txt").open("a") as commands:
            commands.write("stop\n")
        process.wait(timeout=45)
    with EXE.open("r+b") as out:
        out.truncate(size)
    assert hashlib.sha256(EXE.read_bytes()).hexdigest() == EXPECTED
result = {
    "passed": 3,
    "modified_executable_rejected": True,
    "commands_refused_native_access": True,
    "shutdown_clean": True,
    "original_executable_restored": True,
}
(ROOT / "evidence/windows/fingerprint.json").write_text(
    json.dumps(result, indent=2) + "\n"
)
print("PASS Windows fingerprint rejection, unavailable commands, and clean shutdown")

from pathlib import Path
import os
import json
import time

ROOT = Path(__file__).resolve().parents[1]
PLATFORM = os.environ.get("WORLDCHUNKS_LAB_PLATFORM", "linux")
LAB = ROOT / "lab"
LOG = LAB / ("windows-server.log" if PLATFORM == "windows" else "server.log")
COMMANDS = LAB / ("windows-commands.txt" if PLATFORM == "windows" else "commands.txt")
EVIDENCE = ROOT / "evidence" / ("windows" if PLATFORM == "windows" else "release-0.3.0")
EVIDENCE.mkdir(parents=True, exist_ok=True)
checks = []


def command(value, marker="WORLDCHUNKS ", *, json_output=True):
    if (
        json_output
        and value.split()[0] in ("wc", "wco")
        and "--json" not in value.split()
    ):
        value += " --json"
    offset = LOG.stat().st_size
    with COMMANDS.open("a") as out:
        out.write(value + "\n")
    until = time.monotonic() + 20
    while time.monotonic() < until:
        with LOG.open() as inp:
            inp.seek(offset)
            text = inp.read()
        for line in text.splitlines():
            if marker in line:
                result = line.split(marker, 1)[1]
                return json.loads(result) if marker == "WORLDCHUNKS " else result
            if "WorldChunks:" in line or "[ERROR]" in line or " ERROR]:" in line:
                raise AssertionError(f"{value}: {line}")
        time.sleep(0.1)
    raise TimeoutError(value)


def check(name, condition, detail=None):
    assert condition, (name, detail)
    checks.append({"test": name, "passed": True, "detail": detail})
    print("PASS", name, flush=True)


def inspect(x, z):
    return command(f"wc inspect Overworld {x} {z}")

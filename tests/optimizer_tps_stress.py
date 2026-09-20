"""Host-side test: temporarily CPU-throttle ONLY the disposable worldchunks-server.

Uses actual Endstone TPS, no fabricated TPS values or production test commands.
A separate watchdog restores the container CPU limit even if command polling fails.
"""

from pathlib import Path
import json
import re
import subprocess
import threading
import time

ROOT = Path(__file__).resolve().parents[1]
CONTAINER = "worldchunks-server"
REMOTE = """import sys,json
sys.path.insert(0,'/workspace/endstone-worldchunks/tests')
from lab_console import command
print(command('wco '+sys.argv[1],'WORLDCHUNKS_OPTIMIZER '))
"""


def docker(*args):
    return subprocess.run(
        ["docker", *args], check=True, capture_output=True, text=True, timeout=40
    ).stdout


def opt(args):
    return json.loads(
        docker("exec", CONTAINER, "/opt/endstone-python/bin/python", "-c", REMOTE, args)
    )


container = json.loads(docker("inspect", CONTAINER))[0]
assert any(
    m["Destination"] == "/opt/worldchunks-server"
    and "endstone-worldchunks" in m["Source"]
    for m in container["Mounts"]
), "Not the disposable lab"
assert container["HostConfig"]["NanoCpus"] == 0, (
    "Use the normal lab container without a NanoCPUs limit"
)
assert docker("exec", CONTAINER, "cat", "/sys/fs/cgroup/cpu.max").startswith("max "), (
    "Lab must start with an unlimited CPU quota"
)
original = opt("status")["config"]
restored = threading.Event()


def restore():
    # Docker update --cpus 0 leaves an existing NanoCPUs limit unchanged.
    # An explicit unlimited quota is required; verify the kernel's actual limit.
    docker("update", "--cpu-quota", "-1", CONTAINER)
    assert docker("exec", CONTAINER, "cat", "/sys/fs/cgroup/cpu.max").startswith("max ")
    restored.set()


watchdog = threading.Timer(25, restore)
try:
    opt("off")
    opt("set compatibility_mode false")
    opt("set cleanup_interval_seconds 0")
    opt("set tps_threshold 18")
    opt("set low_tps_seconds 1")
    opt("set tps_cooldown_seconds 30")
    opt("on")
    time.sleep(6)
    before = opt("status")["tps_triggers"]
    offset = (ROOT / "lab/server.log").stat().st_size
    watchdog.start()
    docker("update", "--cpu-period", "100000", "--cpu-quota", "2000", CONTAINER)
    found = None
    deadline = time.monotonic() + 22
    while time.monotonic() < deadline:
        with (ROOT / "lab/server.log").open(errors="replace") as inp:
            inp.seek(offset)
            match = re.search(r"Cleanup requested: low_tps, TPS ([0-9.]+)", inp.read())
        if match:
            found = float(match[1])
            break
        time.sleep(0.5)
    restore()
    watchdog.cancel()
    assert found is not None and 0 < found <= 18, (
        "Real sustained low-TPS cleanup did not trigger"
    )
    after = opt("status")
    assert after["tps_triggers"] > before, "TPS trigger counter did not advance"
    result = {
        "passed": 2,
        "real_tps_at_cleanup": found,
        "threshold": 18,
        "sustained_seconds": 1,
        "temporary_container_cpus": 0.02,
        "cpu_limit_restored": restored.is_set(),
    }
    (ROOT / "evidence/release-0.3.1/optimizer-tps-stress.json").write_text(
        json.dumps(result, indent=2) + "\n"
    )
    print(json.dumps(result), flush=True)
finally:
    restore()
    watchdog.cancel()
    opt("off")
    opt("set compatibility_mode " + json.dumps(original["compatibility_mode"]))
    for key in [
        "cleanup_interval_seconds",
        "tps_threshold",
        "low_tps_seconds",
        "tps_cooldown_seconds",
    ]:
        opt("set " + key + " " + str(original[key]))
    if original["enabled"]:
        opt("on")

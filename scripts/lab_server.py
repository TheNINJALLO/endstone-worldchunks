"""Run only the disposable WorldChunks server and consume lab/commands.txt."""

from pathlib import Path
import os
import shutil
import subprocess
import sys
import time
import zipfile

windows = sys.platform == "win32"
wheel = "--wheel" in sys.argv
root = Path(__file__).resolve().parents[1]
lab = root / "lab"
lab.mkdir(exist_ok=True)
server = lab / "windows-server" if windows else Path("/opt/worldchunks-server")
server.mkdir(exist_ok=True)
executable = "bedrock_server.exe" if windows else "bedrock_server"
platform = "windows" if windows else "linux"
if not (server / executable).exists():
    with zipfile.ZipFile(root / f"bedrock-server-{platform}-1.26.51.1.zip") as archive:
        archive.extractall(server)
if not windows:
    (server / executable).chmod(0o755)
(server / "version.txt").write_text("26.51")
plugins = server / "plugins"
plugins.mkdir(exist_ok=True)
extension = ".dll" if windows else ".so"
build = root / ("build-windows" if windows else "build")
backup = lab / f"{platform}-install-backup"
backup.mkdir(exist_ok=True)
for path in list(plugins.glob("endstone_worldchunks*" + extension)) + list(
    plugins.glob("endstone_worldchunks_bundle-*.whl")
):
    shutil.copyfile(path, backup / path.name)
    path.unlink()
if wheel:
    tag = "win_amd64" if windows else "linux_x86_64"
    path = root / "dist" / f"endstone_worldchunks_bundle-0.3.0-py3-none-{tag}.whl"
    shutil.copyfile(path, plugins / path.name)
else:
    for name in ["worldchunks", "worldchunks_optimizer"]:
        filename = f"endstone_{name}{extension}"
        shutil.copyfile(build / filename, plugins / filename)
properties = dict(
    line.split("=", 1)
    for line in (server / "server.properties").read_text().splitlines()
    if "=" in line and not line.startswith("#")
)
properties.update(
    {
        "server-name": "WorldChunks isolated test",
        "server-port": "19142",
        "server-portv6": "19143",
        "enable-lan-visibility": "false",
        "online-mode": "false",
        "allow-list": "false",
        "allow-cheats": "true",
        "gamemode": "creative",
        "level-name": "worldchunks-test",
        "level-seed": "42",
        "view-distance": "8",
        "tick-distance": "4",
        "default-player-permission-level": "operator",
        "max-players": "4",
        "level-type": "FLAT",
        "transport": "nethernet",
        "server-udp-ports": "127.0.0.1:19144:19144",
    }
)
if windows:
    properties.update(
        {"server-port": "19152", "server-portv6": "19153", "server-udp-ports": "19154"}
    )
(server / "server.properties").write_text(
    "".join(f"{k}={v}\n" for k, v in properties.items())
)
commands = lab / ("windows-commands.txt" if windows else "commands.txt")
commands.write_text("")
log = lab / ("windows-server.log" if windows else "server.log")
env = os.environ.copy()
env["PATH"] = str(Path(sys.executable).parent) + os.pathsep + env["PATH"]
if windows:
    env["PYTHONPATH"] = sys.base_prefix
with log.open("w") as out, commands.open() as inp:
    process = subprocess.Popen(
        [sys.executable, "-m", "endstone", "--server-folder", str(server), "--yes"],
        cwd=server,
        env=env,
        stdin=subprocess.PIPE,
        stdout=out,
        stderr=subprocess.STDOUT,
        text=True,
    )
    (lab / ("windows-pid.txt" if windows else "pid.txt")).write_text(str(process.pid))
    print(
        "Lab server starting; commands: lab/commands.txt; log: lab/server.log",
        flush=True,
    )
    identity_saved = (server / "keys/server_identity_key.pem").exists()
    try:
        while process.poll() is None:
            if not identity_saved and "Server started." in log.read_text(
                errors="replace"
            ):
                process.stdin.write("serveridentity save\n")
                process.stdin.flush()
                identity_saved = True
            line = inp.readline()
            if line:
                print("COMMAND", line.strip(), flush=True)
                process.stdin.write(line)
                process.stdin.flush()
            else:
                time.sleep(0.2)
        print("SERVER EXIT", process.returncode, flush=True)
        sys.exit(process.returncode)
    finally:
        if process.poll() is None:
            process.stdin.write("stop\n")
            process.stdin.flush()
            try:
                process.wait(timeout=30)
            except subprocess.TimeoutExpired:
                process.terminate()

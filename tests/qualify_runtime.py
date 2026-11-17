"""Qualify native and bundled plugins in a new disposable, headless server."""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
WINDOWS = os.name == "nt"
PLATFORM = "windows" if WINDOWS else "linux"
ARCHIVE_SHA256 = {
    "linux": "4443e58064f5fa96f3464ab46c20cd89b631f7117e4c94d327f48c4eb64f9969",
    "windows": "c98b9824783b000b8a8d602e6f58467725316c8f635739b1e31998871995621e",
}


class Server:
    def __init__(self, path: Path):
        self.path = path
        self.log = path / "server.log"
        self.process = None
        self.output = None

    def text(self):
        return self.log.read_text(encoding="utf-8", errors="replace") if self.log.exists() else ""

    def start(self):
        self.output = self.log.open("w", encoding="utf-8")
        options = {"creationflags": subprocess.CREATE_NO_WINDOW} if WINDOWS else {}
        self.process = subprocess.Popen(
            [sys.executable, "-m", "endstone", "--server-folder", str(self.path), "--yes"],
            cwd=self.path, stdin=subprocess.PIPE, stdout=self.output, stderr=subprocess.STDOUT,
            text=True, encoding="utf-8", **options,
        )
        self.wait(lambda: "Server started." in self.text(), timeout=180)
        assert "chunk hooks installed" in self.text(), self.text()[-5000:]
        self.command("wc status --json")

    def wait(self, predicate, timeout=40):
        deadline = time.monotonic() + timeout
        while not predicate():
            if self.process.poll() is not None:
                raise RuntimeError(f"Server exited with {self.process.returncode}:\n{self.text()[-5000:]}")
            if time.monotonic() > deadline:
                raise TimeoutError(self.text()[-5000:])
            time.sleep(0.2)

    def command(self, command, marker="WORLDCHUNKS "):
        offset = len(self.text())
        self.process.stdin.write(command + "\n")
        self.process.stdin.flush()
        result = []
        def complete():
            for line in self.text()[offset:].splitlines():
                if marker in line:
                    result.append(line.split(marker, 1)[1])
                    return True
                if "WorldChunks:" in line or " ERROR]:" in line:
                    raise AssertionError(f"{command}: {line}")
            return False
        self.wait(complete, timeout=30)
        return json.loads(result[0]) if marker in ("WORLDCHUNKS ", "WORLDCHUNKS_OPTIMIZER ") else result[0]

    def stop(self):
        if self.process and self.process.poll() is None:
            self.process.stdin.write("stop\n")
            self.process.stdin.flush()
            self.process.wait(timeout=45)
        if self.output:
            self.output.close()
        assert self.process.returncode == 0, self.text()[-5000:]
        assert "Native adapter unavailable" not in self.text()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--wheel", type=Path, required=True)
    parser.add_argument("--server", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    server_path = args.server.resolve()
    if server_path.exists() and any(server_path.iterdir()):
        raise ValueError("Qualification requires a new, empty disposable server directory")
    server_path.mkdir(parents=True, exist_ok=True)
    archive = server_path / "bds.zip"
    suffix = "win" if WINDOWS else "linux"
    request = urllib.request.Request(
        f"https://www.minecraft.net/bedrockdedicatedserver/bin-{suffix}/bedrock-server-1.26.65.1.zip",
        headers={"User-Agent": "Mozilla/5.0 Endstone-WorldChunks-Qualification"},
    )
    with urllib.request.urlopen(request, timeout=60) as response, archive.open("wb") as output:
        shutil.copyfileobj(response, output)
    assert hashlib.sha256(archive.read_bytes()).hexdigest() == ARCHIVE_SHA256[PLATFORM]
    with zipfile.ZipFile(archive) as source:
        source.extractall(server_path)
    executable = server_path / ("bedrock_server.exe" if WINDOWS else "bedrock_server")
    executable.chmod(0o755)
    endstone_spec = importlib.util.find_spec("endstone")
    runtime = Path(endstone_spec.origin).parent / ("endstone_runtime.dll" if WINDOWS else "libendstone_runtime.so")
    subprocess.run([sys.executable, str(ROOT / "scripts/verify_native_binary.py"),
                    "--profile", str(ROOT / f"compatibility/native-binary-profile-0.11.16-{PLATFORM}-x64.json"),
                    "--bds", str(executable), "--endstone-runtime", str(runtime)], check=True)
    (server_path / "version.txt").write_text("26.65", encoding="utf-8")
    properties_path = server_path / "server.properties"
    properties = dict(line.split("=", 1) for line in properties_path.read_text().splitlines() if "=" in line and not line.startswith("#"))
    properties.update({"server-name": "WorldChunks disposable qualification", "enable-script": "true",
                       "online-mode": "false", "allow-cheats": "true", "enable-lan-visibility": "false",
                       "level-name": "worldchunks-qualification", "level-seed": "42", "level-type": "FLAT",
                       "view-distance": "8", "tick-distance": "4", "max-players": "1",
                       "transport": "nethernet", "server-port": "19162", "server-portv6": "19163",
                       "server-udp-ports": "127.0.0.1:19164:19164"})
    properties_path.write_text("".join(f"{key}={value}\n" for key, value in properties.items()), encoding="utf-8")
    plugins = server_path / "plugins"
    plugins.mkdir(exist_ok=True)
    extension = ".dll" if WINDOWS else ".so"
    binaries = [args.build_dir.resolve() / f"endstone_{name}{extension}" for name in ("worldchunks", "worldchunks_optimizer")]
    for path in binaries:
        shutil.copyfile(path, plugins / path.name)
    checks = []
    def check(name, condition):
        assert condition, name
        checks.append({"test": name, "passed": True})
        print("PASS", name, flush=True)
    server = Server(server_path)
    try:
        server.start()
        check("exact native hooks installed", True)
        check("headless fixture has no players", server.command("wc players --json") == [])
        def opt(arguments):
            return server.command("wco " + arguments + " --json", "WORLDCHUNKS_OPTIMIZER ")
        def inspect(x):
            return server.command(f"wc inspect Overworld {x} 0 --json")
        opt("off")
        opt("set compatibility_mode false")
        server.command("wc reset --json")
        server.command("tickingarea add circle 8 0 8 2 api_qualification true", "Added ticking area")
        server.wait(lambda: inspect(0).get("state") == 11 and inspect(2).get("state") == 11)
        server.command("setblock 40 20 8 emerald_block", "Block placed")
        server.command("wc load Overworld 0 0 --json")
        opt("on")
        opt("run")
        server.wait(lambda: not inspect(2)["resident"])
        check("cleanup evicts unprotected ticking-area chunk", not inspect(2)["resident"])
        check("cleanup preserves manual pin", inspect(0).get("state") == 11)
        opt("off")
        server.wait(lambda: inspect(2).get("state") == 11)
        check("disabling cleanup restores vanilla residency", True)
        server.command("execute if block 40 20 8 emerald_block run say SAVED_BLOCK_VERIFIED", "SAVED_BLOCK_VERIFIED")
        check("saved block survives native eviction and reload", True)
        server.command("wc release Overworld 0 0 --json")
        server.command("tickingarea remove api_qualification", "Removed")
        server.command("wc radius 2 --json")
        opt("set compatibility_mode true")
        server.stop()
        check("native plugins shut down cleanly", True)
        shutil.copyfile(server.log, server_path / "native-server.log")
        for path in binaries:
            (plugins / path.name).unlink()
        shutil.copyfile(args.wheel.resolve(), plugins / args.wheel.name)
        server.start()
        check("matching bundle wheel loads both plugins", True)
        check("chunk policy survives restart", server.command("wc status --json")["simulation_radius_cap"] == 2)
        server.command("wc load Overworld 2 0 --json")
        server.wait(lambda: inspect(2).get("state") == 11)
        server.command("execute if block 40 20 8 emerald_block run say RESTART_BLOCK_VERIFIED", "RESTART_BLOCK_VERIFIED")
        check("saved block survives full server restart", True)
        server.command("wc release Overworld 2 0 --json")
        server.stop()
        check("bundle plugins shut down cleanly", True)
    finally:
        if server.process and server.process.poll() is None:
            try:
                server.stop()
            except Exception:
                server.process.kill()
                server.process.wait(timeout=10)
    result = {"platform": PLATFORM, "endstone": "0.11.16", "bds": "1.26.65.1", "enable_script": True,
              "passed": True, "connected_client_tested": False, "checks": checks,
              "artifacts": {path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in [*binaries, args.wheel]}}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()

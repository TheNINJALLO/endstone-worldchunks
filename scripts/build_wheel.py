"""Build a platform wheel containing both prebuilt native plugins."""

from pathlib import Path
import argparse
import base64
import csv
import hashlib
import io
import zipfile

ROOT = Path(__file__).resolve().parents[1]
VERSION = "0.3.0"


def build(platform):
    windows = platform == "win_amd64"
    extension = ".dll" if windows else ".so"
    build_dir = ROOT / ("build-windows" if windows else "build")
    package = "endstone_worldchunks_bundle"
    info = f"{package}-{VERSION}.dist-info"
    tag = f"py3-none-{platform}"
    files = {
        f"{package}/__init__.py": (
            ROOT / "python" / package / "__init__.py"
        ).read_bytes(),
        f"{info}/METADATA": (
            "Metadata-Version: 2.1\n"
            "Name: endstone-worldchunks-bundle\n"
            f"Version: {VERSION}\n"
            "Summary: Native chunk control and automatic cleanup for Endstone\n"
            "License: MIT\n"
            "Requires-Python: >=3.14,<3.15\n"
            "Requires-Dist: endstone==0.11.11\n"
            "Project-URL: Source, https://github.com/TheNINJALLO/endstone-worldchunks\n\n"
        ).encode(),
        f"{info}/WHEEL": (
            "Wheel-Version: 1.0\nGenerator: worldchunks-build\n"
            f"Root-Is-Purelib: false\nTag: {tag}\n"
        ).encode(),
        f"{info}/entry_points.txt": (
            "[endstone]\nworldchunks_bundle = endstone_worldchunks_bundle:WorldChunksBundle\n"
        ).encode(),
    }
    for name in ("worldchunks", "worldchunks_optimizer"):
        binary = f"endstone_{name}{extension}"
        files[f"{package}/native/{binary}"] = (build_dir / binary).read_bytes()
    for path in [
        ROOT / "LICENSE",
        ROOT / "THIRD_PARTY_NOTICES.md",
        *(ROOT / "licenses").rglob("*"),
    ]:
        if path.is_file():
            files[f"{info}/licenses/{path.relative_to(ROOT).as_posix()}"] = (
                path.read_bytes()
            )
    records = io.StringIO(newline="")
    writer = csv.writer(records)
    for name, data in sorted(files.items()):
        digest = (
            base64.urlsafe_b64encode(hashlib.sha256(data).digest())
            .rstrip(b"=")
            .decode()
        )
        writer.writerow([name, f"sha256={digest}", len(data)])
    writer.writerow([f"{info}/RECORD", "", ""])
    files[f"{info}/RECORD"] = records.getvalue().encode()
    destination = ROOT / "dist" / f"{package}-{VERSION}-{tag}.whl"
    destination.parent.mkdir(exist_ok=True)
    with zipfile.ZipFile(destination, "w", zipfile.ZIP_DEFLATED) as archive:
        for name, data in sorted(files.items()):
            entry = zipfile.ZipInfo(name, (2026, 9, 19, 0, 0, 0))
            entry.compress_type = zipfile.ZIP_DEFLATED
            entry.external_attr = 0o644 << 16
            archive.writestr(entry, data)
    print(destination)
    return destination


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("platform", choices=["linux_x86_64", "win_amd64"])
    build(parser.parse_args().platform)

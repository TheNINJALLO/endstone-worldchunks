"""Package verified native builds, platform wheels, documentation, and licenses."""

from pathlib import Path
import hashlib
import shutil
import zipfile

ROOT = Path(__file__).resolve().parents[1]
VERSION = "0.3.0"
DIST = ROOT / "dist"
DIST.mkdir(exist_ok=True)
artifacts = []
for platform, build, extension, tag in [
    ("linux-x86_64", "build", ".so", "linux_x86_64"),
    ("windows-x86_64", "build-windows", ".dll", "win_amd64"),
]:
    binaries = []
    for name in ("worldchunks", "worldchunks_optimizer"):
        filename = f"endstone_{name}{extension}"
        target = DIST / filename
        shutil.copyfile(ROOT / build / filename, target)
        artifacts.append(target)
        binaries.append(target)
    wheel = DIST / f"endstone_worldchunks_bundle-{VERSION}-py3-none-{tag}.whl"
    if not wheel.is_file():
        raise FileNotFoundError(f"Build {wheel.name} with scripts/build_wheel.py first")
    artifacts.append(wheel)
    bundle = DIST / f"worldchunks-{VERSION}-{platform}.zip"
    files = [
        ROOT / name
        for name in ("README.md", "LICENSE", "THIRD_PARTY_NOTICES.md", "CHANGELOG.md")
    ]
    for directory in ("docs", "compatibility", "licenses", "evidence"):
        files.extend(
            path
            for path in (ROOT / directory).rglob("*")
            if path.is_file() and (directory != "evidence" or path.suffix == ".json")
        )
    with zipfile.ZipFile(bundle, "w", zipfile.ZIP_DEFLATED) as archive:
        for binary in binaries:
            archive.write(binary, f"plugins/{binary.name}")
        for path in sorted(files):
            archive.write(path, path.relative_to(ROOT).as_posix())
    artifacts.append(bundle)
hashes = [
    hashlib.sha256(path.read_bytes()).hexdigest() + "  " + path.name
    for path in sorted(artifacts)
]
(DIST / "SHA256SUMS").write_text("\n".join(hashes) + "\n")
print("\n".join(hashes))

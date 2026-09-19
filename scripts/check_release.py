"""Check release checksums, wheel records, and the files included in each archive."""

from pathlib import Path
import base64
import csv
import hashlib
import io
import zipfile

ROOT = Path(__file__).resolve().parents[1]
DIST = ROOT / "dist"
entries = []
for line in (DIST / "SHA256SUMS").read_text().splitlines():
    digest, name = line.split("  ", 1)
    path = DIST / name
    assert path.parent == DIST and path.is_file(), name
    assert hashlib.sha256(path.read_bytes()).hexdigest() == digest, name
    entries.append(path)
assert len(entries) == 8, "Expected four native plugins, two wheels and two ZIPs"
for path in entries:
    if path.suffix not in (".whl", ".zip"):
        continue
    with zipfile.ZipFile(path) as archive:
        names = archive.namelist()
        assert len(names) == len(set(names)), "Duplicate archive entry"
        for name in names:
            assert not name.startswith("/") and ".." not in Path(name).parts, name
            assert Path(name).name not in ("bedrock_server", "bedrock_server.exe"), name
            assert not name.endswith((".log", ".pem", ".pcap", ".pcapng")), name
            assert not any(
                part in ("lab", "worlds", "research", ".git")
                for part in Path(name).parts
            ), name
            if name.endswith((".dll", ".so")):
                assert archive.read(name) == (DIST / Path(name).name).read_bytes(), name
        if path.suffix == ".whl":
            record = next(name for name in names if name.endswith(".dist-info/RECORD"))
            rows = list(csv.reader(io.StringIO(archive.read(record).decode())))
            assert {row[0] for row in rows} == set(names)
            for name, digest, size in rows:
                if name == record:
                    assert not digest and not size
                    continue
                data = archive.read(name)
                actual = (
                    base64.urlsafe_b64encode(hashlib.sha256(data).digest())
                    .rstrip(b"=")
                    .decode()
                )
                assert digest == "sha256=" + actual and int(size) == len(data), name
        else:
            assert {"README.md", "LICENSE", "THIRD_PARTY_NOTICES.md"}.issubset(names)
print(
    "PASS: eight release artifacts, matching native binaries, valid wheel records, and clean archives"
)

"""Inspect the exact retail ELF's RTTI and vtables; no guessed symbol names."""

from pathlib import Path
import json
import sys
from elf_image import Image, CS

ROOT = Path(__file__).resolve().parents[1]
im = Image(
    Path(sys.argv[1])
    if len(sys.argv) > 1
    else ROOT.parent / ".build-inputs/bds-1.26.51-linux/bedrock_server"
)
tables = {}
for start, end, offset, section in im.sections:
    if section != ".rodata":
        continue
    for name in im.raw[offset : offset + end - start].split(b"\0"):
        if len(name) > 90 or not any(
            s in name
            for s in [
                b"ChunkSource",
                b"ChunkView",
                b"TickingArea",
                b"ChunkTick",
                b"LevelChunk",
                b"Player",
                b"LevelData",
            ]
        ):
            continue
        for pos in im.find_string(name):
            for ref in im.references.get(pos, []):
                ti = ref - 8
                for tr in im.references.get(ti, []):
                    if im.pointer(tr - 8) != 0:
                        continue
                    vt = tr + 8
                    slots = []
                    while (
                        im.base
                        <= im.pointer(vt + len(slots) * 8)
                        < im.base + len(im.text)
                    ):
                        slots.append(im.pointer(vt + len(slots) * 8))
                    if slots:
                        tables[name.decode(errors="replace")] = dict(
                            vtable=vt, slots=slots
                        )
(ROOT / "research/vtables.json").write_text(json.dumps(tables, indent=2))
for name, table in tables.items():
    print(name, hex(table["vtable"]), len(table["slots"]))
    if name in [
        "15ChunkViewSource",
        "15MainChunkSource",
        "11ChunkSource",
        "22WorldLimitChunkSource",
        "18NetworkChunkSource",
    ]:
        for i, address in enumerate(table["slots"]):
            print(" ", i, hex(address), "size", im.function(address)[1] - address)
if len(sys.argv) > 2:
    addr = int(sys.argv[2], 0)
    size = min(
        im.function(addr)[1] - addr, int(sys.argv[3], 0) if len(sys.argv) > 3 else 3000
    )
    for ins in CS.disasm(im.read(addr, size), addr):
        print(hex(ins.address), ins.mnemonic, ins.op_str)

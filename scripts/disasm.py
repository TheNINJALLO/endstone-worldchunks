from pathlib import Path
import sys
import struct
import re
from elf_image import Image, CS

root = Path(__file__).resolve().parents[1]
path = root.parent / ".build-inputs/bds-1.26.51-linux/bedrock_server"
args = sys.argv[1:]
if args and args[0] == "--runtime":
    path = root.parent / ".build-inputs/libendstone_runtime-0.11.12-cp314.so"
    args.pop(0)
im = Image(path)
for arg in args:
    if arg.startswith("ptr:"):
        _, start, count = arg.split(":")
        for i in range(int(count, 0)):
            print(i, hex(im.pointer(int(start, 0) + i * 8)))
    elif arg.startswith("sym:"):
        for n, a in im.symbols.items():
            if arg[4:] in n:
                print(hex(a), n)
    elif arg.startswith("calls:"):
        target = int(arg[6:], 0)
        for match in re.finditer(b"\xe8", im.text):
            off = match.start()
            if (
                off + 5 <= len(im.text)
                and im.base + off + 5 + struct.unpack_from("<i", im.text, off + 1)[0]
                == target
            ):
                address = im.base + off
                print("CALL", hex(address), "in", hex(im.function(address)[0]))
    else:
        pieces = arg.split(":")
        addr = int(pieces[0], 0)
        size = int(pieces[1], 0) if len(pieces) > 1 else im.function(addr)[1] - addr
        print("\nFUNCTION", hex(addr), "SIZE", size)
        for ins in CS.disasm(im.read(addr, size), addr):
            print(hex(ins.address), ins.mnemonic, ins.op_str)

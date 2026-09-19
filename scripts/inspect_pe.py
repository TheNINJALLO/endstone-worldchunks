from pathlib import Path
import pefile, capstone, struct, bisect, re


class Image:
    def __init__(self, path):
        self.path = Path(path)
        self.data = self.path.read_bytes()
        self.pe = pefile.PE(data=self.data, fast_load=True)
        self.base = self.pe.OPTIONAL_HEADER.ImageBase
        directory = self.pe.OPTIONAL_HEADER.DATA_DIRECTORY[3]
        entries = self.read(directory.VirtualAddress, directory.Size)
        self.functions = [(a, b) for a, b, _ in struct.iter_unpack("<III", entries)]
        self.starts = [x[0] for x in self.functions]
        self.cs = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)

    def read(self, rva, n):
        offset = self.pe.get_offset_from_rva(rva)
        return self.data[offset : offset + n]

    def function(self, rva):
        i = bisect.bisect_right(self.starts, rva) - 1
        return self.functions[i]

    def dump(self, rva, n=None):
        start, end = self.function(rva)
        if n is None:
            rva = start
            n = end - start
        print("FUNCTION", hex(start), hex(end))
        for ins in self.cs.disasm(self.read(rva, n), rva):
            print(hex(ins.address), ins.mnemonic, ins.op_str)

    def slots(self, rva, count=40):
        return [
            struct.unpack_from("<Q", self.read(rva, count * 8), i * 8)[0] - self.base
            for i in range(count)
        ]

    def xrefs(self, target, kind="lea"):
        section = next(s for s in self.pe.sections if s.Name.startswith(b".text"))
        data = section.get_data()
        offset = section.VirtualAddress
        if kind == "lea":
            candidates = (
                m.start() for m in re.finditer(rb"[\x48\x4c][\x8d\x8b]", data)
            )
        else:
            candidates = (m.start() for m in re.finditer(b"\xe8", data))
        for pos in candidates:
            size = 7 if kind == "lea" else 5
            displacement = struct.unpack_from("<i", data, pos + size - 4)[0]
            if offset + pos + size + displacement == target:
                yield offset + pos, self.function(offset + pos)


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(
        description="Inspect PE functions, virtual tables and direct references."
    )
    parser.add_argument(
        "image", help="Path to the matching BDS executable or Endstone runtime DLL"
    )
    parser.add_argument(
        "commands", nargs="+", help="RVA[:size], refs:RVA, calls:RVA, or slots:RVA"
    )
    args = parser.parse_args()
    image = Image(args.image)
    for arg in args.commands:
        pieces = arg.split(":")
        if pieces[0] == "refs":
            print(list(image.xrefs(int(pieces[1], 0))))
        elif pieces[0] == "calls":
            print(list(image.xrefs(int(pieces[1], 0), "call")))
        elif pieces[0] == "slots":
            print([hex(x) for x in image.slots(int(pieces[1], 0))])
        else:
            image.dump(
                int(pieces[0], 0), int(pieces[1], 0) if len(pieces) > 1 else None
            )

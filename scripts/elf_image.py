"""Offline candidate discovery; results require structural and runtime validation."""

from pathlib import Path
import bisect
import io
import json
import re
import struct
import sys
import capstone
import difflib
from capstone.x86 import X86_OP_MEM, X86_REG_RIP
from elftools.elf.elffile import ELFFile

ROOT = Path(__file__).resolve().parent
CS = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
CS.detail = True


class Image:
    def __init__(self, path):
        self.raw = path.read_bytes()
        self.elf = ELFFile(io.BytesIO(self.raw))
        self.sections = [
            (s["sh_addr"], s["sh_addr"] + s["sh_size"], s["sh_offset"], s.name)
            for s in self.elf.iter_sections()
            if s["sh_flags"] & 2
        ]
        s = self.elf.get_section_by_name(".text")
        self.text = s.data()
        self.base = s["sh_addr"]
        self.relocs = {}
        s = self.elf.get_section_by_name(".rela.dyn")
        if s:
            for off, info, add in struct.iter_unpack("<QQq", s.data()):
                if info & 0xFFFFFFFF == 8:
                    self.relocs[off] = add
        s = self.elf.get_section_by_name(".eh_frame_hdr")
        data = s.data()
        base = s["sh_addr"]
        assert data[:4] == bytes.fromhex("011b033b"), data[:4]
        count = struct.unpack_from("<I", data, 8)[0]
        self.functions = [
            base + struct.unpack_from("<i", data, 12 + 8 * i)[0] for i in range(count)
        ]
        self.symbols = {}
        for name in [".symtab", ".dynsym"]:
            s = self.elf.get_section_by_name(name)
            if s:
                self.symbols.update(
                    {x.name: x["st_value"] for x in s.iter_symbols() if x["st_value"]}
                )
        self.reverse_symbols = {v: k for k, v in self.symbols.items()}
        self.references = {}
        for k, v in self.relocs.items():
            self.references.setdefault(v, []).append(k)

    def read(self, address, size):
        for begin, end, offset, _ in self.sections:
            if begin <= address < end:
                return self.raw[
                    offset + address - begin : offset + address - begin + size
                ]
        raise ValueError(hex(address))

    def pointer(self, address):
        return self.relocs.get(address, struct.unpack("<Q", self.read(address, 8))[0])

    def string(self, address):
        return self.read(address, 2048).split(b"\0", 1)[0]

    def function(self, address):
        i = bisect.bisect_right(self.functions, address) - 1
        return self.functions[i], self.functions[i + 1] if i + 1 < len(
            self.functions
        ) else self.base + len(self.text)

    def find_string(self, value):
        results = []
        for start, end, off, name in self.sections:
            if name != ".rodata":
                continue
            data = self.raw[off : off + end - start]
            pos = 0
            while (pos := data.find(value + b"\0", pos)) >= 0:
                results.append(start + pos)
                pos += 1
        return results

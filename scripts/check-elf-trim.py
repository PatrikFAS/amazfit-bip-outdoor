#!/usr/bin/env python3
"""Verify that an ELF trim preserves the loaded image and app metadata."""
import collections
import os
import pathlib
import struct
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
PREFIX = os.environ.get(
    "ARM_PREFIX",
    str(ROOT / ".tools/gcc-arm-none-eabi-7-2018-q2-update/bin/arm-none-eabi-"),
)
NM = PREFIX + "nm"
SHT_REL = 9
SHF_ALLOC = 2
PT_LOAD = 1


def fail(message):
    raise ValueError(message)


def span(data, offset, size, what):
    if offset < 0 or size < 0 or offset + size > len(data):
        fail(f"{what} lies outside the file")
    return data[offset:offset + size]


def cstring(table, offset, what):
    if offset >= len(table):
        fail(f"invalid {what} string offset")
    end = table.find(b"\0", offset)
    if end < 0:
        fail(f"unterminated {what} string")
    return table[offset:end].decode("ascii", "strict")


def elf(path):
    data = pathlib.Path(path).read_bytes()
    if len(data) < 52 or data[:7] != b"\x7fELF\x01\x01\x01":
        fail(f"{path}: expected ELF32 little-endian ARM")
    (etype, machine, version, entry, phoff, shoff, flags, ehsize,
     phentsize, phnum, shentsize, shnum, shstrndx) = struct.unpack_from("<HHIIIIIHHHHHH", data, 16)
    if etype != 2 or machine != 40 or version != 1 or ehsize != 52 or entry == 0:
        fail(f"{path}: invalid ARM executable header")
    if phentsize != 32 or shentsize != 40 or not phnum or not shnum or shstrndx >= shnum:
        fail(f"{path}: invalid or unsupported program/section table")
    phbytes = span(data, phoff, phnum * phentsize, "program header table")
    shbytes = span(data, shoff, shnum * shentsize, "section header table")
    phdrs = [struct.unpack_from("<IIIIIIII", phbytes, i * 32) for i in range(phnum)]
    raw_sections = [struct.unpack_from("<IIIIIIIIII", shbytes, i * 40) for i in range(shnum)]
    names_hdr = raw_sections[shstrndx]
    names = span(data, names_hdr[4], names_hdr[5], "section-name table")
    sections = {}
    for header in raw_sections:
        name = cstring(names, header[0], "section name")
        if name in sections and name:
            fail(f"{path}: duplicate section {name}")
        contents = b"" if header[1] == 8 else span(data, header[4], header[5], name or "section")
        sections[name] = {"type": header[1], "flags": header[2], "addr": header[3],
                          "link": header[6], "info": header[7],
                          "entsize": header[9], "data": contents}
    loads = [p for p in phdrs if p[0] == PT_LOAD]
    if not loads:
        fail(f"{path}: no PT_LOAD segment")
    for p in phdrs:
        span(data, p[1], p[4], "program segment")
        if p[0] == PT_LOAD and p[4] > p[5]:
            fail(f"{path}: PT_LOAD filesz exceeds memsz")
    if not any(p[6] & 1 and p[2] <= entry < p[2] + p[5] for p in loads):
        fail(f"{path}: entrypoint is outside executable PT_LOAD memory")
    return {"data": data, "entry": entry, "flags": flags, "ident": data[:16],
            "phdrs": phdrs, "loads": loads, "sections": sections}


def globals_from_nm(path):
    result = subprocess.run([NM, "-g", "--defined-only", path], check=True,
                            text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    symbols = []
    for line in result.stdout.splitlines():
        fields = line.split()
        if len(fields) != 3 or len(fields[1]) != 1:
            fail(f"{path}: cannot parse nm output: {line!r}")
        try:
            value = int(fields[0], 16)
        except ValueError:
            fail(f"{path}: invalid nm symbol value: {line!r}")
        symbols.append((fields[2], value, fields[1]))
    return collections.Counter(symbols)


def check(before_path, after_path):
    before, after = elf(before_path), elf(after_path)
    if (before["entry"], before["flags"], before["ident"]) != \
       (after["entry"], after["flags"], after["ident"]):
        fail("ELF entrypoint or ABI header changed")
    if len(before["phdrs"]) != len(after["phdrs"]):
        fail("program header count changed")
    # Offsets may move when non-loaded sections are removed; all fields that
    # affect mapping, plus the bytes selected by each header, must stay equal.
    for old, new in zip(before["phdrs"], after["phdrs"]):
        if (old[0], old[2], old[3], old[4], old[5], old[6], old[7]) != \
           (new[0], new[2], new[3], new[4], new[5], new[6], new[7]):
            fail("program header mapping semantics changed")
        if span(before["data"], old[1], old[4], "input segment") != \
           span(after["data"], new[1], new[4], "output segment"):
            fail("program segment bytes changed")
    if len(before["loads"]) != len(after["loads"]):
        fail("PT_LOAD count changed")
    for old, new in zip(before["loads"], after["loads"]):
        if (old[2], old[4], old[5]) != (new[2], new[4], new[5]):
            fail("PT_LOAD virtual address/filesz/memsz changed")
        if span(before["data"], old[1], old[4], "input PT_LOAD") != \
           span(after["data"], new[1], new[4], "output PT_LOAD"):
            fail("loaded PT_LOAD bytes changed")

    old_symbols = globals_from_nm(before_path)
    new_symbols = globals_from_nm(after_path)
    if old_symbols != new_symbols:
        fail("global defined symbol (name/value/type) set changed")

    old_sections, new_sections = before["sections"], after["sections"]
    for name in (".symtab", ".elf.label", ".elf.settings", ".elf.resources"):
        if name in old_sections and name not in new_sections:
            fail(f"required section removed: {name}")
    for name in (".elf.label", ".elf.settings", ".elf.name", ".elf.resources"):
        if name in old_sections and name in new_sections and \
           old_sections[name]["data"] != new_sections[name]["data"]:
            fail(f"metadata section bytes changed: {name}")
    # .symtab bytes may shrink; it must remain, and nm above checks globals.
    if ".symtab" in old_sections and new_sections[".symtab"]["type"] != old_sections[".symtab"]["type"]:
        fail(".symtab section type changed")

    def runtime_rel_sections(image):
        return {name: (sec["type"], sec["flags"], sec["addr"], sec["entsize"], sec["data"])
                for name, sec in image["sections"].items()
                if sec["type"] == SHT_REL and sec["flags"] & SHF_ALLOC}

    if runtime_rel_sections(before) != runtime_rel_sections(after):
        fail("runtime allocated REL sections changed")
    print(f"PASS: {before_path} -> {after_path}; loaded image, globals, runtime RELs, and metadata preserved")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(f"usage: {pathlib.Path(sys.argv[0]).name} BEFORE.elf AFTER.elf")
    try:
        check(sys.argv[1], sys.argv[2])
    except (OSError, ValueError, subprocess.CalledProcessError, UnicodeError) as error:
        sys.exit(f"FAIL: {error}")

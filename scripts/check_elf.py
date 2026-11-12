"""Basic artifact checks, not a substitute for running the BipOS loader."""
import os
import pathlib
import re
import struct
import subprocess

root = pathlib.Path(__file__).resolve().parent.parent
prefix = os.environ.get('ARM_PREFIX', str(root / '.tools/gcc-arm-none-eabi-7-2018-q2-update/bin/arm-none-eabi-'))
for name in ('hello', 'gps_diagnostic', 'waypoint_navigator', 'tides_sun', 'barometer', 'compass_diagnostic'):
    path = root / 'build' / (name + '.elf')
    data = path.read_bytes()
    header = subprocess.check_output([prefix + 'readelf', '-h', path], text=True)
    # Upstream GNU ld -N -pie emits EXEC with runtime relative relocations,
    # also seen in the existing BT Notes artifact. BipOS supplies the loader.
    assert 'ELF32' in header and 'ARM' in header and 'EXEC' in header
    entry = int(re.search(r'Entry point address:\s+(0x[0-9a-f]+)', header)[1], 16)
    assert entry != 0
    sections = subprocess.check_output([prefix + 'readelf', '-S', path], text=True)
    assert '.elf.label' in sections
    assert '.symtab' in sections, 'Retain the symbol section expected by the BipOS loader'
    if name in ('waypoint_navigator', 'barometer'):
        assert '.elf.settings' in sections
        shoff = struct.unpack_from('<I', data, 32)[0]
        shentsize, shnum, shstrndx = struct.unpack_from('<HHH', data, 46)
        assert shentsize == 40 and shstrndx < shnum
        headers = [struct.unpack_from('<10I', data, shoff + i * shentsize) for i in range(shnum)]
        names_header = headers[shstrndx]
        names = data[names_header[4]:names_header[4] + names_header[5]]
        settings = [h for h in headers if names[h[0]:].split(b'\0', 1)[0] == b'.elf.settings']
        expected = 114 if name == 'waypoint_navigator' else 10
        assert len(settings) == 1 and settings[0][5] == expected, 'Unexpected settings allocation'
        assert data[settings[0][4]:settings[0][4] + expected] == bytes(expected)
    undefined = subprocess.check_output([prefix + 'nm', '-u', path], text=True)
    assert not undefined.strip(), undefined
    relocations = subprocess.check_output([prefix + 'readelf', '-r', path], text=True)
    types = set(re.findall(r'R_ARM_\w+', relocations))
    assert types == {'R_ARM_RELATIVE'}, types
    phoff = struct.unpack_from('<I', data, 28)[0]
    phentsize, phnum = struct.unpack_from('<HH', data, 42)
    assert phentsize == 32 and phoff + phnum * phentsize <= len(data)
    loads = []
    for index in range(phnum):
        kind, offset, vaddr, _, filesz, memsz, _, _ = struct.unpack_from(
            '<8I', data, phoff + index * phentsize)
        if kind == 1:
            assert filesz <= memsz and offset + filesz <= len(data)
            loads.append((vaddr, memsz))
    assert loads and any(addr <= (entry & ~1) < addr + size for addr, size in loads)
    loaded_bytes = sum(size for _, size in loads)
    # Engineering budget prompted by real -9 failure, not a heap guarantee.
    # The watch showed 30,696 bytes free; allow margin for loader/app overhead.
    if name != 'hello':
        assert len(data) <= 16 * 1024, f'App ELF exceeds 16 KiB budget: {len(data)}'
        assert loaded_bytes <= 13 * 1024, f'App load exceeds 13 KiB budget: {loaded_bytes}'
        assert len(data) + loaded_bytes <= 28 * 1024, 'App file + load budget exceeds 28 KiB'
    print(f'{name}: file {len(data)} bytes; PT_LOAD {loaded_bytes} bytes; combined {len(data) + loaded_bytes}')
    print(f'{name}: ARM entry, relative relocations, metadata and undefined-symbol checks passed')

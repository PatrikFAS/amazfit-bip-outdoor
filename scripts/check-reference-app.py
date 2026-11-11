#!/usr/bin/env python3
"""Check the archived GPS Returner's versioned alias literals against our table.

This is an import-table check, not verification of the app's GPS behaviour.
"""
import struct
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
app = Path(sys.argv[1])
data = app.read_bytes()
table = (root / "vendor/bip-web/public/files/libbip/BipOS_0.5.5_b7_not_latin_1.1.2.05_ENGLISH.bin").read_bytes()
assert data[:7] == b"\x7fELF\x01\x01\x01" and table[:6] == b"LIBBIP"
table_size = struct.unpack_from("<I", table, 8)[0]
keys = struct.unpack_from(f"<{table_size // 4}I", table, 32)
values = struct.unpack_from(f"<{table_size // 4}I", table, 32 + table_size)
aliases = {}
for key, value in zip(keys, values):
    aliases.setdefault(key, set()).add(value)
offset = struct.unpack_from("<I", data, 32)[0]
entry_size, count, names_index = struct.unpack_from("<HHH", data, 46)
headers = [struct.unpack_from("<10I", data, offset + i * entry_size) for i in range(count)]
names_header = headers[names_index]
names = data[names_header[4]:names_header[4] + names_header[5]]
required = set()
for header in headers:
    if names[header[0]:].split(b"\0", 1)[0] == b".text":
        for index in range(header[4], header[4] + header[5] - 3, 2):
            word = struct.unpack_from("<I", data, index)[0]
            if word >> 16 == 0xFACE:
                required.add(word)
assert required, "No versioned imports found in reference app"
for key in required:
    assert len(aliases.get(key, set())) == 1, f"Missing/conflicting reference alias {key:#x}"
print(f"GPS Returner: {len(required)} versioned alias literals resolve in the English table")

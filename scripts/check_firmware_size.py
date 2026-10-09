"""Check the actual app image against its generated partition table (not ELF estimates)."""
from pathlib import Path
import struct
import sys

root = Path(__file__).resolve().parents[1]
if len(sys.argv) != 2:
    raise SystemExit("Usage: python scripts/check_firmware_size.py <platformio-environment>")
directory = root / ".pio/build" / sys.argv[1]
image = directory / "firmware.bin"
table = (directory / "partitions.bin").read_bytes()
slots = []
for offset in range(0, len(table) - 31, 32):
    magic, kind, subtype, address, size, label, flags = struct.unpack_from("<HBBII16sI", table, offset)
    if magic != 0x50AA:
        break
    if kind == 0:
        slots.append(size)
if not slots:
    raise SystemExit("No application partition found")
size = image.stat().st_size
limit = min(slots)
if size > limit:
    raise SystemExit(f"{sys.argv[1]}: actual image {size} exceeds application slot {limit}")
print(f"{sys.argv[1]}: actual image {size} / {limit} bytes; {limit - size} bytes free")

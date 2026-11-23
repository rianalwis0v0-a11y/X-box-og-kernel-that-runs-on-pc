#!/usr/bin/env python3
"""Bounds-checked PE32/i386 image inspector. Validation only; never executes images."""
from __future__ import annotations
import argparse
import json
import struct
import sys
from pathlib import Path

class PEError(ValueError):
    pass

def u16(data: bytes, off: int) -> int:
    if off < 0 or off + 2 > len(data):
        raise PEError(f"truncated 16-bit field at file offset 0x{off:X}")
    return struct.unpack_from("<H", data, off)[0]

def u32(data: bytes, off: int) -> int:
    if off < 0 or off + 4 > len(data):
        raise PEError(f"truncated 32-bit field at file offset 0x{off:X}")
    return struct.unpack_from("<I", data, off)[0]

def span_ok(start: int, size: int, limit: int) -> bool:
    return start <= limit and size <= limit - start

def inspect_pe32(data: bytes) -> dict:
    if len(data) < 0x40 or data[:2] != b"MZ":
        raise PEError("missing DOS MZ header")
    pe = u32(data, 0x3C)
    if not span_ok(pe, 24, len(data)) or data[pe:pe+4] != b"PE\0\0":
        raise PEError("invalid or truncated PE signature/header")
    coff = pe + 4
    machine = u16(data, coff)
    count = u16(data, coff + 2)
    optional_size = u16(data, coff + 16)
    if machine != 0x14C:
        raise PEError(f"unsupported machine 0x{machine:04X}; expected i386 (0x014C)")
    if count == 0 or count > 96:
        raise PEError(f"invalid section count: {count}")
    opt = coff + 20
    if optional_size < 96 or not span_ok(opt, optional_size, len(data)):
        raise PEError("truncated PE32 optional header")
    if u16(data, opt) != 0x10B:
        raise PEError("optional header is not PE32 (0x010B)")
    entry = u32(data, opt + 16)
    image_base = u32(data, opt + 28)
    section_alignment = u32(data, opt + 32)
    file_alignment = u32(data, opt + 36)
    size_image = u32(data, opt + 56)
    size_headers = u32(data, opt + 60)
    directory_count = u32(data, opt + 92)
    if not section_alignment or not file_alignment or not size_image:
        raise PEError("zero alignment or SizeOfImage")
    if size_headers > len(data) or size_headers > size_image:
        raise PEError("invalid SizeOfHeaders")
    if section_alignment < file_alignment:
        raise PEError("SectionAlignment is smaller than FileAlignment")
    dirs_off = opt + 96
    available_dirs = max(0, (optional_size - 96) // 8)
    if directory_count > available_dirs:
        raise PEError("data-directory count exceeds optional-header bounds")
    directories = []
    for i in range(directory_count):
        rva, size = struct.unpack_from("<II", data, dirs_off + i * 8)
        if rva and size and not span_ok(rva, size, size_image):
            raise PEError(f"data directory {i} exceeds SizeOfImage")
        directories.append({"index": i, "rva": rva, "size": size})
    sec_off = opt + optional_size
    if not span_ok(sec_off, count * 40, len(data)):
        raise PEError("truncated section table")
    sections = []
    virtual_ranges = []
    entry_executable = False
    for i in range(count):
        off = sec_off + i * 40
        name = data[off:off+8].split(b"\0", 1)[0].decode("ascii", "replace")
        virtual_size, va, raw_size, raw_ptr = struct.unpack_from("<IIII", data, off + 8)
        characteristics = u32(data, off + 36)
        mapped_size = max(virtual_size, raw_size)
        if raw_size and (raw_ptr < size_headers or not span_ok(raw_ptr, raw_size, len(data))):
            raise PEError(f"section {name!r} raw data is outside file or overlaps headers")
        if mapped_size and not span_ok(va, mapped_size, size_image):
            raise PEError(f"section {name!r} virtual range exceeds SizeOfImage")
        if mapped_size:
            lo, hi = va, va + mapped_size
            for old_lo, old_hi, old_name in virtual_ranges:
                if lo < old_hi and old_lo < hi:
                    raise PEError(f"virtual sections {old_name!r} and {name!r} overlap")
            virtual_ranges.append((lo, hi, name))
        if va <= entry < va + max(virtual_size, raw_size) and (characteristics & 0x20000000):
            entry_executable = True
        sections.append({
            "name": name, "virtual_address": va, "virtual_size": virtual_size,
            "raw_offset": raw_ptr, "raw_size": raw_size,
            "characteristics": f"0x{characteristics:08X}",
        })
    if entry == 0 or entry >= size_image:
        raise PEError("AddressOfEntryPoint is outside SizeOfImage")
    if not entry_executable:
        raise PEError("entry point is not inside an executable section")
    return {
        "format": "PE32", "machine": "i386", "image_base": f"0x{image_base:08X}",
        "entry_rva": f"0x{entry:08X}", "entry_va_preferred": f"0x{image_base + entry:08X}",
        "section_alignment": section_alignment, "file_alignment": file_alignment,
        "size_of_image": size_image, "size_of_headers": size_headers,
        "sections": sections, "data_directories": directories,
        "relocation_directory": directories[5] if len(directories) > 5 else None,
        "import_directory": directories[1] if len(directories) > 1 else None,
        "execution": "NOT ATTEMPTED (inspection only)",
    }

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    args = parser.parse_args()
    try:
        report = inspect_pe32(args.image.read_bytes())
    except (OSError, PEError) as exc:
        print(f"PE32-INSPECT: REJECTED: {exc}", file=sys.stderr)
        return 2
    print(json.dumps(report, indent=2))
    return 0

if __name__ == "__main__":
    raise SystemExit(main())

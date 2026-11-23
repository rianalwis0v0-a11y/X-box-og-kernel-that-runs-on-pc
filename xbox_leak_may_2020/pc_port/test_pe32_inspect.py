import struct
import unittest
from pe32_inspect import PEError, inspect_pe32

def fixture():
    b = bytearray(0x400)
    b[0:2] = b"MZ"
    struct.pack_into("<I", b, 0x3C, 0x80)
    b[0x80:0x84] = b"PE\0\0"
    c = 0x84
    struct.pack_into("<HHIIIHH", b, c, 0x14C, 1, 0, 0, 0, 224, 0x0102)
    o = c + 20
    struct.pack_into("<H", b, o, 0x10B)
    struct.pack_into("<I", b, o+16, 0x1000)
    struct.pack_into("<I", b, o+28, 0x80010000)
    struct.pack_into("<II", b, o+32, 0x1000, 0x200)
    struct.pack_into("<II", b, o+56, 0x2000, 0x200)
    struct.pack_into("<I", b, o+92, 16)
    s = o + 224
    b[s:s+8] = b".text\0\0\0"
    struct.pack_into("<IIII", b, s+8, 0x100, 0x1000, 0x200, 0x200)
    struct.pack_into("<I", b, s+36, 0x60000020)
    b[0x200:0x400] = b"\x90" * 0x200
    return bytes(b)

class PE32Tests(unittest.TestCase):
    def test_valid_i386_image(self):
        r = inspect_pe32(fixture())
        self.assertEqual(r["machine"], "i386")
        self.assertEqual(r["entry_va_preferred"], "0x80011000")
        self.assertEqual(r["execution"], "NOT ATTEMPTED (inspection only)")

    def test_rejects_non_mz(self):
        with self.assertRaises(PEError):
            inspect_pe32(b"not an executable")

    def test_rejects_non_i386(self):
        b = bytearray(fixture())
        struct.pack_into("<H", b, 0x84, 0x8664)
        with self.assertRaisesRegex(PEError, "unsupported machine"):
            inspect_pe32(bytes(b))

    def test_rejects_entry_outside_executable_section(self):
        b = bytearray(fixture())
        struct.pack_into("<I", b, 0x84+24+16, 0x1800)
        with self.assertRaisesRegex(PEError, "entry point is not inside"):
            inspect_pe32(bytes(b))

    def test_rejects_truncated_section_raw_data(self):
        b = bytearray(fixture())
        struct.pack_into("<I", b, 0x84+24+224+20, 0x1000)
        with self.assertRaisesRegex(PEError, "raw data"):
            inspect_pe32(bytes(b))

if __name__ == "__main__":
    unittest.main()

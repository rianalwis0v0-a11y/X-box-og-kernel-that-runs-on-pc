"""Static integration guards for the original Xbox x86 HAL's native-PC path.

These tests do not replace compiling the historical NT DDK target. They guard
the source-level PC_COMPAT_NATIVE wiring while that toolchain is unavailable.
"""
from pathlib import Path
import unittest


HAL = (
    Path(__file__).resolve().parents[1]
    / "xbox trunk"
    / "xbox"
    / "private"
    / "ntos"
    / "halx"
)


class NativeXboxHalSourceTests(unittest.TestCase):
    def test_native_build_switch_and_original_x86_sources_are_wired(self):
        sources = (HAL / "sources").read_text(encoding="utf-8")
        self.assertIn("-DPC_COMPAT_NATIVE", sources)
        self.assertIn("i386\\ixsysint.asm", sources)
        self.assertIn("i386\\ixclock.asm", sources)
        self.assertIn("i386\\pcispace.c", sources)
        self.assertIn("i386\\xxhal.c", sources)
        self.assertIn("PC_COMPAT_GUEST and PC_COMPAT_NATIVE are mutually exclusive", sources)

    def test_native_phase_zero_uses_nt_pic_rtc_and_pit_without_xbox_chipset_setup(self):
        source = (HAL / "i386" / "xxhal.c").read_text(encoding="utf-8")
        native = source.split("#elif defined(PC_COMPAT_NATIVE)", 1)[1].split("#else", 1)[0]
        self.assertIn("HalpInitializePICs();", native)
        self.assertIn("HalpInitializeRealTimeClock();", native)
        self.assertIn("HalpInitializeClock();", native)
        self.assertNotIn("HalpSetupPCIDevices();", native)
        self.assertNotIn("HalpInitializeSystemControlInterrupt();", native)

    def test_native_pci_setup_never_replays_xbox_fixed_bdfs(self):
        source = (HAL / "i386" / "pcisetup.c").read_text(encoding="utf-8")
        native = source.split("#if defined(PC_COMPAT_GUEST) || defined(PC_COMPAT_NATIVE)", 1)[1].split("#else", 1)[0]
        self.assertIn("return;", native)
        self.assertIn("HalReadWritePCISpace remains the generic PCI config-space interface.", native)

    def test_pci_config_access_restores_the_callers_interrupt_flag(self):
        source = (HAL / "i386" / "pcispace.c").read_text(encoding="utf-8")
        self.assertIn("pushfd", source)
        self.assertIn("mov     OldEFlags, eax", source)
        self.assertIn("cli", source)
        self.assertIn("if (OldEFlags & 0x00000200)", source)
        self.assertNotIn("    _enable();\r\n}", source)


if __name__ == "__main__":
    unittest.main()

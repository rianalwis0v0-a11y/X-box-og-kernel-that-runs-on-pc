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

    def test_firmware_apic_pages_are_reserved_and_mapped_uncached(self):
        source = (Path(__file__).resolve().parent / "kernel.c").read_text(encoding="utf-8")
        self.assertIn("firmware_madt = madt;", source)
        self.assertIn("pc_physmem_reserve(&physical_memory", source)
        paging = (Path(__file__).resolve().parent / "pc_paging.c").read_text(encoding="utf-8")
        self.assertIn("PC_PAGING_WRITE | PC_PAGING_PCD | PC_PAGING_PWT", paging)
        self.assertIn("pc_paging_map_mmio_identity", source)
        self.assertIn("map_firmware_apic_mmio()", source)
        self.assertIn("firmware LAPIC/IOAPIC MMIO mappings prepared uncached (paging active)", source)

    def test_page_table_activation_is_guarded_by_identity_map_bounds(self):
        source = (Path(__file__).resolve().parent / "kernel.c").read_text(encoding="utf-8")
        self.assertIn("static int activate_pc_paging(uint32_t directory_physical)", source)
        self.assertIn("kernel_finish > 0x10000000u", source)
        self.assertIn("stack_pointer >= 0x10000000u", source)
        self.assertIn("block < 0x10000000u", source)
        self.assertIn("PC-HAL: paging activated; identity-map execution check PASS", source)
        self.assertIn("or $0x80000000, %%eax", source)

    def test_pci_config_access_restores_the_callers_interrupt_flag(self):
        source = (HAL / "i386" / "pcispace.c").read_text(encoding="utf-8")
        self.assertIn("pushfd", source)
        self.assertIn("mov     OldEFlags, eax", source)
        self.assertIn("cli", source)
        self.assertIn("if (OldEFlags & 0x00000200)", source)
        self.assertNotIn("    _enable();\r\n}", source)


if __name__ == "__main__":
    unittest.main()

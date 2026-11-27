# PC HAL Port Matrix

## Objective

Port the PC-hardware support needed to boot the existing Xbox kernel on an IA-32 PC/QEMU machine. Keep the Xbox kernel and its existing HAL entry points; do not create a second host-side HAL or replace the kernel with ReactOS.

## Reference and licensing boundary

Use the ReactOS x86 HAL as an architectural reference for NT-style PC hardware support, not as code to copy into this repository. ReactOS HAL is GPL-2.0; importing its implementation into this repository requires an explicit licensing decision and compatibility review. Windows 2000 HAL binaries are not source code and are not a safe drop-in replacement: HAL/kernel private interfaces and build-specific structures must match exactly.

Reference directories:
- ReactOS: `hal/halx86/generic`
- ReactOS legacy PIC: `hal/halx86/pic`
- ReactOS APIC: `hal/halx86/apic`
- ReactOS ACPI: `hal/halx86/acpi`

## Hardware capability mapping

| Capability | Reference area | Xbox HAL area to assess | Porting rule |
|---|---|---|---|
| 8259 PIC and IRQ masking | `pic/`, `generic/pic.c` | `ix8259.inc`, `ixsysint.asm` | Preserve Xbox interrupt/IRQL contract; implement PC PIC programming behind existing HAL interfaces. |
| APIC / I/O APIC | `apic/` | Existing interrupt and initialization path | Add only after MADT parsing, MMIO mapping, vector allocation and EOI handling are verified. |
| ACPI discovery | `acpi/` | `mcpxacpi.inc` and PC initialization path | Discover RSDP, validate checksums, parse MADT; do not assume Xbox firmware tables exist on PCs. |
| PCI configuration | `generic/kdpci.c`, PCI support | `pcisetup.c`, `pcispace.c` | Start with mechanism #1 (CF8/CFC); validate BAR sizing and resource ownership. |
| Clock / timer | `generic/clock.c`, `generic/timer.c`, APIC timer | `ixclock.asm`, `pmtimer.asm` | Select a timer only after platform discovery; calibrate and test monotonic tick behavior. |
| CMOS / RTC | `generic/cmos.c` | `ixcmos.asm`, `ixcmos.inc` | Keep RTC access separate from Xbox EEPROM semantics. |
| DMA / memory mapping | `generic/dma.c`, `generic/memory.c` | Existing Xbox DMA and memory paths | Define PC DMA/IOMMU assumptions explicitly; never reuse Xbox device addresses blindly. |
| Port I/O and reboot | `generic/portio.c`, `generic/reboot.c` | `xxioacc.asm`, `ixreboot.c` | Use architecture-defined x86 I/O operations; preserve Xbox-specific shutdown/reboot behavior behind platform checks. |

## Required implementation sequence

1. Record the Xbox HAL's exported entry points, calling conventions, IRQL assumptions, and initialization order from project-owned interface declarations.
2. Add a PC platform-selection path without deleting the Xbox hardware path.
3. Implement BIOS/ACPI discovery and physical MMIO mapping with bounds and checksum validation.
4. Bring up legacy PIC interrupts first in QEMU; verify timer ticks and keyboard IRQ before APIC.
5. Add PCI enumeration and resource assignment; log vendor/device IDs and BARs.
6. Add APIC support only after PIC-mode boot is stable.
7. Add a QEMU smoke test that proves the Xbox kernel itself reaches its HAL initialization milestones. A separate test kernel or host harness does not count as success.

## Acceptance criteria

- Existing Xbox build remains intact.
- PC mode is selected explicitly and Xbox hardware mode remains available.
- No ReactOS GPL implementation is copied into this repository without an approved license plan.
- No Windows 2000 HAL binary is substituted for the Xbox HAL.
- CI captures serial/debug output proving HAL entry, firmware discovery, interrupt-controller initialization, timer progress, and PCI enumeration.
- A successful PC HAL test must execute the Xbox kernel, not merely a standalone PC test kernel.

## Current status

This is an analysis/porting plan only. No HAL implementation has been replaced or claimed to boot on PC by this document.

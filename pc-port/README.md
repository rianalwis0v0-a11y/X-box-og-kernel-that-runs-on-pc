# Original Xbox kernel: PC port workbench

This directory is a clean-room **PC platform bring-up workbench**. It does not modify the imported Xbox source tree and does not claim that the Xbox kernel boots on a PC yet.

## What the source tree tells us

The checked-in tree contains real Xbox kernel, boot and HAL source directories under `xbox_leak_may_2020/xbox trunk/xbox/private/ntos/`. In particular:

- `bootx/preldr32` builds the pre-loader image `xpreldr.bin`.
- `bootx/bldr32` builds the boot-loader image `xboxbldr.bin`; its build description uses the native subsystem and the `BldrStartup` entry point.
- `init/console/sources` selects the `xboxkrnl` image and its kernel libraries.
- `halx/sources` builds the Xbox HAL from x86 assembly and C sources.
- `halx/i386/xxhal.c` performs early interrupt-controller, RTC, timer and chipset initialization.
- `halx/i386/pcisetup.c` configures Xbox-specific LPC, IDE and NIC PCI devices and hard-coded routing/register values.

That last point is the central porting obstacle: the HAL is x86, but it is **not a generic PC HAL**. The boot chain also assumes Xbox ROM/board initialization and platform-specific memory/register mappings. A PC port must replace those assumptions, not simply relink the original binaries.

## Scope of this first increment

The initial code defines and tests a small, versioned boot-information contract between a future PC loader and a future kernel/HAL adapter. It checks memory-map arithmetic and optional framebuffer metadata without touching privileged hardware.

This is deliberately not a bootable kernel or a replacement for the Xbox boot ROM. It gives the loader and HAL work a stable, testable boundary before privileged code is introduced.

## Build and test

Requirements: a C11 compiler and Make.

```sh
make -C pc-port test
```

## Next engineering stages

1. Implement a Multiboot2 or UEFI loader that populates `pc_boot_info`; keep it separate from Xbox ROM code.
2. Add x86 CPU discovery (CPUID), physical-memory map handling and a PC interrupt-controller path (APIC first, PIC fallback).
3. Add PCI enumeration using configuration mechanism 1 or PCIe ECAM; discover devices rather than using Xbox bus/device constants.
4. Add a serial debug console and a deterministic QEMU test target.
5. Only after the loader and generic PC HAL work, map the Xbox kernel's required entry state and replace board-specific HAL routines incrementally.

## Safety and compatibility

Do not run the Xbox bootloader or kernel on a physical PC as-is. Do not write to firmware, flash, disks or PCI registers during early tests. QEMU is the intended first execution target. Keep the imported source tree unchanged while the compatibility layer is developed.

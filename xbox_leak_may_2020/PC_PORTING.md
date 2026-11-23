# Native PC Port: Engineering Plan

This document tracks the work required to boot the original Xbox kernel on ordinary x86 PCs. It is deliberately separate from the leaked source: do not modify leaked files until their provenance, build system, target architecture, and licensing are understood.

## Goal and acceptance criteria

Boot the Xbox kernel natively on x86 hardware or a PC virtual machine, without emulating the Xbox CPU. Xbox-specific devices may initially be absent; the first milestone is reaching a controlled kernel diagnostic halt, not running games.

A milestone is complete only when its test output, build command, compiler version, and commit are recorded.

## Stage 0 — source and toolchain inventory

- [x] Identify the principal kernel, bootloader, HAL, and build-manifest directories. The full 63,683-entry recursive tree is truncated, so exhaustive inventory remains open.
- [ ] Record source provenance and licensing. Do not assume a directory name proves that files are an authentic Microsoft leak.
- [x] Confirm the kernel/HAL target is x86 and the source build uses Microsoft's legacy SOURCES build system and Microsoft linker flags. Exact kernel image entry and ABI still need inspection.
- [ ] Separate source files from binaries and generated artifacts.
- [ ] Establish reproducible hashes for the untouched input tree.

## Stage 1 — PC boot entry

Implement a small, independent x86 boot harness. It must not overwrite or patch the original kernel image.

- [x] Select a first test path: Multiboot v1 via GRUB/QEMU, 32-bit protected mode. This validates the PC harness only; it is not the original Xbox firmware path.
- [x] Confirm the Xbox HAL source is x86-specific; the isolated harness enters 32-bit protected mode.
- [ ] Establish a known-good stack, GDT, IDT, and exception handlers.
- [ ] Capture CPU vendor/features and physical memory map.
- [ ] Provide serial-console diagnostics and a deterministic halt on fatal errors.
- [ ] Validate the harness with a minimal standalone test image before loading kernel code.

## Stage 2 — kernel image contract

- [ ] Determine the kernel image format and entry-point contract from source/build metadata.
- [ ] Verify section alignment, relocations, imports, symbols, and expected load address.
- [ ] Implement only the required loader/relocation behavior, with bounds checks.
- [ ] Confirm that the entry point can be reached and report its first diagnostic.
- [ ] Do not guess Xbox memory addresses or silently map PC memory over them.

## Stage 3 — hardware abstraction

Create a PC-specific HAL adapter rather than pretending PC devices are Xbox devices.

Initial PC facilities:
- serial output for diagnostics;
- physical memory discovery and page-table setup;
- interrupt-controller setup (PIC initially if appropriate, then IOAPIC/APIC);
- timer source;
- keyboard input only after interrupt handling is stable.

Xbox-specific facilities requiring explicit implementations or stubs include NV2A, MCPX, SMBus, Xbox IDE/DMA behavior, and console-specific interrupt routing. A stub must return a documented unsupported status; it must never fake successful hardware initialization.

## Stage 4 — validation ladder

1. Boot harness prints a serial banner.
2. CPU mode, GDT, IDT, and exception path pass self-tests.
3. Memory map is parsed and reserved regions are respected.
4. Kernel image is validated and loaded without executing it.
5. Kernel entry is invoked and emits a diagnostic (or reaches a captured exception).
6. Required kernel subsystems are brought up one at a time.
7. Xbox-specific hardware support is added and tested independently.

## Required evidence for each change

Record:
- exact build command and toolchain versions;
- test command and complete serial/log output;
- expected versus actual result;
- hardware/VM configuration;
- known limitations.

## Current status

Source analysis has confirmed these concrete facts:

- The Xbox kernel source is under xbox trunk/xbox/private/ntos; init/sources.inc builds an init library, and init/ntoskrnl.src defines the xboxkrnl.exe export table.
- The Xbox HAL has an x86 implementation under halx/i386. xxhal.c initializes the 8259 PIC, RTC, clock, PCI southbridge devices, and system-control interrupt; it also disables the local APIC in its Phase 0 path. halp.h includes Xbox-specific PCI and SMC definitions.
- halx/sources lists the original assembly and C implementation files, including interrupt, clock, PCI, SMBus/SCI, and Super-I/O code.
- The boot tree contains bootx/preldr32 and bootx/bldr32. preldr32.c verifies ROM contents/signature, decrypts the bootloader, and derives its entry address from ROM-specific data. That flow cannot be used unchanged on a PC with no Xbox ROM/MCPX.
- CPXXUPD/private/ntos/init/sources.inc is only an update component manifest and links against prebuilt Xbox libraries; it is not a complete independent kernel build.

The independent PC harness, Makefile, linker script, serial diagnostics, and QEMU CI workflow have been committed under pc_port/. GitHub Actions run #1 is building the harness. Its result must be checked before marking the harness milestone complete.

Next engineering task: inspect bootx/bldr32 startup/loader contract and the halx/i386 initialization dependencies, then define the smallest PC loader-to-kernel handoff. Do not call the original Xbox preloader on a PC: its ROM signature/decryption and fixed-address assumptions are platform-specific.

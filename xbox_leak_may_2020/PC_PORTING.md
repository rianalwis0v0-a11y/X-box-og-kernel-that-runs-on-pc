# Native PC Port: Engineering Plan

This document tracks the work required to boot the original Xbox kernel on ordinary x86 PCs. It is deliberately separate from the leaked source: do not modify leaked files until their provenance, build system, target architecture, and licensing are understood.

## Goal and acceptance criteria

Boot the Xbox kernel natively on x86 hardware or a PC virtual machine, without emulating the Xbox CPU. Xbox-specific devices may initially be absent; the first milestone is reaching a controlled kernel diagnostic halt, not running games.

A milestone is complete only when its test output, build command, compiler version, and commit are recorded.

## Stage 0 — source and toolchain inventory

- [ ] Enumerate every file under this directory and identify the actual kernel, headers, libraries, build scripts, and generated binaries.
- [ ] Record source provenance and licensing. Do not assume a directory name proves that files are an authentic Microsoft leak.
- [ ] Identify the target CPU mode, calling convention, compiler, linker, object format, and required SDK.
- [ ] Separate source files from binaries and generated artifacts.
- [ ] Establish reproducible hashes for the untouched input tree.

## Stage 1 — PC boot entry

Implement a small, independent x86 boot harness. It must not overwrite or patch the original kernel image.

- [ ] Start in a documented firmware path (UEFI x64 or legacy BIOS; choose one first).
- [ ] Enter the CPU mode required by the kernel (likely 32-bit protected mode, to be verified from source).
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

This is a plan, not a claim that the leaked source currently builds or boots. The source tree must be inventoried before writing an ABI-specific loader or modifying kernel code. The first implementation target is the isolated PC boot harness and its self-tests.

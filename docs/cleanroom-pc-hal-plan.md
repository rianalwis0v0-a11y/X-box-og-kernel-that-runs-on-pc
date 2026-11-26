# Clean-room PC HAL port plan

This document describes an independently implemented PC hardware abstraction layer. It is not derived from proprietary or leaked source code.

## Objective

Build a small, testable x86 PC HAL that can run under QEMU first, with an eventual path to physical PCs. Keep hardware access behind explicit interfaces so a higher-level kernel can be adapted separately, only where legally available.

## HAL modules

- **Boot contract**: document the bootloader-provided CPU mode, memory map, boot parameters, and loaded image. Reject unsupported boot states instead of silently assuming them.
- **CPU**: CPUID feature discovery, basic CPU identification, and safe feature gating.
- **Interrupts**: IDT setup, exception handlers, PIC initially; APIC support as a later milestone.
- **Timer**: PIT fallback and a monotonic tick source; HPET/APIC timer later if exposed by the emulator.
- **Memory**: physical memory map parsing, page-frame allocator interface, paging setup/validation.
- **PCI**: read-only enumeration first; validate configuration reads and avoid touching device BARs until a driver owns them.
- **Serial diagnostics**: COM1 early output for QEMU logs, with failure-safe polling.
- **Power/reset**: emulator-safe reset path; no ACPI power actions until table validation is implemented.

## Development sequence

1. Write down and validate the boot contract using a minimal standalone test kernel.
2. Establish serial output and an exception/interrupt table; test deliberate faults in QEMU.
3. Add timer ticks and verify stable monotonic counting.
4. Parse the memory map and test allocator invariants (alignment, bounds, no overlap).
5. Enumerate PCI devices read-only and compare output with QEMU's device list.
6. Add a structured HAL API and stub backends for deterministic unit tests.
7. Only after these tests pass, integrate with a separately licensed kernel implementation.

## Initial QEMU target

Start with qemu-system-i386 or qemu-system-x86_64 in a deliberately simple machine configuration, one CPU, serial console, and no GPU passthrough. The first success criterion is a repeatable serial log and a clean halt—not a graphical boot.

## Safety and compatibility boundaries

- Do not copy or adapt code from leaked or proprietary kernel/HAL sources.
- Do not assume an Xbox boot contract is interchangeable with a PC boot contract.
- Do not access hardware registers based only on guessed addresses.
- Do not initialize GPU, DMA, or storage hardware until ownership, mappings, and device specifications are verified.
- Keep PC HAL tests separate from Xbox ABI/kernel integration tests.

## Status

Planning scaffold only. No bootloader, kernel, or HAL implementation has been compiled or boot-tested yet.

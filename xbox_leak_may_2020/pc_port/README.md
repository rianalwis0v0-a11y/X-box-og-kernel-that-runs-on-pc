# Native PC bare-metal bring-up harness

This is a real ring-0 x86 test kernel, booted directly by a Multiboot v1 bootloader. It does not run as a Windows/Linux process and does not require VT-x, AMD-V, or VMCALL. GRUB is only the bootloader; after handoff, the harness executes privileged CPU and PCI I/O instructions directly.

## Current executable milestone

- 32-bit protected-mode entry with a private stack.
- COM1 serial diagnostics.
- CPUID vendor identification.
- Multiboot v1 physical memory-map parsing with entry-length bounds checks.
- PCI configuration mechanism #1 enumeration through ports 0xCF8/0xCFC.
- NVIDIA NV2A PCI ID detection (10DE:02A0), if present.
- Deterministic halt after hardware probes.

## Build and test

Requires a 32-bit-capable GCC/binutils toolchain and QEMU:

```sh
make CROSS=
qemu-system-i386 -kernel pc-harness.elf -display none -serial stdio -monitor none
```

The GitHub Actions workflow builds the ELF, checks its Multiboot header, boots it in QEMU, and verifies the serial diagnostics.

## Xbox kernel boot-chain integration (first step)

The harness now accepts an `xboxkrnl.exe` Multiboot module and validates its PE32/i386 headers, entry RVA, image/header sizes, section table, and section raw/virtual ranges. Pass the module to GRUB with the module name exactly `xboxkrnl.exe`. The serial log reports the preferred image base, entry RVA, image size, and section count.

This is deliberately a validation milestone, not a kernel launch: it does not yet map sections, apply PE relocations, resolve imports, initialize the Xbox kernel's expected boot parameters/crypt-key ABI, or jump to its entry point. The original Xbox `preldr32` depends on Xbox ROM/MCPX key material and cannot run unchanged on a PC. The next port step is a PC-native replacement for the ROM-loading portion of `bldr32`, while preserving the original kernel image and its own XBE loader.

## Native PC HAL primitives (new foundation)

`pc_hal_native.c/.h` adds direct ring-0 x86 I/O primitives, a dual-8259 PIC remap to the Xbox HAL's existing vector base (0x30/0x38), and PCI Configuration Mechanism #1 reads through 0xCF8/0xCFC. The PIC is left fully masked; these routines do not install an IDT, enable an IRQ, or start a clock. PCI access preserves the caller's interrupt-enable state.

This is a native hardware path, not a VMCALL adapter. It is currently a separately compiled porting module: it has **not** yet replaced the Xbox HAL's initialization call sites or been built with the original Xbox NT DDK toolchain. The PCI config path is only the legacy mechanism #1 256-byte configuration space; ACPI, APIC routing, timer calibration, DMA, storage, and interrupt dispatch remain to be implemented and integrated. The module must run only at ring 0 with a valid x86 environment.

## Critical limitation

This proves only the standalone native PC harness and its early hardware probes. It does **not** load or boot the original Xbox kernel. The Xbox kernel still needs a verified PE32 loader/relocation/import path, its exact boot handoff, native replacement implementations for Xbox-specific HAL services, and integration of the NV2A translation layer. The existing `PC_COMPAT_GUEST` VMCALL adapter is a separate hypervisor-guest experiment; it is not used by this bare-metal harness.

Do not flash this test ELF to a physical disk or firmware. Test it in QEMU first; use removable media only after the boot image and hardware paths are validated.


## XboxRuntime code reuse: XBE parser

The portable `xbe_format.h/.cpp` parser and image-layout routine were copied from [rianalwis0v0-a11y/XboxRuntime](https://github.com/rianalwis0v0-a11y/XboxRuntime/tree/main/modules/xbe_loader). The source MIT license is preserved in `XBOXRUNTIME_LICENSE.txt`. This code validates XBE metadata and maps headers/sections into a caller-provided buffer; it does not perform disk I/O, relocation, kernel-thunk import resolution, TLS execution, or kernel entry.

CI compiles this parser as a 32-bit freestanding C++ translation unit. It is not linked into the boot harness yet: the next step is a physical-storage reader and a bounded allocator/memory-map reservation, then a verified XBE-to-kernel boot handoff. Windows-only runtime files and the Windows guest execution backend were intentionally not copied into the bare-metal kernel.


## Kernel entry and unsupported PE startup features

The source-defined Xbox kernel entry is `__cdecl main(PUCHAR LoadOptions, const UCHAR *CryptKeys)`; see `xbox_pc_kernel_entry` in `xbox_pc_boot.h`. The port must provide valid load-options and crypt-key buffers before invoking it. It must not invent EEPROM/certificate key contents.

The PE32 validator rejects images with import, TLS, or delay-import directories until their startup handling is implemented. The mapper only supports HIGHLOW base relocations. A successful map is still not permission to execute: physical allocation, page permissions, memory aliases, kernel globals, boot parameters, and hardware initialization remain prerequisites.

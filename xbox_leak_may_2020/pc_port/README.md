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

## Critical limitation

This proves only the standalone native PC harness and its early hardware probes. It does **not** load or boot the original Xbox kernel. The Xbox kernel still needs a verified PE32 loader/relocation/import path, its exact boot handoff, native replacement implementations for Xbox-specific HAL services, and integration of the NV2A translation layer. The existing `PC_COMPAT_GUEST` VMCALL adapter is a separate hypervisor-guest experiment; it is not used by this bare-metal harness.

Do not flash this test ELF to a physical disk or firmware. Test it in QEMU first; use removable media only after the boot image and hardware paths are validated.


## XboxRuntime code reuse: XBE parser

The portable `xbe_format.h/.cpp` parser and image-layout routine were copied from [rianalwis0v0-a11y/XboxRuntime](https://github.com/rianalwis0v0-a11y/XboxRuntime/tree/main/modules/xbe_loader). The source MIT license is preserved in `XBOXRUNTIME_LICENSE.txt`. This code validates XBE metadata and maps headers/sections into a caller-provided buffer; it does not perform disk I/O, relocation, kernel-thunk import resolution, TLS execution, or kernel entry.

CI compiles this parser as a 32-bit freestanding C++ translation unit. It is not linked into the boot harness yet: the next step is a physical-storage reader and a bounded allocator/memory-map reservation, then a verified XBE-to-kernel boot handoff. Windows-only runtime files and the Windows guest execution backend were intentionally not copied into the bare-metal kernel.

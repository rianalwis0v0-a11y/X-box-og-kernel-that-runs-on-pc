# PC-native Xbox kernel bootloader — BIOS milestone 1

This is our own PC boot path, separate from the Multiboot diagnostic harness.

## Current boot chain

1. Legacy PC BIOS loads the 512-byte boot sector at physical 0x7C00.
2. Stage 1 checks BIOS EDD support and reads 32 sectors from disk LBA 1 to 0000:8000.
3. Stage 2 enables A20, installs a flat 32-bit GDT, enters protected mode, and writes diagnostics to COM1.
4. Stage 2 halts deliberately. It does NOT load, relocate, initialize, or jump to xboxkrnl.exe.

The fixed stage-2 location and sector count are temporary bring-up choices. This is a raw BIOS disk image, not yet an ISO and not UEFI bootable.

## Build and test

Install NASM and GNU coreutils, then run:

    make

Output: build/xbox-pc-boot.img

With QEMU installed:

    qemu-system-i386 -drive format=raw,file=build/xbox-pc-boot.img -boot c -serial stdio

Expected serial output includes:
- PC Xbox bootloader: stage 2 entered protected mode.
- Kernel PE loading and Xbox handoff are NOT implemented yet.

## Is xboxkrnl.exe alone enough?

No. It is the kernel image, not a complete bootable disk. At minimum we need:

- A bootable PC disk image containing stage 1, stage 2, and the kernel payload.
- A known, matching kernel build and a validated PE32 image layout.
- A PC memory map and reserved ranges for loader, kernel, page tables, stacks, and devices.
- A loader for the image's actual storage format (raw PE versus encrypted/compressed Xbox ROM payload).
- A checked import/relocation policy and the kernel's expected initialized-data contents.
- A precisely verified entry ABI, load-options buffer, and cryptographic-key argument semantics.
- A PC HAL path for hardware services required during early initialization.

The original bldr32 path obtains the kernel from Xbox ROM, performs Xbox-specific decrypt/decompress and board setup, then calls the kernel entry with LoadOptions and XboxCryptKeys. A PC bootloader must not invent key bytes or pretend PC hardware is MCPX. The PC path must use an appropriately prepared kernel image and trace every early consumer of the key argument before calling the kernel.

## Not implemented yet

- FAT/ISO filesystem support (this image uses fixed LBAs).
- Kernel PE loader, relocations, imports, or section mapping.
- Xbox ROM .data restoration or firmware-derived state.
- Kernel entry invocation.
- PC HAL handoff, ACPI/APIC setup, or NV2A translation.
- UEFI boot.

This milestone is only a BIOS-to-protected-mode bootloader skeleton. It does not boot the Xbox kernel.

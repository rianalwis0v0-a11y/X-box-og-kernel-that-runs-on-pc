# Kernel payload and original Xbox boot animation

## What is present in this repository

The repository contains two identical-path copies of a 1 MiB recovery ROM payload:

- `xbox trunk/xbox/private/ntos/recovery/cdf_man/XBOXROM.BIN`
- `xbox trunk/xbox/private/ntos/recovery/cdf_man/RETAIL/XBOXROM.BIN`

Both tree entries resolve to the same Git blob (1,048,576 bytes). The repository does **not** currently contain a standalone `xboxkrnl.exe` payload.

The original loader source is present under `xbox trunk/xbox/private/ntos/bootx/bldr32`. Its `bldr32.c` calls `BldrCopyROMToRAM`, then reads the PE entry point from the decompressed image at `PsNtosImageBase`, and calls it with the boot command line and `XboxCryptKeys`.

## Why the ROM is not a ready-to-use kernel EXE

The original `romcpy.c` describes a multi-step image path:

1. Read the compressed-kernel size and uncompressed-data size from the ROM's `XBOOT_PARAM` trailer.
2. Locate the encrypted compressed-kernel bytes and the uncompressed kernel-data block.
3. Decrypt the compressed kernel in-place with RC4 using the third 16-byte key.
4. Decompress the Xbox LZX block stream to `PsNtosImageBase`.
5. Rebase the initialized-data pointer stored in the Xbox-specific DOS-header extension.

The checked-in `enckey.h` explicitly says its keys are **test keys intended for DevKits (xm2) only**. They must not be assumed to decrypt the checked-in ROM. The PC loader must not silently try these keys and claim success. A matching ROM/key set and verified image parameters are required before extracting or executing a kernel image.

The existing source is sufficient to specify the extraction algorithm, but the current PC bootloader does not yet implement the ROM parser, RC4/LZX pipeline, initialized-data restoration, or kernel handoff. No standalone `xboxkrnl.exe` has been extracted or added by this commit.

## Where the boot animation lives

The Xbox kernel reference documents the boot animation in the kernel's `INIT` section. That section is discarded after initialization. The original cold-boot code accesses Xbox graphics hardware directly before the normal display driver is available.

Therefore the correct sequence is:

1. Load the exact matching kernel image and restore its initialized `.data` state.
2. Initialize the PC HAL services needed by early kernel startup.
3. Adapt the early Xbox NV2A register/pushbuffer path to the PC GPU translation backend.
4. Preserve the kernel `INIT` section until the animation routine has completed.
5. Continue kernel startup and allow normal graphics-driver initialization to take over.

A generic VGA text splash or a newly drawn green logo is not the original Xbox boot animation. Do not label either as the original animation. The current bootloader has no animation renderer.

## Current limitations

- No verified kernel EXE payload.
- No ROM decryption/decompression implementation in the PC loader.
- No validated initialized-`.data` restoration.
- No full PC APIC/IOAPIC HAL path.
- No NV2A method decoder connected to a host GPU backend.
- No execution of the original `INIT` animation routine.
- No bare-metal Xbox-kernel boot test.

This file records source-derived requirements; it does not claim that the Xbox kernel or its animation boots on a PC.

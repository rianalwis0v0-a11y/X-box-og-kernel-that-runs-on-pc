# PC boot harness (isolated from Xbox kernel)

This directory is the first executable milestone for the native-PC port. It is intentionally independent of the leaked kernel: it proves the PC firmware/bootloader-to-32-bit-kernel path before any Xbox binary is loaded.

## Target

- x86 32-bit protected mode
- GRUB 2 Multiboot v1 loader
- serial diagnostics on COM1 (0x3F8)
- QEMU/Bochs-compatible first test target

## Build

Requires NASM, an i686-elf GCC/binutils toolchain, and GRUB tooling. Run `make` here. The output is a standalone harness ELF, not an Xbox kernel image.

## Test

Boot the ELF with a Multiboot-capable GRUB image in QEMU. Expected serial output:

```
PC-HARNESS: entered 32-bit kernel
PC-HARNESS: serial PASS
PC-HARNESS: halting (Xbox kernel not loaded)
```

The harness must not claim Xbox hardware is initialized. No NV2A, MCPX, SMBus, or Xbox interrupt-controller emulation is provided here.

## Next integration gate

Do not add a kernel loader until Stage 0 in ../PC_PORTING.md identifies the source image format, entry point, ABI, relocation model, and required CPU features from the actual source/build artifacts.


## PE32 kernel-image inspection (validation only)

The first kernel-integration tool is `pe32_inspect.py`. It accepts an i386 PE32 image and prints a JSON report containing the preferred image base, entry RVA/VA, section map, imports directory and relocation directory.

```sh
python3 pe32_inspect.py /path/to/kernel-image.exe
python3 -m unittest discover -s . -p 'test_*.py' -v
```

The inspector rejects malformed/truncated headers, non-i386 or non-PE32 images, invalid section ranges, overlapping virtual sections, and entry points outside executable sections. It never loads or executes the image. The GitHub Actions workflow runs its unit tests before building and boot-testing the separate Multiboot harness.

Do not supply proprietary Xbox keys or try to run an image based only on a successful inspection report. Loading and execution require a separately reviewed relocation, memory-map, load-options and kernel-entry contract.

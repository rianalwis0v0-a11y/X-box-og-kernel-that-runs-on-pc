# Source inventory: Xbox trunk and CPXXUPD

Inspected on 2026-11-23 from the repository's Git tree and raw file contents.

## Confirmed source layout

The directory `xbox_leak_may_2020/xbox trunk/xbox/private/ntos` contains distinct kernel subsystems, including:

- `init` — kernel initialization and image/export metadata;
- `halx/i386` — Xbox HAL's x86-specific implementation;
- `bootx` — boot support, including `bldr32` and `preldr32`;
- `ex`, `ke`, `mm`, `ob`, `ps`, `io`, `idex`, `fatx`, `gdfx` — executive, scheduler, memory manager, object/process, I/O, storage, and filesystem components.

The separate `CPXXUPD/private/ntos/init/sources.inc` is an update/build fragment, not a complete kernel source tree. It lists `xboxkrnl.c`, `init.c`, and `fatal.cpp`, and links against prebuilt Xbox libraries such as `halx.lib`, `ex.lib`, `ke.lib`, `mmcon.lib`, `io.lib`, `idexcon.lib`, `fatx.lib`, and `ldrcon.lib`. It is therefore not independently buildable as a PC kernel.

The Xbox-trunk `init/sources.inc` likewise describes a library target and uses the legacy Microsoft `SOURCES` build system, Microsoft compiler switches, and Xbox-specific libraries. Its `ntoskrnl.src` is an export-definition file for `xboxkrnl.exe`; the export list includes Xbox HAL interfaces such as SMBus and PCI access. These facts are direct evidence that a stock GCC/ELF build is not a drop-in replacement for the original build.

## Porting implications

1. The source is not merely an NT kernel with a replaceable boot sector. The Xbox HAL and platform-specific initialization are part of the kernel's expected environment.
2. The repository contains original Xbox bootloader components. Inspect their entry contracts and image-loading path before designing a replacement loader.
3. Keep the PC harness isolated. It is a test platform for CPU/firmware entry and diagnostics, not a claim that the Xbox kernel can already be linked into it.
4. Next source-analysis targets are `bootx/bldr32`, `bootx/preldr32`, `halx/i386`, and the kernel image/link rules. Determine exact file names and entry points from those directories before adapting anything.
5. A viable native PC port needs a PC HAL implementation for every required Xbox HAL service, plus explicit decisions for Xbox-only services. Returning success from fake stubs is prohibited.

## Evidence and limitations

This inventory is based on repository directory listings and the complete visible contents of the Xbox-trunk `init/sources.inc`, `init/ntoskrnl.src`, and CPXXUPD `init/sources.inc`. The recursive Xbox-trunk tree endpoint reports truncation at 63,683 entries; this note does not claim that the entire tree has been exhaustively enumerated.

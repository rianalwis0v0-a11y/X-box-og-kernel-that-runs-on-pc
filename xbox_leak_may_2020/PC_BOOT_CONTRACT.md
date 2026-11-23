# Xbox boot-chain contract findings for a native PC port

Status: source-inspection milestone; no Xbox kernel image has been loaded or executed.

## 1. The actual Xbox handoff

The Xbox boot chain is not a generic Multiboot handoff.

### Preloader

`bootx/preldr32/preldr32.c` derives ROM locations from data stored at the top of the address space, validates the ROM digest/signature, decrypts the bootloader, and obtains its entry address from a value stored near the end of the decrypted block. It reads the encrypted bootloader from the ROM window near `0xFFFFFE00`.

Consequences:
- The original preloader cannot be used as a PC boot stage: a normal PC has no Xbox ROM window, MCPX-derived key material, or matching ROM layout.
- The PC port must load a separately supplied, already-decrypted/authorized kernel image through a PC loader path. It must not imitate ROM validation with dummy data.

### 32-bit Xbox bootloader

Files inspected:
- `bootx/bldr32/sources`
- `bootx/bldr32/i386/startup.asm`
- `bootx/bldr32/i386/bldr32.c`
- `bootx/bldr32/i386/bldr32.h`

The source manifest identifies `bldr32` as a native 32-bit program with absolute entry symbol `BldrStartup`, linked with retained relocations and 0x20 alignment.

The assembly startup is already in 32-bit protected mode. It:
1. Changes cache/MTRR policy using privileged MSR operations.
2. Establishes segment registers and a stack at `BLDR_RELOCATED_ORIGIN`.
3. Relocates the bootloader.
4. Builds page tables with identity mappings for the first 256 MiB and a corresponding mapping at `0x80000000`, plus mappings for the page directory, ROM window and Xbox GPU register window.
5. Enables PSE, FXSR and XMM exception support, loads CR3, and enables paging/write-protect.
6. Calls `BldrStartup2` using the relocated address.

The C startup then performs Xbox board setup (including ROM-derived drive/slew values, LDT, USB ASRC and MCPX PCI configuration), decrypts/decompresses the kernel from ROM into RAM, validates the PE NT headers, computes:
`PsNtosImageBase + NtHeader->OptionalHeader.AddressOfEntryPoint`,
and invokes the kernel as:
`int (__cdecl *)(PUCHAR LoadOptions, const UCHAR *CryptKeys)`.

That is the strongest currently verified kernel entry contract. The actual image base, entry RVA, section layout, relocation requirements and exact boot parameter/key ABI must still be extracted from the matching kernel image/build artifacts before execution.

## 2. Kernel/HAL dependencies

The kernel initialization manifest `init/sources.inc` builds a library, not a complete standalone kernel. It links the init component against executive, HAL, memory manager, scheduler, object/process, I/O, storage, filesystem, loader, crypto and other Xbox libraries.

The inspected `init/init.c` calls `HalInitSystemPhase0` (or quick-boot variant) before `MmInitSystem`, then initializes object and process managers. Later initialization connects Xbox-specific clock, profile, SMBus and SCI interrupt vectors.

The inspected `halx/i386/xxhal.c` is not a generic PC HAL. It initializes the 8259 PIC and Xbox RTC/clock paths, configures Xbox southbridge PCI devices, and initializes the MCPX/SMC system-control interrupt. Other files in `halx/i386` include Xbox-specific SMBus/SCI, PCI-space, Super-I/O and interrupt code.

## 3. PC loader design (next implementation)

Keep the existing Multiboot harness as a separate test target. It proves only that the PC can enter a 32-bit freestanding program and print diagnostics.

The next target should be a second-stage **PE32 image inspection and load-contract tool**, not a blind jump:
- Parse DOS and PE32 headers with bounds checks.
- Require i386 machine type and PE32 optional-header magic.
- Validate `SizeOfHeaders`, `SizeOfImage`, section-table bounds, section raw-data bounds, virtual ranges, and entry RVA.
- Report the preferred image base, entry RVA, section map, data directories, relocation directory, imports and required alignment.
- Initially perform validation only. Do not map or execute the kernel.
- Later, implement a controlled PE32 loader with checked relocations and an explicit memory-map reservation policy.
- Only after the handoff ABI and all early kernel dependencies are known should the loader call the kernel entry with a documented load-options buffer and key buffer. Do not fabricate Xbox cryptographic keys or report Xbox security services as initialized.

## 4. Explicit non-goals

- Do not patch the leaked Xbox bootloader to pretend PC hardware is MCPX.
- Do not write Xbox GPU registers on a PC.
- Do not claim the kernel has booted when only the independent Multiboot harness has booted.
- Do not return success from unimplemented HAL services.
- Do not overwrite the original source tree or binary inputs.

## 5. Evidence required to advance

For every milestone, record the source file and symbol, image hash, exact toolchain, build/test command, full output, and VM/hardware configuration. The first kernel-integration milestone is a PE32 validation report for a known kernel image, not kernel execution.


## 6. PC-side implementation status for the original bldr32 paging layout

The original startup assembly's first-stage layout is now represented by `pc_paging.c`: 256 MiB of identity mapping plus a second mapping of the same physical range at virtual `0x80000000`. The PC harness allocates the page directory and 128 page tables from its Multiboot-backed physical allocator and builds these entries using 4 KiB pages.

This is intentionally not activated yet. The original bldr32 also maps Xbox ROM and NV2A register windows and enables PSE/FXSR/XMM support. A PC port must replace those device windows with PC/monitor-owned mappings and must not map host devices as if they were MCPX or NV2A. The builder currently creates only the low-memory and high-alias mappings; it does not execute the Xbox kernel.

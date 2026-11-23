# Xbox boot handoff findings

Source examined:
- xbox trunk/xbox/private/ntos/bootx/bldr32/i386/startup.asm
- xbox trunk/xbox/private/ntos/bootx/bldr32/i386/bldr32.c
- xbox trunk/xbox/private/ntos/bootx/bldr32/sources
- xbox trunk/xbox/private/ntos/bootx/preldr32/preldr32.c

## Confirmed original handoff

The original BldrStartup entry expects 32-bit protected mode. Its startup code performs platform-level CPU and memory setup before calling BldrStartup2:

- changes CR0 cache policy and executes WBINVD;
- programs variable MTRRs and PAT;
- loads the expected segment selectors and stack;
- relocates the bootloader;
- creates large-page identity mappings for the first 256 MiB at both low and 0x80000000 virtual addresses;
- maps the page-directory page at 0xC0000000;
- maps ROM at 0xFFC00000 and Xbox GPU register space as uncached;
- enables PSE, FXSR, XMM exception support, paging, and write-protect.

BldrStartup2 then performs Xbox-specific board setup (including LDT/USB and, for applicable builds, ROM/SMC actions), decrypts and decompresses the kernel from ROM into RAM, parses its PE NT headers, computes the entry as image base plus AddressOfEntryPoint, and calls the kernel entry with two arguments: LoadOptions and XboxCryptKeys.

The bldr32 SOURCES manifest identifies the target as a native subsystem program with absolute entry BldrStartup, built using Microsoft's legacy build environment and Xbox crypto/loader libraries.

## PC port design consequence

A PC loader must reproduce the *kernel-required contract*, not the Xbox board initialization sequence. Do not copy the Xbox MTRR/PAT values, physical mappings, ROM accesses, or MCP PCI writes blindly. A PC port should instead:

1. load a separately prepared kernel PE image into a safe PC physical-memory range;
2. validate PE headers and every section/relocation/import range before use;
3. create page tables from the PC memory map and reserve loader/kernel regions;
4. establish compatible 32-bit protected-mode CPU state and stack;
5. pass a documented PC boot-parameter structure and a PC HAL configuration to a dedicated PC kernel entry shim;
6. initialize PC interrupt/timer/memory services before allowing the NT-derived kernel to depend on them.

The existing Xbox entry point takes XboxCryptKeys as its second argument. A PC port must not pass a fake key pointer and must not call the unmodified entry until the role of that argument and all early consumers have been traced in the kernel startup code.

## Current implementation boundary

The separate pc_port harness currently demonstrates only a Multiboot-to-32-bit-C entry and serial diagnostics. It does not load a PE image, call BldrStartup, or invoke xboxkrnl.exe.

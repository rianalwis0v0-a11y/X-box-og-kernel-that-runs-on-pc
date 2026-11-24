# Native PC bare-metal bring-up harness

This is a real ring-0 x86 test kernel, booted directly by a Multiboot v1 bootloader. It does not run as a Windows/Linux process and does not require VT-x, AMD-V, or VMCALL. GRUB is only the bootloader; after handoff, the harness executes privileged CPU and PCI I/O instructions directly.

## Original Xbox startup animation

The placeholder VGA text-mode logo/spinner has been removed from the PC harness. The leaked Xbox startup animation implementation is in `xbox trunk/xbox/private/ntos/ani2`: `animate.c` starts the animation on an NT system thread, while `renderer.cpp`, `scene_renderer.cpp`, `logo_renderer.cpp`, and the shader/geometry assets implement the animated Xbox logo. `mslogo.cpp` handles the later Microsoft-logo stage, and `stboot.c` contains the boot sound sequencer.

This code is **not yet executable in the PC harness**. It expects Xbox NT kernel thread/event/memory services, the Xbox D3D8/XGraphics interfaces, NV2A rendering, and boot-time resource setup. The PC port must supply those interfaces (including the NV2A-to-PC-GPU translation backend) before it can start the authentic animation. The harness intentionally shows no substitute animation.

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

The harness accepts an `xboxkrnl.exe` Multiboot module and validates its PE32/i386 headers, entry RVA, image/header sizes, section table, and section raw/virtual ranges. The standalone PE32 mapper copies headers/sections into caller-provided memory and applies HIGHLOW base relocations. It rejects images with import, TLS, or delay-import directories because their startup handling is not implemented. Pass the module to GRUB with the module name exactly `xboxkrnl.exe`.

This remains a loader milestone, not a kernel launch. The page-table builder is tested but paging is not enabled, physical memory is not allocated/reserved, Xbox virtual aliases are not established, and the entry is not called. The source-defined entry is `__cdecl main(LoadOptions, CryptKeys)`; valid load-options and original crypt-key buffers must be supplied. The original Xbox `preldr32` depends on Xbox ROM/MCPX key material and cannot run unchanged on a PC.

## Native PC HAL primitives (new foundation)

`pc_hal_native.c/.h` adds direct ring-0 x86 I/O primitives, a dual-8259 PIC remap to the Xbox HAL's existing vector base (0x30/0x38), and PCI Configuration Mechanism #1 reads through 0xCF8/0xCFC. The PIC is left fully masked; these routines do not install an IDT, enable an IRQ, or start a clock. PCI access preserves the caller's interrupt-enable state.

This is a native hardware path, not a VMCALL adapter. It is currently a separately compiled porting module: it has **not** yet replaced the Xbox HAL's initialization call sites or been built with the original Xbox NT DDK toolchain. The PCI config path is only the legacy mechanism #1 256-byte configuration space; ACPI discovery and staged APIC MMIO mapping are described below, while APIC interrupt routing, timer calibration, DMA, storage, and full kernel interrupt integration remain unfinished. The module must run only at ring 0 with a valid x86 environment.

## Native PC HAL ABI backend

The freestanding harness now instantiates and validates the version-1 `pc_hal_ops` contract at kernel startup. Its native backend provides PCI device lookup and 32-bit configuration-space reads/writes through the ring-0 CF8/CFC path, plus a legacy PIC/PIT IRQ0 callback, IRQ0 mask control, and a PIT-derived monotonic clock/sleep service. The QEMU boot workflow asserts that the HAL ABI contract validates during startup.

MMIO mapping deliberately returns `PC_STATUS_UNSUPPORTED`: the page-table builder can construct cache-disabled mappings, but the kernel has not yet installed a general MMIO mapper or activated paging. The current interrupt backend is legacy PIC/PIT only; it does not claim general GSI/APIC routing. This is a real, limited native backend rather than the test-only callback stubs, but it is not yet a complete Xbox HAL implementation.

## Critical limitation

This proves only the standalone native PC harness and its early hardware probes. It does **not** load or boot the original Xbox kernel. The Xbox kernel still needs a verified PE32 loader/relocation/import path, its exact boot handoff, native replacement implementations for Xbox-specific HAL services, and integration of the NV2A translation layer. The existing `PC_COMPAT_GUEST` VMCALL adapter is a separate hypervisor-guest experiment; it is not used by this bare-metal harness.

Do not flash this test ELF to a physical disk or firmware. Test it in QEMU first; use removable media only after the boot image and hardware paths are validated.


## XboxRuntime code reuse: XBE parser

The portable `xbe_format.h/.cpp` parser and image-layout routine were copied from [rianalwis0v0-a11y/XboxRuntime](https://github.com/rianalwis0v0-a11y/XboxRuntime/tree/main/modules/xbe_loader). The source MIT license is preserved in `XBOXRUNTIME_LICENSE.txt`. This code validates XBE metadata and maps headers/sections into a caller-provided buffer; it does not perform disk I/O, relocation, kernel-thunk import resolution, TLS execution, or kernel entry.

CI compiles this parser as a 32-bit freestanding C++ translation unit. It is not linked into the boot harness yet: the next step is a physical-storage reader and a bounded allocator/memory-map reservation, then a verified XBE-to-kernel boot handoff. Windows-only runtime files and the Windows guest execution backend were intentionally not copied into the bare-metal kernel.


## Kernel entry and unsupported PE startup features

The source-defined Xbox kernel entry is `__cdecl main(PUCHAR LoadOptions, const UCHAR *CryptKeys)`; see `xbox_pc_kernel_entry` in `xbox_pc_boot.h`. The port must provide valid load-options and crypt-key buffers before invoking it. It must not invent EEPROM/certificate key contents.

The PE32 validator rejects images with import, TLS, or delay-import directories until their startup handling is implemented. The mapper only supports HIGHLOW base relocations. A successful map is still not permission to execute: physical allocation, page permissions, memory aliases, kernel globals, boot parameters, and hardware initialization remain prerequisites.


## PC page-table builder

`pc_paging.c/.h` implements a non-PAE 32-bit 4 KiB page-table builder over caller-provided, aligned directory and table storage. It supports explicit virtual-to-physical mappings and supervisor/user plus read-write flags. The CI tests verify translation, permission flags, conflict rejection, alignment/overflow checks, and atomic failure when page-table capacity is insufficient. It does not allocate physical memory or activate CR0.PG.


## PE32 loader hardening and physical-page allocator

The PE32 validator now checks power-of-two file/section alignment, header/image alignment, aligned section addresses and raw sizes, and rejects overlapping virtual or file-backed section ranges. Synthetic malformed-image tests cover these cases.

pc_physmem.c/.h adds a caller-bitmap-backed 4 KiB page allocator for physical addresses below 4 GiB. It imports the variable-length Multiboot memory map, admits only complete pages inside usable RAM, gives overlapping reserved entries priority, supports explicit unaligned reservations, and never allocates page zero. Its native tests and a freestanding 32-bit compile check run in CI.

This allocator is not yet wired into the boot harness. The boot code must reserve its own image/stack, Multiboot information and module buffers, page-table storage, and device/firmware ranges before using it.


## Physical allocator in the boot harness

The Multiboot harness now links the physical allocator and exercises it after reserving the complete kernel image, the Multiboot v1 information block and memory map, command-line storage, module descriptors/payloads/names, low memory, VGA, and local/IO APIC windows. It allocates one page and reports its physical address over serial. The linker exports kernel start/end bounds so the full image (including stack and allocator bitmap) is reserved.

This is still a reservation/allocator self-test. The allocated page is not yet used as a page table; paging remains disabled until all mappings and the Xbox address-space contract are verified.


## Physical memory and Xbox page-table layout

The harness now initializes a 4 GiB-bounded physical-page bitmap from the Multiboot memory map, reserving the kernel, boot metadata, modules, low memory, and conventional MMIO windows. It allocates contiguous storage for one page directory and 128 page tables, then builds the source-verified 256 MiB identity mapping and the 0x80000000 alias with 4 KiB pages. Paging deliberately remains disabled; this validates the layout without jumping into the Xbox kernel or assuming PC MMIO is Xbox hardware.


The current GitHub Actions run [45186680707](https://github.com/rianalwis0v0-a11y/X-box-og-kernel-that-runs-on-pc/actions/runs/45186680707) passed end-to-end: native allocator tests, freestanding allocator compilation, page-table builder tests, 32-bit harness build, Multiboot validation, and QEMU serial checks for physical allocation and the Xbox 256 MiB identity/high-alias table layout. Paging remains off and the Xbox kernel is not executed.


## QEMU-tested Xbox memory-layout preparation

The boot harness now allocates a contiguous 129-page block for a page directory and 128 page tables. It builds a 256 MiB identity map and a matching alias at 0x80000000 based on the inspected bldr32 startup layout, then verifies representative identity and alias translations. The tables remain inactive: CR0.PG is not set and CR3 is not loaded. QEMU CI has passed the allocator and page-table build milestone.

The page-table builder rejects overlapping physical or virtual backing storage. The next work is to validate the full Xbox virtual-memory contract, add PCI/firmware MMIO mappings and page attributes, and only then consider enabling paging.


## PCI BAR resource decoding

`pc_pci.c/.h` decodes the assigned Base Address Registers of a PCI function into I/O or memory resources, including 64-bit memory BAR pairs and prefetchability. It is intentionally read-only: it does not probe BAR sizes by writing all-ones, alter command-register decode bits, or assume that firmware-assigned addresses are safe to map. Unit tests cover I/O, 32-bit and 64-bit memory BARs, unassigned slots, reserved encodings, truncated 64-bit pairs, and output-capacity errors. The native harness now reads each enumerated function's assigned BAR registers and reports decoded I/O and memory bases over serial. It uses each function's PCI header type to select the valid BAR count and handles 64-bit BAR pairs. This does not size or assign BARs, resolve firmware resource conflicts, enable device decoding, or map MMIO; those operations remain future work.


## Read-only PCI BAR resource discovery

The kernel now passes the six type-0 or two type-1 BAR register values through `PcPciDecodeBars`. It reports decoded I/O or memory BAR base addresses, 32/64-bit width, original BAR slot, and prefetchability. A 64-bit BAR consumes its following register. Unknown header layouts and malformed BAR encodings are not guessed.

This is discovery only: the harness does not size BARs by writing all-ones, change PCI command bits, claim resources, map device MMIO, enable bus mastering, or route device interrupts. The next HAL stage is to reserve these decoded ranges against RAM, build cache-correct MMIO mappings, and only then initialize individual devices.


## MMIO page attributes

The 32-bit page-table builder now preserves the x86 PWT and PCD leaf-entry flags in addition to write and user permissions. This provides the page-table primitive needed to map device MMIO with cache-control attributes; a native unit test maps a representative APIC page with both flags and verifies the translated physical address and flags. It does not yet establish a complete PAT/MTRR memory-type policy, map firmware-discovered BARs, or enable paging.

## Staged local APIC and I/O APIC driver

`pc_apic.c/.h` now provides a staged xAPIC backend: it enables the BSP local APIC through IA32_APIC_BASE, masks local LVT sources, attaches caller-mapped I/O APIC windows using MADT GSI bases, routes a GSI with ACPI polarity/trigger flags, and issues local-APIC EOI. The LAPIC physical address and mapped virtual address are separate parameters; the caller must establish uncached MMIO mappings before calling these routines. x2APIC mode is rejected rather than mis-handled.

The driver now checks CPUID APIC capability before enabling the local APIC, provides a mask-all operation for every I/O APIC redirection entry, and masks all I/O APIC pins when an I/O APIC is attached so firmware routing is never inherited accidentally. The IDT also installs a dedicated 0xFF local-APIC spurious-vector stub (which correctly does not send EOI). These are safety prerequisites, not proof of a complete APIC interrupt path. The boot harness now retains the parsed MADT, reserves the firmware-reported LAPIC/IOAPIC physical pages, and adds identity mappings for those pages with PWT+PCD cache-disable flags to the prepared page tables. QEMU CI checks the translations and attributes. Paging remains OFF, so these mappings are only page-table data and are not dereferenced. The APIC driver is still not activated by `kernel_main`: paging activation, APIC-vs-PIC selection, PIT GSI routing, and runtime APIC interrupt testing remain pending. The existing PIT self-test continues using the legacy PIC. No claim of APIC hardware testing is made.

## Firmware ACPI root-table discovery

The early PC harness now walks the firmware RSDT (or XSDT when its address is representable in the 32-bit port), validates the root SDT checksum and entry layout, locates an APIC/MADT table, and reports the local-APIC address, I/O APIC addresses/GSI bases, and interrupt-source-override count over serial. The root walker has an injectable physical-memory mapper and unit coverage for successful RSDT-to-MADT discovery and checksum rejection. The boot harness uses a conservative identity-mapped 32-bit physical reader while paging is disabled.

Discovery is not activation: the harness still does not map APIC MMIO with a proven uncached memory type, select APIC mode, route the PIT through an I/O APIC, or switch the IRQ dispatcher to local-APIC EOI. APIC hardware remains disabled. A separate GitHub Actions job now compiles the 32-bit freestanding harness in addition to the ACPI parser sanitizer tests.


## XboxRuntime NV2A code reuse: freestanding shader validation

The freestanding `nv2a_vertex_shader.c/.h` port is derived from the MIT-licensed
`modules/nv2a/nv2a_vertex_shader.c/.h` in XboxRuntime. It validates NV2A
four-DWORD vertex microcode and extracts input/output/constant-register usage.
The code has no Win32, D3D, heap, or host-process dependency and is compiled
into the ring-0 harness. Startup runs a terminated-NOP microcode self-test.

This is deliberately only the portable validation/metadata stage. It is not
the XboxRuntime D3D11 HLSL compiler, does not execute shaders, and does not
submit work to a physical GPU. The full XboxRuntime bridge currently depends
on Windows/D3D8/D3D11 and cannot be copied wholesale into a bare-metal kernel.

# Native PC Port: Engineering Plan

This document tracks the work required to boot the original Xbox kernel on ordinary x86 PCs. It is deliberately separate from the leaked source: do not modify leaked files until their provenance, build system, target architecture, and licensing are understood.

## Goal and acceptance criteria

Boot the Xbox kernel natively on x86 hardware or a PC virtual machine, without emulating the Xbox CPU. Xbox-specific devices may initially be absent; the first milestone is reaching a controlled kernel diagnostic halt, not running games.

A milestone is complete only when its test output, build command, compiler version, and commit are recorded.

## Stage 0 — source and toolchain inventory

- [x] Identify the principal kernel, bootloader, HAL, and build-manifest directories. The full 63,683-entry recursive tree is truncated, so exhaustive inventory remains open.
- [ ] Record source provenance and licensing. Do not assume a directory name proves that files are an authentic Microsoft leak.
- [x] Confirm the kernel/HAL target is x86 and the source build uses Microsoft's legacy SOURCES build system and Microsoft linker flags. Exact kernel image entry and ABI still need inspection.
- [ ] Separate source files from binaries and generated artifacts.
- [ ] Establish reproducible hashes for the untouched input tree.

## Stage 1 — PC boot entry

Implement a small, independent x86 boot harness. It must not overwrite or patch the original kernel image.

- [x] Select a first test path: Multiboot v1 via GRUB/QEMU, 32-bit protected mode. This validates the PC harness only; it is not the original Xbox firmware path.
- [x] Confirm the Xbox HAL source is x86-specific; the isolated harness enters 32-bit protected mode.
- [x] Install an IDT with fail-stop handlers for CPU exception vectors 0-31 in the independent PC harness.\n- [x] Install a flat 32-bit GDT and reload CS/data segments in the independent PC harness.\n- [x] Report fatal exception vector, error code, and EIP over serial.\n- [x] Add 8259 IRQ stubs, mask all IRQs except IRQ0, and run a PIT tick self-test.
- [ ] Add IOAPIC/APIC support and robust spurious IRQ handling.
- [x] Capture CPU vendor and parse the Multiboot physical memory map.
- [x] Provide serial-console diagnostics and a deterministic fatal halt.
- [x] Add a QEMU boot test that checks the harness serial output; latest CI result still needs confirmation.

## Stage 2 — kernel image contract

- [ ] Determine the kernel image format and entry-point contract from source/build metadata.
- [ ] Verify section alignment, relocations, imports, symbols, and expected load address.
- [ ] Implement only the required loader/relocation behavior, with bounds checks.
- [ ] Confirm that the entry point can be reached and report its first diagnostic.
- [ ] Do not guess Xbox memory addresses or silently map PC memory over them.

## Stage 3 — hardware abstraction

Create a PC-specific HAL adapter rather than pretending PC devices are Xbox devices.

Initial PC facilities:
- serial output for diagnostics;
- physical memory discovery and page-table setup;
- interrupt-controller setup (PIC initially if appropriate, then IOAPIC/APIC);
- timer source;
- keyboard input only after interrupt handling is stable.

Xbox-specific facilities requiring explicit implementations or stubs include NV2A, MCPX, SMBus, Xbox IDE/DMA behavior, and console-specific interrupt routing. A stub must return a documented unsupported status; it must never fake successful hardware initialization.

## Stage 4 — validation ladder

1. Boot harness prints a serial banner.
2. CPU mode, GDT, IDT, and exception path pass self-tests.
3. Memory map is parsed and reserved regions are respected.
4. Kernel image is validated and loaded without executing it.
5. Kernel entry is invoked and emits a diagnostic (or reaches a captured exception).
6. Required kernel subsystems are brought up one at a time.
7. Xbox-specific hardware support is added and tested independently.

## Required evidence for each change

Record:
- exact build command and toolchain versions;
- test command and complete serial/log output;
- expected versus actual result;
- hardware/VM configuration;
- known limitations.

## Current status

Source analysis has confirmed these concrete facts:

- The Xbox kernel source is under xbox trunk/xbox/private/ntos; init/sources.inc builds an init library, and init/ntoskrnl.src defines the xboxkrnl.exe export table.
- The Xbox HAL has an x86 implementation under halx/i386. xxhal.c initializes the 8259 PIC, RTC, clock, PCI southbridge devices, and system-control interrupt; it also disables the local APIC in its Phase 0 path. halp.h includes Xbox-specific PCI and SMC definitions.
- halx/sources lists the original assembly and C implementation files, including interrupt, clock, PCI, SMBus/SCI, and Super-I/O code.
- The boot tree contains bootx/preldr32 and bootx/bldr32. preldr32.c verifies ROM contents/signature, decrypts the bootloader, and derives its entry address from ROM-specific data. That flow cannot be used unchanged on a PC with no Xbox ROM/MCPX.
- CPXXUPD/private/ntos/init/sources.inc is only an update component manifest and links against prebuilt Xbox libraries; it is not a complete independent kernel build.

The independent PC harness, Makefile, linker script, serial diagnostics, GDT, exception IDT, and QEMU CI workflow are committed under pc_port/. The latest commits add a flat GDT, fatal exception diagnostics, 8259 IRQ stubs, and a PIT IRQ0 self-test. The GitHub Actions run must pass before these changes are considered tested; no passing run has been confirmed in this session.

Next engineering task: inspect bootx/bldr32 startup/loader contract and the halx/i386 initialization dependencies, then define the smallest PC loader-to-kernel handoff. Do not call the original Xbox preloader on a PC: its ROM signature/decryption and fixed-address assumptions are platform-specific.


## Stage 1 result and Stage 2 contract findings (2026-11-23)

- [x] Inspect the original `bootx/bldr32` source manifest and 32-bit startup assembly.
- [x] Identify the C bootloader's kernel-entry call contract.
- [x] Document why the original ROM preloader is not a PC boot path.
- [x] Document Xbox-specific early HAL dependencies and the PC loader design.

See [PC_BOOT_CONTRACT.md](PC_BOOT_CONTRACT.md) for source-level evidence and the detailed handoff analysis.

Verified from `bldr32.c`: the kernel is called as a cdecl function taking two arguments, `LoadOptions` and `CryptKeys`. The entry address is calculated from the PE NT header's `AddressOfEntryPoint` plus `PsNtosImageBase`. The Xbox startup assembly establishes paging and Xbox-specific physical mappings before that call. A Multiboot entry is therefore not a substitute for the Xbox bootloader contract.

Next implementation: add a bounded PE32 inspection utility and tests. It must validate the actual kernel image format and section/relocation metadata without executing the image. Kernel execution remains gated on a complete memory/CPU/HAL handoff contract.


### Stage 2 progress

- [x] Add a standalone, bounds-checked PE32/i386 inspection utility at `pc_port/pe32_inspect.py`.
- [x] Add synthetic valid/malformed PE32 unit tests at `pc_port/test_pe32_inspect.py`.
- [x] Run the inspector tests in GitHub Actions before the QEMU harness job.
- [ ] Run the inspector against a legally available, matching Xbox kernel image and archive its report/hash.
- [ ] Determine the actual relocation/import requirements and memory layout from that image.
- [ ] Implement a loader only after the above contract is verified.

The synthetic tests validate parser behavior; they are not evidence that the Xbox kernel itself has been parsed or booted.


### Stage 3 progress — PC HAL and NV2A host boundary

- [x] Define a versioned host-PC HAL contract for MMIO mapping, PCI configuration, interrupt routing and monotonic timers in `pc_port/include/pc_hal.h`.
- [x] Define a renderer callback contract for surfaces, shader translation, indexed draws and presentation in `pc_port/include/nv2a_pc_bridge.h`.
- [x] Preserve the existing fixed-width guest/host shared-memory ABI in `pc_port/nv2a_bridge.h`; it is the transport protocol, not the renderer itself.
- [x] Add bounds/sequence validation for the shared-memory header, commands and pushbuffer ranges in `pc_port/nv2a_transport.c`.
- [x] Add compile/run tests for the ABI validators and register them in the PC harness workflow.
- [ ] Implement the command-ring consumer and actual NV2A pushbuffer/method decoder.
- [ ] Connect the decoder to the existing NV2A register/state model and shader translator.
- [ ] Implement a real host renderer backend (D3D or Vulkan) and verify clear → triangle → present.
- [x] Add a `PC_COMPAT_GUEST` HAL path that delegates platform discovery and interrupt registration through the PC shared-memory/VMCALL ABI; bypass Xbox MCPX/LPC/8259/RTC/southbridge setup in that build.
- [ ] Connect the PC boot handoff to `PcHalBindSharedWindow` and implement the host VMCALL dispatcher, PCI/memory/timer responses, and clock interrupt injection.

Important distinction: these commits define and test the PC-facing contracts and reject malformed shared-memory submissions. They do not yet translate NV2A methods into GPU commands, provide a working D3D/Vulkan renderer, or boot the Xbox kernel. The Xbox kernel's existing hardware initialization must not be called against generic PC hardware.


### PC HAL implementation progress (2026-11-23)

The existing Xbox HAL source now has a compile-time `PC_COMPAT_GUEST` path:

- `halx/i386/xxhal.c` skips Xbox APIC-disable, 8259 PIC, RTC, Xbox clock programming, MCPX southbridge setup, SMC/SMBus and SCI initialization in the PC guest path.
- The PC path requires the boot handoff to call `PcHalBindSharedWindow(mapped_window, mapped_window_bytes, call_record_offset)` before `HalInitSystemPhase0`. The guest adapter issues the versioned `pc_platform_abi.h` VMCALL contract using only a registered shared-window offset; it does not pass guest pointers to the host.
- The adapter validates the monitor handshake, requests the PC memory map, virtual PCI enumeration and monotonic timer information, then registers the kernel clock vector with the monitor in Phase 1.
- `halx/i386/pcisetup.c` no longer programs Xbox MCPX/LPC/IDE/NIC/USB/audio/AGP PCI registers when built with `PC_COMPAT_GUEST`.
- `halx/sources` includes the guest adapter source and its include directory.

This is a real source-level HAL split, but it is not yet a complete working PC HAL. The PC bootloader still must map/register the shared window and call `PcHalBindSharedWindow`; the host monitor must implement the VMCALL dispatcher, PCI/memory/timer responses, and clock interrupt injection. The original Xbox build remains unchanged when `PC_COMPAT_GUEST` is not defined; select the PC HAL with `PC_COMPAT_GUEST=1` in the legacy SOURCES build environment. The shared-window binding includes its mapped byte length, checks offset alignment and bounds, and the response header carries a bounded result count. No Xbox kernel boot or real-hardware test has been claimed.


### Native bare-metal PC probe milestone (2026-11-23)

- [x] Extend the standalone Multiboot v1 test kernel with ring-0 CPUID vendor detection.
- [x] Parse the bootloader-supplied physical memory map with record-length and bounds checks.
- [x] Enumerate PCI functions through native x86 configuration mechanism #1 (I/O ports 0xCF8/0xCFC).
- [x] Detect NVIDIA NV2A PCI ID 10DE:02A0 when present; report absence honestly on ordinary PCs.
- [x] Update QEMU CI to require these serial diagnostics.

This is the first native hardware-access milestone: the harness executes directly in 32-bit protected mode and performs CPU/PCI I/O without a hypervisor. It still halts before loading the Xbox kernel. The PC harness now installs a flat GDT and exception/legacy IRQ IDT gates and tests PIT IRQ0 through the 8259 PIC. It does not yet build Xbox-required page tables or implement the Xbox kernel's PE loader/entry ABI. The VMCALL-based `PC_COMPAT_GUEST` path is not used by this bare-metal harness and must not be confused with it.


### Kernel module validation hardening (2026-11-23)

- [x] Bound the Multiboot module-name scan to 64 bytes.
- [x] Reject module descriptor-table address overflow before indexing it.
- [x] Require the PE section table to fit inside SizeOfHeaders.
- [x] Reject section raw data that overlaps PE headers or exceeds the supplied module.
- [x] Require the PE entry RVA to be nonzero, inside SizeOfImage, and covered by an executable section.
- [ ] Await GitHub Actions run 101 for commit `efffdc7e6dedb8211e927f654da71f159dd13627`; it was still in progress at the time of this update.

These checks harden validation only. The module is still not mapped into a new image layout, relocations/imports are not processed, page tables are not built, and the Xbox kernel entry is not called.


### Stage 2 — bounded PE32 image mapper (2026-11-23)

- [x] Add caller-buffer PE32 mapping: zero image, copy headers, copy section raw data to virtual addresses.
- [x] Apply PE32 `IMAGE_REL_BASED_HIGHLOW` relocations when the requested load base differs from the preferred base.
- [x] Reject missing/out-of-range relocation directories, malformed relocation blocks, out-of-image targets, and unsupported relocation types.
- [x] Add native tests for section mapping, preferred-base entry calculation, relocated absolute addresses, and unsupported relocation rejection.
- [ ] Confirm behavior against a matching, legally available original Xbox kernel image and its actual relocation table.
- [ ] Implement page-table mappings, memory reservations, import resolution, XboxCryptKeys/LoadOptions contract, and the PC HAL shared-window handoff.

The mapper writes only to a caller-provided buffer. It does not allocate physical memory, create page tables, set executable permissions, resolve imports, or call the entry point. It is not yet safe to execute xboxkrnl.exe.


### PE32 mapper verification update

- [x] Native PE32 validator + mapper tests passed in GitHub Actions run [45184279152](https://github.com/rianalwis0v0-a11y/X-box-og-kernel-that-runs-on-pc/actions/runs/45184279152), commit `95628a5336da178fbacb008c83aa7899bff50d44`.
- [x] The same workflow completed the standalone 32-bit build, Multiboot header validation, and QEMU serial boot checks successfully.
- [ ] Re-run CI after the follow-up hardening for caller-supplied metadata and relocation arithmetic (commits after `95628a5`).

The QEMU pass is for the independent PC harness only. It does not load or execute the Xbox kernel image.

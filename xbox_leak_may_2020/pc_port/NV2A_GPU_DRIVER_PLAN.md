# NV2A native PC GPU-driver integration

## Required mode: bare metal, no emulation

This port must not create a virtual NV2A PCI function, VMM device, or software GPU emulator. The Xbox-facing driver retains the NV2A command/register contract, while a native backend translates supported NV2A work into commands for the physical PC GPU.

The PC must not be told that a physical NV2A exists. The Xbox kernel's NV2A-facing interfaces are serviced by the port's driver layer; host GPU PCI identity, BARs, MMIO, DMA and interrupts remain private to the native backend.

## Layers

1. **Xbox NV2A front end:** consumes the original Xbox GPU register writes, pushbuffers, DMA contexts, surfaces and synchronization semantics.
2. **Portable XboxRuntime translation core:** reuse only components that can be separated from Win32/D3D. Keep bounds checks, register decoding, texture/vertex conversion and NV2A shader microcode parsing independent of the host API.
3. **Native physical-GPU backend:** vendor/device-specific implementation for the detected Intel, AMD or NVIDIA GPU. It owns PCI config, BAR mapping, GPU memory, command submission, fences and display output. No Windows D3D calls are available in a bare-metal kernel.
4. **Xbox kernel adapter:** presents the expected Xbox-facing driver contract and reports unsupported operations honestly.

## Current code and first integration milestone

- `nv2a_bridge.h`, `nv2a_dispatch.c`, and `nv2a_transport.c` are a validated shared-memory dispatch protocol. They are not the native hardware renderer and are not used as a virtual device.
- `include/nv2a_pc_bridge.h` defines a host-renderer callback ABI, but does not implement a renderer.
- `nv2a_vertex_shader.c/.h` from XboxRuntime contains a portable NV2A microcode validator and register-usage metadata decoder. It does not compile or execute shaders.
- This PC port now carries a freestanding copy of that validator, built into the ring-0 harness with no Win32/D3D dependency.

## Safety and correctness constraints

- Never report an NV2A PCI ID as present on a normal PC.
- Never pass Xbox physical addresses directly to a PC GPU or expose host GPU BARs to the Xbox kernel.
- Use the PC HAL PCI/MMIO/DMA mapping services; fail closed while a service is unavailable.
- Validate all pushbuffer, DMA, texture, vertex and shader ranges before reading them.
- Unsupported NV097 methods and shader instructions must return explicit errors, not fake success.
- Do not call D3D8/D3D11 or user-mode Windows graphics APIs from the bare-metal kernel.
- Do not claim that a shader is translated merely because its microcode was validated.

## Current limitations

The native harness currently enumerates PCI devices and decodes assigned BAR values, but its MMIO mapper deliberately returns unsupported and it has no vendor-specific GPU command backend. Therefore this milestone adds portable NV2A shader validation only; it does not yet submit rendering to a physical PC GPU. A working native backend requires a selected GPU family and implementation of its command processor, memory manager, synchronization and display path.


## Intel integrated-GPU discovery foundation

The PC harness now has a freestanding Intel display-controller discovery module. It identifies PCI vendor 8086 display/VGA functions, records the PCI revision and assigned BAR resources, and conservatively classifies a curated set of known device IDs into broad architecture generations (Gen2 through Gen12). Unknown IDs remain explicitly unknown; the classifier does not guess.

The kernel PCI scan reports detected Intel display devices and their classified generation. CI tests device classification, rejection of non-Intel/non-display functions, BAR decoding, and a 32-bit freestanding compile.

**This is discovery and resource-description groundwork only. It is not an Intel GPU driver.** It does not enable PCI memory decoding or bus mastering, map BARs, initialise GT power/forcewake, allocate GGTT/GTT or local memory, submit render/BLT commands, compile NV2A shaders, manage fences, or drive display output. No GPU registers are touched. Generation-specific command backends must be implemented and validated independently; support for every Intel iGPU generation is not yet present.

## Intel UHD 610 backend: use Linux i915 as the hardware reference

The first physical target is the Whiskey Lake-U Intel UHD Graphics 610 (PCI vendor/device 8086:3EA1). Linux upstream i915 supports this device family; the kernel PCI device database identifies 3EA1 as Whiskey Lake-U GT1 UHD 610. The i915 driver is therefore a proven hardware reference for Gen9/Gen9.5 initialization, GT power management, memory translation, command submission, synchronization, and display programming.

Do not attempt to load the Linux `i915.ko` binary in the Xbox kernel. It depends on Linux DRM, GEM, Linux PCI, DMA, interrupt, memory-management, workqueue, and kernel APIs that are not provided by the Xbox NT kernel. Porting the complete driver would amount to porting a substantial part of Linux's DRM and kernel support layer.

### Porting strategy

1. Keep the existing Xbox kernel and XBE loader as the guest-facing NT/Xbox environment.
2. Treat upstream i915 source and Intel PRM documentation as the hardware-behaviour reference, not as a drop-in binary.
3. First implement a minimal, isolated Gen9.5 native backend behind the PC HAL: PCI resource validation, BAR mapping, MMIO accessors, safe GT wake/forcewake sequencing, and a read-only device/status probe.
4. Add graphics memory setup (stolen memory/GTT or GGTT as applicable), then a minimal ring/engine submission path with explicit fences and timeout recovery.
5. Add a framebuffer/display path for the first visible boot output. Only then translate the Xbox NV2A methods and shader operations needed by the original startup animation.
6. Keep other Intel generations behind separate generation-specific operations; do not assume UHD 610 register sequences work on every Intel iGPU.

### Licensing boundary

The Linux i915 driver is GPL-2.0. The Xbox source tree contains Microsoft-originated code with separate licensing restrictions. Do not copy i915 implementation code into that tree without a deliberate legal review of the combined distribution and corresponding source obligations. Prefer using public hardware documentation and independently written adapter code; if GPL code is reused, keep its licensing and distribution obligations explicit.

### Current status

The existing Intel module still performs PCI discovery/classification only. No i915 code has been copied, no Intel BAR is mapped, no GT is initialized, and no rendering/display test has been performed. The UHD 610 backend is not yet implemented.


## First Gen9.5 backend code milestone

Added `pc_intel_gen9.h/.c` as a deliberately narrow backend boundary. It accepts only discovered Intel VGA devices classified as Gen9, requires a caller-supplied mapping of at least 4 KiB, and provides aligned, bounds-checked 32-bit MMIO reads. The module performs no PCI writes, no BAR mapping, no GT power changes, no forcewake writes, and no command submission. Unit tests use a fake memory array to validate the access boundary and reject other GPU generations.

This is not yet a hardware probe: no real mapping is supplied by the current HAL, whose `map_mmio` callback remains unsupported. The next prerequisite is a real page-table/MMIO mapping lifecycle and a validated BAR aperture size; only after that can a hardware read-only probe be enabled. Gen9 GT registers may require forcewake/power-domain sequencing, as documented by Intel and handled by i915; the caller must not read those registers before that sequence is implemented.


## Uncached MMIO page-mapping primitive

The PC page-table builder now exposes `pc_paging_map_mmio_identity`. It accepts a 64-bit physical base and length, rejects zero-length, overflow, and ranges outside the 32-bit non-PAE address space, rounds the range out to 4 KiB pages, and creates an identity mapping with writable + cache-disable + write-through PTE flags. The firmware LAPIC/IOAPIC page-table preparation now uses this common helper. Host tests cover unaligned ranges, page-boundary crossing, translation results, cache flags, and rejection of out-of-range inputs.

The harness now has a guarded paging-activation step after validating that the kernel image, stack, and page-table block are below the first 256 MiB identity-map limit. It activates non-PAE paging with the identity map and 0x80000000 alias preserved; the current QEMU run is the first verification of this activation path. This still does not map any Intel BAR or enable GPU MMIO. The native HAL `map_mmio` callback remains unsupported; Intel access still needs a verified aperture length, BAR resource reservation, and GT power/forcewake sequencing.

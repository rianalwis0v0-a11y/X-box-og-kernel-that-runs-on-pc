# PC Compatibility Architecture

## Objective

Run Xbox x86 kernel code on a normal x86-64 PC without interpreting/emulating its CPU instructions. The PC supplies a compatibility platform; the existing NV2A translation project supplies GPU command translation.

This is not achieved by replacing Xbox APIs with Windows APIs inside the kernel. The Xbox kernel is privileged code and expects Xbox-specific physical memory, interrupt, DMA, and device semantics. A host-side compatibility monitor is therefore required.

## Execution model

1. A small PC boot application starts a hardware-virtualization VM (Intel VT-x or AMD-V).
2. The Xbox kernel executes as a 32-bit x86 guest. Its ordinary x86 instructions execute directly on the CPU; VM exits handle privileged operations and accesses to virtualized devices.
3. The monitor provides the Xbox-compatible boot handoff, guest physical-memory map, interrupt injection, timer, PCI configuration space, and DMA address translation.
4. Xbox HAL device operations are implemented by PC device models/adapters. Unsupported Xbox-only devices return explicit errors; they must not report fake success.
5. The guest NV2A-facing adapter writes command/state packets into a shared-memory ring. The host NV2A translation provider consumes the ring, translates NV2A methods/state/shaders, and submits work through the PC graphics API.
6. Completion, fences, interrupts, and errors travel back through the shared ring and virtual interrupt controller.

The VM is a hardware-assisted compatibility boundary, not a full-system Xbox emulator: there is no CPU instruction interpreter. It does virtualize the Xbox platform devices and memory contract that ordinary PCs do not have.

## GPU boundary

The Xbox kernel must not call a Windows D3D/DXGI function directly. The kernel-facing HAL/miniport publishes GPU work through a versioned, fixed-width C ABI in `nv2a_bridge.h`. A host process owns the actual NV2A translator and PC graphics backend.

The shared ABI uses guest-physical/shared-buffer handles, offsets, lengths, and monotonically increasing sequence numbers. Never pass guest virtual pointers to the host. Validate every offset/length against the registered shared-memory allocation before reading it. DMA buffers require explicit pin/map/unmap and lifetime tracking.

Initial GPU milestones:
- [ ] Enumerate and expose a virtual NV2A PCI function to the guest.
- [ ] Map the guest NV2A register aperture to a monitor-owned device model.
- [ ] Implement bounded pushbuffer/shared-ring submission and validation.
- [ ] Connect the host consumer to the existing NV2A translation provider.
- [ ] Translate the first safe command path (clear + triangle) and verify a fence.
- [ ] Add texture, vertex/index buffers, shaders, render targets, and synchronization incrementally.
- [ ] Add interrupt/vblank behavior only after the basic submission path is stable.

## Non-GPU PC platform milestones

- [ ] VT-x/AMD-V entry/exit and guest 32-bit execution.
- [ ] Guest memory map and Xbox-required low/high physical aliases.
- [ ] GDT/IDT, exception reflection, and interrupt injection.
- [ ] Virtual PCI configuration and BAR mapping.
- [ ] Timer source and clock behavior.
- [ ] Xbox IDE/DMA, SMBus/SMC, and MCPX dependencies: implement only those proven necessary; otherwise report unsupported.
- [ ] PE32 kernel loading, relocation, imports, load-options, and exact bldr32 entry ABI.

## Safety/correctness rules

- Do not run the leaked Xbox preloader: it depends on ROM/MCPX secrets and fixed ROM locations.
- Do not execute a kernel image until its PE32 layout, relocation model, entry ABI, and memory requirements are verified.
- Do not claim NV2A support until a real command buffer is submitted and a completion/fence is observed.
- Do not make unsupported hardware initialization appear successful.
- Keep the standalone Multiboot harness as a separate test; it does not prove Xbox kernel boot.

## Current implementation status

The existing pc_port/ Multiboot harness and PE32 inspection tool are validation scaffolding only. This document and nv2a_bridge.h define the next integration boundary; they do not yet implement VT-x/AMD-V, the guest device model, or the host GPU connection.

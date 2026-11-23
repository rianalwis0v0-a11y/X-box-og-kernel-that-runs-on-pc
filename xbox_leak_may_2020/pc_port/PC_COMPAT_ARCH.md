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
- [x] Implement a bounded shared-ring consumer and operation dispatcher in `nv2a_dispatch.c`; malformed commands are retired with an explicit status.
- [ ] Implement the NV2A pushbuffer/method decoder and register/state model connection.
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

## Standard PC device contract

The guest-facing PC contract is defined in `pc_platform_abi.h`. It deliberately uses fixed-width records and offsets into registered shared memory; guest pointers are never passed to the host. The initial operation set covers:

- firmware-provided RAM map and reserved/MMIO ranges;
- PCI enumeration and configuration-space reads/writes;
- BAR/MMIO mapping and DMA map/unmap;
- interrupt registration/EOI and a monotonic timer;
- block I/O through shared buffers;
- NV2A submission through the separate GPU bridge.

The compatibility monitor maps these operations to the host PC. On a Windows host, the monitor/backend can use the OS PCI, memory, interrupt, storage and graphics interfaces; on other hosts, equivalent host backends can be supplied. The Xbox guest sees stable virtual PCI/device contracts, not arbitrary host kernel pointers or direct access to host hardware.

## NV2A connection sequence

1. The monitor exposes a virtual PCI function with NVIDIA vendor ID `0x10DE` and NV2A device ID `0x02A0`, and traps its BAR accesses.
2. The guest-side Xbox GPU adapter converts register/pushbuffer activity into validated `pc_nv2a_command` records in the shared ring from `nv2a_bridge.h`.
3. The host GPU provider consumes those records, translates NV2A state/methods and submits equivalent work through the host graphics backend.
4. Host completion updates the fence and raises the guest's virtual GPU interrupt.

The platform ABI and NV2A bridge are compile-checked by CI. The new dispatcher now validates and consumes one shared-ring command at a time, bounds-checks pushbuffer references, and forwards supported operations through explicit host callbacks. It is a tested integration boundary, not a finished VMM, PCI device model, NV2A method decoder, linked translation provider, or rendered GPU output. The next executable milestone is the real pushbuffer decoder and connection to the existing NV2A state/shader translator, followed by a clear/triangle fence test.


## Verified Xbox kernel entry contract (source inspection)

The checked-in source `xbox trunk/xbox/private/ntos/init/xboxkrnl.c` defines the kernel entry as:

```c
VOID __cdecl main(PUCHAR LoadOptions, const UCHAR *CryptKeys);
```

The PC port now records this as the `xbox_pc_kernel_entry` function-pointer type. The first argument is the boot-loader options string; the second points to the two key blocks consumed by `RtlCopyMemory` in `main`. Their exact lengths and the required buffer layout must come from the matching `bldr.h` definitions and original boot-loader contract; this port does not fabricate key bytes or lengths.

The PE32 loader now fails closed when import, TLS, or delay-import directories are present because no resolver/callback runner exists yet. This prevents treating a copied image as runnable when its startup dependencies have not been initialized.


## 4 KiB page-table builder

`pc_paging.c/.h` now provides a non-PAE 32-bit x86 page-table builder. It accepts caller-supplied page-aligned directory/table storage and explicit physical addresses, maps page-aligned virtual-to-physical ranges, supports supervisor/user and read-write permissions, rejects conflicting mappings, and preflights table capacity so a failed range map does not partially install entries.

The builder does not allocate physical pages, parse firmware memory maps, enable CR0.PG, or choose Xbox virtual/physical aliases. Those are intentionally separate tasks: the next integration must reserve RAM from the Multiboot map, keep the PC loader/stack/Multiboot structures mapped, and establish the verified Xbox address layout before enabling paging or calling the Xbox entry.

# NV2A GPU driver integration plan

## Requirement

The Xbox OS must see an NV2A-compatible GPU through its graphics-driver path. This must be a functional device contract, not just a PCI vendor/device ID inserted into an enumeration list.

## Driver architecture

1. **Xbox-facing GPU driver/miniport** — owns the Xbox-visible device initialization, register access, pushbuffer capture, DMA-buffer registration, fences, and GPU interrupt/vblank contract.
2. **NV2A device model** — provides the expected PCI configuration space and BAR/register behavior. The known identity is NVIDIA vendor ID 0x10DE and NV2A device ID 0x02A0. BAR sizing, reset values, register semantics, and interrupts must be based on verified hardware behavior rather than guessed constants.
3. **NV2A command translator** — decodes NV2A pushbuffers/methods and translates state, shaders, surfaces, textures, vertex/index buffers, and synchronization to the host GPU backend.
4. **Host GPU backend** — submits translated work through a supported PC graphics API/driver and returns completion fences and errors.

## Existing code to build on

- `nv2a_bridge.h`: fixed-width guest/host shared-memory ABI.
- `nv2a_dispatch.c`: bounded shared-ring command consumer and callback dispatcher.
- `nv2a_transport.c`: ABI, command, and pushbuffer bounds validation.
- `include/nv2a_pc_bridge.h` and `nv2a_pc_bridge.c`: host graphics callback contract.

These pieces are interfaces and validation scaffolding. They are not yet a complete PCI function, BAR register model, NV2A method decoder, connected translator, or working graphics driver.

## Non-negotiable correctness rules

- Do not expose 10DE:02A0 as a working GPU until the device configuration space, BARs, register model, reset, and interrupt paths are implemented.
- Never expose host GPU BAR addresses directly to Xbox code.
- Never treat guest physical addresses as host pointers. Pin/register buffers, validate every range, and keep buffers alive until fences complete.
- Unsupported NV2A methods must report an error; do not return fake success.
- First end-to-end acceptance test: initialize the virtual NV2A device, submit a real clear + triangle pushbuffer, observe a completed fence, and verify visible output. Then add textures, shaders, render targets, vblank, and device-loss handling.

## Deployment-mode distinction

The existing `pc_port` is a bare-metal Multiboot diagnostic harness. A virtual PCI NV2A function needs a device-model boundary such as a compatibility monitor/VMM. A strictly bare-metal OS instead needs its Xbox-facing GPU driver to call a native PC GPU backend directly; it cannot call Windows graphics APIs after booting directly into this OS. Do not mix these designs or claim either is implemented until its full path is tested.

## Bootloader reference

Roswell's GPL-2.0 `boot/nxldr` is a useful reference for freestanding PE loading, paging, decompression, and checked loader phases. It is not a drop-in replacement for Microsoft's `bldr32`: Roswell's loader targets Roswell's own kernel startup contract and image layout. Use it as a design reference and compare every ABI and memory-layout assumption against this repository's original `bldr32` sources. Do not copy GPL-2.0 code into the leaked-source tree without a deliberate licensing review.
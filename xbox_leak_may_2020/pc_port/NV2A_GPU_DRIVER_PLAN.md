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

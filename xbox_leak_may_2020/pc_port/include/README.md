# PC port interfaces

These headers define the boundary for replacing Xbox-only hardware services
with ordinary PC services while retaining the Xbox kernel-facing contracts.

- `pc_hal.h`: host CPU/platform services (MMIO mapping, PCI config access,
  interrupt routing, monotonic clock and sleep).
- `nv2a_pc_bridge.h`: Xbox-visible NV2A surface/shader/draw/present requests
  handed to a host renderer backed by a PC graphics API.

## Required architecture

```
Xbox kernel + Xbox game-facing D3D8/NV2A semantics
                    |
          PC compatibility HAL
        /           |            \
   memory/PCI    IRQ/timer     NV2A bridge
                                  |
                      host renderer (D3D/Vulkan)
                                  |
                           ordinary PC GPU
```

The bridge deliberately does not require an NV2A PCI device or map an NV2A
BAR on the PC. Guest register and pushbuffer semantics must be interpreted by
the Xbox-side frontend, then translated into host resources, shaders and draw
commands. The renderer must preserve Xbox-visible synchronization and
presentation behavior.

## Implementation rules

1. Validate ABI versions and callback presence before use.
2. Return `PC_STATUS_UNSUPPORTED` for any host service not implemented.
3. Never report a GPU operation as complete before the host renderer confirms
   submission/completion.
4. Keep guest virtual/physical addresses separate from host pointers.
5. Do not call the original Xbox MCPX, SMC, SMBus, LPC or fixed southbridge
   initialization paths on a generic PC.
6. The first usable graphics milestone is clear → triangle → present, followed
   by textures, fixed-function state, vertex shaders and pixel shaders.

These are contracts only. They do not yet constitute a working PC HAL,
NV2A command translator, Vulkan/D3D renderer, or a bootable Xbox kernel.

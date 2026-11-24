# Original Xbox Dashboard

Place a legally obtained, unmodified dashboard installation in this directory.

Expected layout (actual file names vary by dashboard release):

```
dashboard/
  default.xbe
  data/
  fonts/
  media/
  ...other original dashboard files...
```

## Boot integration status

This directory is a payload location only. The current PC port does **not** yet load or execute the original Xbox dashboard.

The required boot sequence is:

1. PC bootloader loads the PC port kernel.
2. PC HAL initializes CPU, memory, interrupt, PCI and display services.
3. Xbox kernel compatibility layer initializes the Xbox kernel interfaces required by XBE programs.
4. XBE loader maps and relocates `default.xbe`, resolves imports, establishes the Xbox execution environment, and transfers control.
5. Dashboard graphics and input services are connected to native PC hardware drivers.

Do not copy dashboard files into the kernel image or overwrite the kernel/HAL sources. Keep this directory as a separate runtime payload.

## Current limitations

- The existing GRUB ISO boots the standalone PC HAL test harness, not the original Xbox kernel.
- The XBE code currently provides validation/mapping groundwork; it is not a complete Xbox XBE execution environment.
- The NV2A work does not yet provide a native PC GPU renderer.
- No original Xbox boot animation or dashboard launch has been verified on PC.

Keep original Microsoft files and notices intact. This repository's PC-port code does not grant redistribution rights to dashboard files.

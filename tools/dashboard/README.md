# Host-side project dashboard

Place the dashboard application in this directory:

`tools/dashboard/`

Keep it outside `xbox_leak_may_2020/`: the dashboard is a host-side development tool, not part of the Xbox kernel image, HAL, bootloader, or leaked source tree.

## Intended responsibilities

- Display GitHub Actions build/test status and the latest commit.
- Show serial-console output from QEMU or a physical test machine.
- Display boot milestones reported by the loader: PC bootloader entered, Multiboot handoff, PC HAL initialized, kernel module found/validated, Xbox kernel entered, and original animation started.
- Show NV2A backend capability separately from the Xbox kernel's animation state.
- Never report the animation as started merely because the PC harness or VGA console started.

## Data boundary

The dashboard must not load or execute kernel code. It should consume explicit host-side status data (for example, CI results and captured serial logs). Do not bundle leaked Xbox source or ROM/kernel payloads into the dashboard.

## Current state

The harness currently boots its own 32-bit PC test kernel under QEMU. It does not yet execute the Xbox kernel. The authentic Xbox animation remains blocked on a legally obtained, matching boot image and the missing early graphics/NT runtime dependencies. The dashboard should show that as **pending**, not **running**.

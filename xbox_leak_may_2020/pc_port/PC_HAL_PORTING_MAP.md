# Xbox HAL to native-PC HAL porting map

This document records the first concrete integration boundary for replacing Xbox-only
hardware initialization with native PC services. It is an implementation map, not a
claim that the Xbox kernel currently boots on a PC.

## Decision: do not replace the HAL binary

A Windows 2000 `hal.dll` is not a drop-in replacement for the Xbox `halx` image.
The kernel and HAL are a matched NT build: internal structures, exported entry points,
interrupt conventions, loader assumptions, and private HAL/kernel callbacks must agree.
Copying a desktop `hal.dll` beside the Xbox kernel would not make those contracts match.

The safe route is to keep the Xbox kernel's expected HAL-facing entry points and replace
the hardware-dependent implementation behind them, one subsystem at a time. Open,
compatible reference implementations may help with algorithms, but their source must
be adapted to the exact Xbox/NT interfaces rather than linked as an unrelated binary.

## Existing Xbox Phase 0 / Phase 1 hardware contract

The current Xbox HAL initialization source calls these hardware-specific operations:

| Xbox initialization operation | Xbox platform dependency | PC-port action |
| --- | --- | --- |
| `HalpInitializePICs` | Xbox interrupt-controller setup and vector layout | Keep interrupts masked while installing the PC IDT; select PIC or IOAPIC routing from firmware data. |
| `HalpInitializeRealTimeClock` | Xbox southbridge RTC | Use the PC RTC/firmware clock interface; validate century and BCD/binary mode. |
| `HalpInitializeClock` | Xbox timer source and interrupt routing | Select a calibrated HPET, ACPI PM timer, or PIT fallback; connect the chosen IRQ before enabling it. |
| `HalpInitializeStallExecution` | Xbox-specific delay calibration | Calibrate against a monotonic hardware timer; never assume the Xbox CPU clock. |
| `HalpSetupPCIDevices` | Fixed MCPX/LPC/IDE/NIC/USB/AGP BDFs and register values | Replace fixed Xbox BDF writes with enumerated PCI devices and firmware-assigned BAR/IRQ resources. |
| `HalpInitializeSystemControlInterrupt` | Xbox SCI/SMBus/SMC wiring | Register ACPI SCI only after parsing the FADT/MADT and routing its GSI. |
| `HalpProcessSMCInitialState` | Xbox System Management Controller | No generic PC equivalent; isolate behind an optional platform service. |
| `HalpSMBusInterrupt` | Xbox SMBus/SMC device | Do not register on generic PCs unless a supported SMBus controller and device are discovered. |
| `HalpProfileInterrupt` | Xbox profile timer vector | Implement only after a real profile source and interrupt route exist. |

The Xbox source also changes the APIC-base MSR and programs the legacy 8259 PIC.
A PC port must not blindly replay that sequence: modern firmware may require the local
APIC and IOAPIC, and the ACPI MADT describes the interrupt topology.

## Current PC implementation coverage

The standalone `pc_port` currently provides:

- 32-bit protected-mode boot and a flat GDT.
- Exception IDT setup and a separate legacy PIC/PIT test path.
- PCI Configuration Mechanism #1 access (CF8/CFC) and device enumeration.
- A physical-page allocator and a page-table builder (paging remains disabled).
- A basic VGA text splash and COM1 diagnostics.

These are bring-up primitives, not yet an implementation of the Xbox HAL ABI.
In particular, PCI enumeration is read-only in the harness; it does not safely
assign BARs, enable bus mastering, configure bridges, or route device interrupts.

## Integration order

1. **Freeze the ABI boundary.** Inventory every HAL routine imported by the Xbox kernel
   and record calling convention, argument layout, IRQL assumptions, and initialization
   phase. Keep the Xbox executive above this boundary unchanged.
2. **Firmware discovery.** Add bounded ACPI RSDP/XSDT-or-RSDT parsing, then MADT and FADT
   parsing. Validate checksums and every table length before using addresses.
3. **PCI resource model.** Enumerate all buses (including bridges), read BAR sizes using
   a controlled and restored sizing transaction, reserve address ranges, and map MMIO
   as uncached/device memory. Do not write Xbox hard-coded BAR values to PC hardware.
4. **Interrupt backend.** Choose PIC only when firmware/platform conditions permit;
   otherwise configure local APIC/IOAPIC from MADT. Keep all routes masked until IDT
   handlers and kernel dispatch are installed.
5. **Timer and delay services.** Calibrate a monotonic source first, then implement
   clock interrupts and stall routines against that source.
6. **Optional platform devices.** Add storage, USB, network, audio, and GPU services
   independently. Xbox SMC/SMBus behavior must not be assumed to exist on a PC.
7. **Kernel handoff.** Only after the HAL services and exact loader contract are valid
   should the Xbox kernel's Phase 0/Phase 1 initialization be entered.

## Boot safety gates

- Do not enable interrupts before the IDT, interrupt-controller mode, and dispatch path
  agree on vector numbers and end-of-interrupt handling.
- Do not enable paging until kernel, stack, page tables, boot data, and MMIO mappings
  are all valid in the new address space.
- Do not enable PCI bus mastering until DMA buffers, IOMMU policy (if present), and
  device ownership are established.
- Do not invoke Xbox HAL initialization that writes MCPX/LPC register offsets on an
  ordinary PC.
- A QEMU run can validate generic PC boot mechanics, but it cannot validate NV2A or
  Xbox-specific SMC/SMBus hardware behavior.

## Source locations to audit

The current repository's Xbox HAL implementation is under
`xbox trunk/xbox/private/ntos/halx/i386/`. The key initialization path is
`xxhal.c`; private HAL declarations are in `halp.h`; PCI device programming is in
`pcisetup.c`; and PCI configuration access is in `pcispace.c`.

The native bring-up implementation is under `pc_port/`, especially
`pc_hal_native.c`, `pc_irq.c`, `pc_pci.c`, and `pc_paging.c`.

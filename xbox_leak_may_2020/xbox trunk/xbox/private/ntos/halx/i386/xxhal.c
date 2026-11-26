/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    xxhal.c

Abstract:

    This module implements the initialization of the system dependent
    functions that define the Hardware Architecture Layer (HAL) for an
    x86 system.

--*/

#include "halp.h"
#ifdef PC_COMPAT_GUEST
#include "pc_hal_guest.h"
#endif

/*
 * PC_COMPAT_NATIVE reuses the x86 NT HAL's real ring-0 PIC, CMOS RTC and
 * PIT clock routines. It is deliberately separate from PC_COMPAT_GUEST:
 * no VMCALL monitor or shared-memory transport is involved.
 */

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, HalInitSystemPhase0)
#endif

VOID
HalInitSystemPhase0(
    VOID
    )
/*++

Routine Description:

    This function initializes the Hardware Architecture Layer (HAL) for an
    x86 system.

Arguments:

    None.

Return Value:

    None.

--*/
{
#ifdef PC_COMPAT_GUEST
    /*
     * The PC monitor owns the physical APIC, interrupt routing, timer and
     * PCI host bridge. Do not execute Xbox MCPX/LPC/PIC initialization here.
     */
    if (!PcHalInitializePlatform()) {
        KeBugCheck(0x50434841); /* PC HAL handshake failed */
    }
#elif defined(PC_COMPAT_NATIVE)
    /*
     * Native legacy-PC path: keep the Xbox NT HAL's x86 hardware routines,
     * but do not touch Xbox LPC/MCPX registers or force-disable the PC APIC.
     * HalpInitializePICs uses the existing 0x30/0x38 NT vector convention;
     * the clock implementation is the standard 8254 PIT channel-0 path.
     *
     * This mode is for firmware exposing legacy PIC compatibility only.
     * Full MADT IOAPIC/LAPIC routing is a separate required integration
     * before enabling this mode on APIC-only or x2APIC-only machines.
     */
    HalpInitializePICsPreserveFlags();
    HalpInitializeRealTimeClock();
    HalpInitializeClock();
    HalStopProfileInterrupt(0);
#else
    ULONGLONG ApicBits;

    _outp(0x61, 0x08);

    //
    // Disable the processor APIC (advanced programmable interrupt
    // controller).  The processor powers on with the APIC enabled.
    //

    HalPulseHardwareMonitorPin();
    ApicBits = RDMSR(APIC_BASE_MSR);
    WRMSR(APIC_BASE_MSR, (ApicBits & ~APIC_ENABLED));

    //
    // Initialize the 8259 programmable interrupt controller.  Note that
    // this will enable interrupts.
    //

    HalPulseHardwareMonitorPin();
    HalpInitializePICs();

    //
    // Initialize the real time clock.
    //

    HalPulseHardwareMonitorPin();
    HalpInitializeRealTimeClock();

    //
    // Note that HalpInitializeClock MUST be called after
    // HalpInitializeStallExecution, because HalpInitializeStallExecution
    // reprograms the timer.
    //

//  HalpInitializeStallExecution();

    //
    // Setup the clock.
    //

    HalPulseHardwareMonitorPin();
    HalpInitializeClock();

    //
    // Make sure the profile interrupt is disabled.
    //

    HalPulseHardwareMonitorPin();
    HalStopProfileInterrupt(0);

    //
    // Configure the devices built into the south bridge.
    //

    HalPulseHardwareMonitorPin();
    HalpSetupPCIDevices();

    //
    // Initialize the System Control Interrupt.
    //

    HalPulseHardwareMonitorPin();
    HalpInitializeSystemControlInterrupt();
#endif
}

VOID
HalInitSystemPhase0Quick(
    VOID
    )
/*++

Routine Description:

    This function initializes the Hardware Architecture Layer (HAL) for an
    x86 system.

Arguments:

    None.

Return Value:

    None.

--*/
{
#ifdef PC_COMPAT_GUEST
    if (!PcHalInitializePlatform()) {
        KeBugCheck(0x50434841);
    }
#elif defined(PC_COMPAT_NATIVE)
    HalpInitializePICsPreserveFlags();
#else
    HalPulseHardwareMonitorPin();
    HalpInitializePICs();
#endif

}

VOID
HalInitSystemPhase1(
    VOID
    )
/*++

Routine Description:

    This function initializes the Hardware Architecture Layer (HAL) for an
    x86 system.

Arguments:

    None.

Return Value:

    None.

--*/
{
#ifdef PC_COMPAT_GUEST
    KiSetHandlerAddressToIDT(CLOCK_VECTOR, KeClockInterrupt);
    if (!PcHalRegisterInterrupt(CLOCK_VECTOR, 0)) {
        KeBugCheck(0x50434952); /* PC interrupt registration failed */
    }
    /* Xbox-only SMC/SMBus/SCI interrupts are intentionally not registered. */
#elif defined(PC_COMPAT_NATIVE)
    /*
     * Install the normal NT clock ISR on the legacy PC timer vector.
     * Do not register Xbox SMC/SMBus/SCI sources or the Xbox profile source.
     */
    KiSetHandlerAddressToIDT(CLOCK_VECTOR, KeClockInterrupt);
    HalEnableSystemInterrupt(CLOCK_VECTOR - PRIMARY_VECTOR_BASE, Latched);
#else

    //
    // Connect to the system interrupts.
    //

    HalPulseHardwareMonitorPin();
    KiSetHandlerAddressToIDT(CLOCK_VECTOR, KeClockInterrupt);
    HalEnableSystemInterrupt(CLOCK_VECTOR - PRIMARY_VECTOR_BASE, Latched);

    HalPulseHardwareMonitorPin();
    KiSetHandlerAddressToIDT(PROFILE_VECTOR, HalpProfileInterrupt);
    HalEnableSystemInterrupt(PROFILE_VECTOR - PRIMARY_VECTOR_BASE, Latched);

    HalPulseHardwareMonitorPin();
    KiSetHandlerAddressToIDT(SMBUS_VECTOR, HalpSMBusInterrupt);
    HalEnableSystemInterrupt(SMBUS_VECTOR - PRIMARY_VECTOR_BASE, LevelSensitive);

    //
    // The following actions need to be performed before servicing interrupts
    // from the SMC, but after connecting to the above interrupt vectors.
    //

    if (!KeHasQuickBooted) {
        HalPulseHardwareMonitorPin();
        HalpProcessSMCInitialState();
    }

    //
    // Connect to the System Control Interrupt.
    //

    HalPulseHardwareMonitorPin();
    KiSetHandlerAddressToIDT(SCI_VECTOR, HalpSystemControlInterrupt);
    HalEnableSystemInterrupt(SCI_VECTOR - PRIMARY_VECTOR_BASE, LevelSensitive);
#endif
}

VOID
HalPulseHardwareMonitorPin(
    VOID
    )
/*++

Routine Description:

    This function pulses the hardware monitor pin used to externally monitor the
    boot process.

Arguments:

    None.

Return Value:

    None.

--*/
{
#if defined(PC_COMPAT_GUEST) || defined(PC_COMPAT_NATIVE)
    /* No Xbox LPC hardware-monitor pin exists on a standard PC. */
#else
    if (!KeHasQuickBooted &&
        ((XboxBootFlags & XBOX_BOOTFLAG_SHADOW) == 0)) {
        _outp(XPCICFG_LPCBRIDGE_IO_REGISTER_BASE_0 + 0xCF, 0x05);
        KeStallExecutionProcessor(5);
        _outp(XPCICFG_LPCBRIDGE_IO_REGISTER_BASE_0 + 0xCF, 0x04);
        KeStallExecutionProcessor(5);
    }
#endif

}

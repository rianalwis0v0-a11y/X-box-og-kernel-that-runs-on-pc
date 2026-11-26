/*++

Copyright (c) 1989  Microsoft Corporation

Module Name:

    ixsysbus.c

Abstract:

--*/

#include "halp.h"

ULONG
HalGetInterruptVector(
    IN ULONG BusInterruptLevel,
    OUT PKIRQL Irql
    )

/*++

Routine Description:

Arguments:

    BusInterruptLevel - Supplies the bus specific interrupt level.

    Irql - Returns the system request priority.

Return Value:

    Returns the system interrupt vector corresponding to the specified device.

--*/
{
    ULONG SystemVector;

#if defined(PC_COMPAT_NATIVE)
    /*
     * A legacy dual-8259 PC exposes only IRQ0..IRQ15.  The generic Xbox
     * HAL's HIGHEST_LEVEL_FOR_8259 constant is an IRQL limit, not a count
     * of physical PIC inputs.  Reject out-of-range bus levels before
     * converting them to vectors, rather than manufacturing vectors for
     * nonexistent PIC inputs.
     */
    if (BusInterruptLevel >= PIC_VECTORS) {
        return(0);
    }
#endif

    SystemVector = BusInterruptLevel + PRIMARY_VECTOR_BASE;

    if ((SystemVector < PRIMARY_VECTOR_BASE) ||
        (SystemVector > PRIMARY_VECTOR_BASE + HIGHEST_LEVEL_FOR_8259) ) {

        //
        // This is an illegal BusInterruptLevel and cannot be connected.
        //

        return(0);
    }

    *Irql = (KIRQL)(HIGHEST_LEVEL_FOR_8259 + PRIMARY_VECTOR_BASE - SystemVector);

    return SystemVector;
}

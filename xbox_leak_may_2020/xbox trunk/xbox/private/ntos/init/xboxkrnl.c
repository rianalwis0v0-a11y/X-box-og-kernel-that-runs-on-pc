/*++

Copyright (c) 1989-2001  Microsoft Corporation

Module Name:

    xboxkrnl.c

Abstract:

    Entry point for the kernel.

--*/

#include "ntos.h"
#include <bldr.h>

VOID
__cdecl
main(
    IN PUCHAR LoadOptions,
    IN const UCHAR* CryptKeys
    );

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, main)
#endif

VOID
__cdecl
main(
    IN PUCHAR LoadOptions,
    IN const UCHAR* CryptKeys
    )
{
#if defined(DEVKIT) && !defined(PC_COMPAT_NATIVE)
    //
    // Copy the load options from the boot loader to a local buffer and then
    // parse the various options and convert them to boot flags.
    //

    UCHAR buf[64];
    strncpy(buf, LoadOptions, 64);
    buf[63] = '\0';
    _strupr(buf);

    if (strstr(buf, "SHADOW") != NULL)
        XboxBootFlags |= XBOX_BOOTFLAG_SHADOW;

    if (strstr(buf, "/HDBOOT") != NULL)
        XboxBootFlags |= XBOX_BOOTFLAG_HDBOOT;

    if (strstr(buf, "/CDBOOT") != NULL)
        XboxBootFlags |= XBOX_BOOTFLAG_CDBOOT;

    if (strstr(buf, "/DBBOOT") != NULL)
        XboxBootFlags |= XBOX_BOOTFLAG_DASHBOARDBOOT;
#endif

#if !defined(PC_COMPAT_NATIVE)
    //
    // Save the encryption keys that have been passed in from the Xbox boot loader.
    //

    RtlCopyMemory(XboxEEPROMKey, CryptKeys, XBOX_KEY_LENGTH);
    RtlCopyMemory(XboxCERTKey, CryptKeys + XBOX_KEY_LENGTH, XBOX_KEY_LENGTH);

    //
    // Zero out the memory used by the boot loader at its relocated origin.  The
    // original origin of the boot loader has already been overwritten by the
    // kernel image.
    //

    RtlZeroMemory((PUCHAR)MM_SYSTEM_RANGE_START + BLDR_RELOCATED_ORIGIN,
        BLDR_BLOCK_SIZE);
#else
    // A PC loader has no Xbox ROM-derived keys or relocated Xbox bootloader.
    // Leave the zero-initialized Xbox key globals untouched; never dereference
    // a fabricated CryptKeys pointer or clear an unrelated PC physical range.
    (void)LoadOptions;
    (void)CryptKeys;
#endif

    //
    // Initialize the system.
    //

    KiSystemStartup();
}

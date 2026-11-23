#include "pc_hal_native.h"

#if defined(_MSC_VER) && defined(_M_IX86)
uint8_t PcHalNativeIn8(uint16_t port)
{
    uint8_t value;
    __asm {
        mov dx, port
        in al, dx
        mov value, al
    }
    return value;
}
void PcHalNativeOut8(uint16_t port, uint8_t value)
{
    __asm {
        mov dx, port
        mov al, value
        out dx, al
    }
}
void PcHalNativeDisableInterrupts(void) { __asm cli }
void PcHalNativeEnableInterrupts(void) { __asm sti }
#elif defined(__i386__) || defined(__x86_64__)
uint8_t PcHalNativeIn8(uint16_t port)
{
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}
void PcHalNativeOut8(uint16_t port, uint8_t value)
{
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}
void PcHalNativeDisableInterrupts(void) { __asm__ volatile ("cli" ::: "memory"); }
void PcHalNativeEnableInterrupts(void) { __asm__ volatile ("sti" ::: "memory"); }
#else
#error "PC native HAL requires an x86 compiler target"
#endif

static uint32_t PcHalNativeSaveAndDisableInterrupts(void)
{
#if defined(_MSC_VER) && defined(_M_IX86)
    uint32_t flags;
    __asm { pushfd; pop eax; mov flags, eax; cli }
    return flags;
#else
    uint32_t flags;
    __asm__ volatile ("pushfl; popl %0; cli" : "=r"(flags) :: "memory");
    return flags;
#endif
}

static void PcHalNativeRestoreInterrupts(uint32_t flags)
{
    if ((flags & 0x200u) != 0u) PcHalNativeEnableInterrupts();
    else PcHalNativeDisableInterrupts();
}

static void PcHalNativeIoWait(void)
{
    PcHalNativeOut8(0x80u, 0u);
}

void PcHalNativePicRemapMasked(void)
{
    uint32_t flags = PcHalNativeSaveAndDisableInterrupts();
    /* ICW1: initialize both PICs; ICW4: 8086 mode. */
    PcHalNativeOut8(0x21u, 0xffu);
    PcHalNativeOut8(0xa1u, 0xffu);
    PcHalNativeOut8(0x20u, 0x11u); PcHalNativeIoWait();
    PcHalNativeOut8(0xa0u, 0x11u); PcHalNativeIoWait();
    PcHalNativeOut8(0x21u, PC_HAL_PIC_MASTER_VECTOR); PcHalNativeIoWait();
    PcHalNativeOut8(0xa1u, PC_HAL_PIC_SLAVE_VECTOR); PcHalNativeIoWait();
    PcHalNativeOut8(0x21u, 0x04u); PcHalNativeIoWait();
    PcHalNativeOut8(0xa1u, 0x02u); PcHalNativeIoWait();
    PcHalNativeOut8(0x21u, 0x01u); PcHalNativeIoWait();
    PcHalNativeOut8(0xa1u, 0x01u); PcHalNativeIoWait();
    /* Keep all IRQs masked until the kernel has installed its IDT handlers. */
    PcHalNativeOut8(0x21u, 0xffu);
    PcHalNativeOut8(0xa1u, 0xffu);
    PcHalNativeRestoreInterrupts(flags);
}

/* PCI Configuration Mechanism #1 address; offset is rounded to its dword. */
static uint32_t PcHalNativePciAddress(uint8_t bus, uint8_t device,
                                      uint8_t function, uint8_t offset)
{
    return 0x80000000u | ((uint32_t)bus << 16) |
           ((uint32_t)device << 11) | ((uint32_t)function << 8) |
           ((uint32_t)offset & 0xfcu);
}

uint8_t PcHalNativePciRead8(uint8_t bus, uint8_t device,
                            uint8_t function, uint8_t offset)
{
    uint32_t address, flags;
    uint8_t value;
    if (device >= 32u || function >= 8u) return 0xffu;
    address = PcHalNativePciAddress(bus, device, function, offset);
    flags = PcHalNativeSaveAndDisableInterrupts();
#if defined(_MSC_VER) && defined(_M_IX86)
    __asm {
        mov dx, 0xcf8
        mov eax, address
        out dx, eax
        mov dx, 0xcfc
        mov al, offset
        and al, 3
        movzx edx, al
        add dx, 0xcfc
        in al, dx
        mov value, al
    }
#else
    __asm__ volatile ("outl %0, %1" : : "a"(address), "Nd"((uint16_t)0xcf8));
    __asm__ volatile ("inb %1, %0" : "=a"(value) :
                      "Nd"((uint16_t)(0xcfcu + (offset & 3u))));
#endif
    PcHalNativeRestoreInterrupts(flags);
    return value;
}

uint16_t PcHalNativePciRead16(uint8_t bus, uint8_t device,
                              uint8_t function, uint8_t offset)
{
    uint32_t address, flags;
    uint16_t value;
    if (device >= 32u || function >= 8u || (offset & 1u) != 0u || offset > 254u)
        return 0xffffu;
    address = PcHalNativePciAddress(bus, device, function, offset);
    flags = PcHalNativeSaveAndDisableInterrupts();
#if defined(_MSC_VER) && defined(_M_IX86)
    __asm {
        mov dx, 0xcf8
        mov eax, address
        out dx, eax
        mov dx, 0xcfc
        movzx eax, offset
        and eax, 2
        add dx, ax
        in ax, dx
        mov value, ax
    }
#else
    __asm__ volatile ("outl %0, %1" : : "a"(address), "Nd"((uint16_t)0xcf8));
    __asm__ volatile ("inw %1, %0" : "=a"(value) :
                      "Nd"((uint16_t)(0xcfcu + (offset & 2u))));
#endif
    PcHalNativeRestoreInterrupts(flags);
    return value;
}

uint32_t PcHalNativePciRead32(uint8_t bus, uint8_t device,
                              uint8_t function, uint8_t offset)
{
    uint32_t address, value, flags;
    if (device >= 32u || function >= 8u || (offset & 3u) != 0u || offset > 252u)
        return 0xffffffffu;
    address = PcHalNativePciAddress(bus, device, function, offset);
    flags = PcHalNativeSaveAndDisableInterrupts();
#if defined(_MSC_VER) && defined(_M_IX86)
    __asm {
        mov dx, 0xcf8
        mov eax, address
        out dx, eax
        mov dx, 0xcfc
        in eax, dx
        mov value, eax
    }
#else
    __asm__ volatile ("outl %0, %1" : : "a"(address), "Nd"((uint16_t)0xcf8));
    __asm__ volatile ("inl %1, %0" : "=a"(value) : "Nd"((uint16_t)0xcfc));
#endif
    PcHalNativeRestoreInterrupts(flags);
    return value;
}

void PcHalNativePciWrite8(uint8_t bus, uint8_t device, uint8_t function,
                          uint8_t offset, uint8_t value)
{
    uint32_t address, flags;
    if (device >= 32u || function >= 8u) return;
    address = PcHalNativePciAddress(bus, device, function, offset);
    flags = PcHalNativeSaveAndDisableInterrupts();
#if defined(_MSC_VER) && defined(_M_IX86)
    __asm {
        mov dx, 0xcf8
        mov eax, address
        out dx, eax
        mov dx, 0xcfc
        movzx eax, offset
        and eax, 3
        add dx, ax
        mov al, value
        out dx, al
    }
#else
    __asm__ volatile ("outl %0, %1" : : "a"(address), "Nd"((uint16_t)0xcf8));
    __asm__ volatile ("outb %0, %1" : : "a"(value),
                      "Nd"((uint16_t)(0xcfcu + (offset & 3u))));
#endif
    PcHalNativeRestoreInterrupts(flags);
}

void PcHalNativePciWrite16(uint8_t bus, uint8_t device, uint8_t function,
                           uint8_t offset, uint16_t value)
{
    uint32_t address, flags;
    if (device >= 32u || function >= 8u || (offset & 1u) != 0u || offset > 254u)
        return;
    address = PcHalNativePciAddress(bus, device, function, offset);
    flags = PcHalNativeSaveAndDisableInterrupts();
#if defined(_MSC_VER) && defined(_M_IX86)
    __asm {
        mov dx, 0xcf8
        mov eax, address
        out dx, eax
        mov dx, 0xcfc
        movzx eax, offset
        and eax, 2
        add dx, ax
        mov ax, value
        out dx, ax
    }
#else
    __asm__ volatile ("outl %0, %1" : : "a"(address), "Nd"((uint16_t)0xcf8));
    __asm__ volatile ("outw %0, %1" : : "a"(value),
                      "Nd"((uint16_t)(0xcfcu + (offset & 2u))));
#endif
    PcHalNativeRestoreInterrupts(flags);
}

void PcHalNativePciWrite32(uint8_t bus, uint8_t device, uint8_t function,
                           uint8_t offset, uint32_t value)
{
    uint32_t address, flags;
    if (device >= 32u || function >= 8u || (offset & 3u) != 0u || offset > 252u)
        return;
    address = PcHalNativePciAddress(bus, device, function, offset);
    flags = PcHalNativeSaveAndDisableInterrupts();
#if defined(_MSC_VER) && defined(_M_IX86)
    __asm {
        mov dx, 0xcf8
        mov eax, address
        out dx, eax
        mov dx, 0xcfc
        mov eax, value
        out dx, eax
    }
#else
    __asm__ volatile ("outl %0, %1" : : "a"(address), "Nd"((uint16_t)0xcf8));
    __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"((uint16_t)0xcfc));
#endif
    PcHalNativeRestoreInterrupts(flags);
}

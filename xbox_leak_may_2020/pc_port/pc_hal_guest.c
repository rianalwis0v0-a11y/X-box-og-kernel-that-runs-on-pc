#include "pc_hal_guest.h"
#include "pc_platform_abi.h"
#include <stddef.h>
#include <string.h>

#define PC_HAL_MAX_MEMORY_RANGES 64u
#define PC_HAL_MAX_PCI_DEVICES 32u

static volatile unsigned char *g_shared;
static uint64_t g_call_offset;
static uint64_t g_sequence;

void PcHalBindSharedWindow(volatile void *mapping, uint64_t call_record_offset)
{
    g_shared = (volatile unsigned char *)mapping;
    g_call_offset = call_record_offset;
    g_sequence = 0;
}

#ifdef PC_COMPAT_GUEST
static uint32_t PcHalHypercall(uint64_t sharedOffset, uint32_t bytes, uint64_t sequence)
{
    uint32_t result;
#if defined(_MSC_VER) && defined(_M_IX86)
    __asm {
        mov eax, PC_PLATFORM_CALL_MAGIC
        mov ebx, dword ptr sharedOffset
        mov ecx, bytes
        mov edx, dword ptr sequence
        _emit 0x0F
        _emit 0x01
        _emit 0xC1
        mov result, eax
    }
#elif defined(__i386__)
    __asm__ volatile ("vmcall"
        : "=a"(result)
        : "a"(PC_PLATFORM_CALL_MAGIC), "b"((uint32_t)sharedOffset),
          "c"(bytes), "d"((uint32_t)sequence)
        : "memory");
#else
#error PC_COMPAT_GUEST requires a 32-bit x86 VMCALL build
#endif
    return result;
}
#endif

static uint32_t PcHalInvoke(uint32_t operation, uint32_t payload_bytes)
{
    volatile pc_platform_call *call;
    uint32_t total = (uint32_t)sizeof(pc_platform_call) + payload_bytes;
    uint32_t status;

    if (!g_shared || g_call_offset > 0xFFFFFFFFULL ||
        total > PC_PLATFORM_MAX_RECORD_BYTES)
        return PC_PLATFORM_E_INVALID;

    call = (volatile pc_platform_call *)(g_shared + (size_t)g_call_offset);
    call->magic = PC_PLATFORM_CALL_MAGIC;
    call->abi_major = PC_PLATFORM_ABI_MAJOR;
    call->abi_minor = PC_PLATFORM_ABI_MINOR;
    call->operation = operation;
    call->record_bytes = total;
    call->shared_offset = g_call_offset;
    call->sequence = ++g_sequence;
    call->status = PC_PLATFORM_E_BUSY;
    call->reserved = 0;

#ifdef PC_COMPAT_GUEST
    status = PcHalHypercall(g_call_offset, total, g_sequence);
    if (status != PC_PLATFORM_OK)
        return status;
    return call->status;
#else
    (void)status;
    return PC_PLATFORM_E_UNSUPPORTED;
#endif
}

int PcHalInitializePlatform(void)
{
#ifdef PC_COMPAT_GUEST
    pc_platform_memory_range *ranges;
    pc_platform_pci_device *devices;
    pc_platform_timer_info *timer;

    if (!g_shared)
        return 0;

    if (PcHalInvoke(PC_PLATFORM_OP_QUERY, 0) != PC_PLATFORM_OK)
        return 0;

    ranges = (pc_platform_memory_range *)(g_shared + (size_t)g_call_offset + sizeof(pc_platform_call));
    memset(ranges, 0, sizeof(*ranges) * PC_HAL_MAX_MEMORY_RANGES);
    if (PcHalInvoke(PC_PLATFORM_OP_MEMORY_MAP,
                    sizeof(*ranges) * PC_HAL_MAX_MEMORY_RANGES) != PC_PLATFORM_OK)
        return 0;

    devices = (pc_platform_pci_device *)(g_shared + (size_t)g_call_offset + sizeof(pc_platform_call));
    memset(devices, 0, sizeof(*devices) * PC_HAL_MAX_PCI_DEVICES);
    if (PcHalInvoke(PC_PLATFORM_OP_PCI_ENUMERATE,
                    sizeof(*devices) * PC_HAL_MAX_PCI_DEVICES) != PC_PLATFORM_OK)
        return 0;

    timer = (pc_platform_timer_info *)(g_shared + (size_t)g_call_offset + sizeof(pc_platform_call));
    memset(timer, 0, sizeof(*timer));
    if (PcHalInvoke(PC_PLATFORM_OP_TIMER_QUERY, sizeof(*timer)) != PC_PLATFORM_OK ||
        timer->frequency_hz == 0)
        return 0;

    return 1;
#else
    return 0;
#endif
}

int PcHalRegisterInterrupt(uint32_t vector, uint32_t flags)
{
#ifdef PC_COMPAT_GUEST
    pc_platform_interrupt_request *request;
    if (!g_shared) return 0;
    request = (pc_platform_interrupt_request *)(g_shared + (size_t)g_call_offset + sizeof(pc_platform_call));
    request->guest_irq = vector;
    request->vector = vector;
    request->flags = flags;
    request->reserved = 0;
    return PcHalInvoke(PC_PLATFORM_OP_INTERRUPT_REGISTER,
                       sizeof(*request)) == PC_PLATFORM_OK;
#else
    (void)vector;
    (void)flags;
    return 0;
#endif
}

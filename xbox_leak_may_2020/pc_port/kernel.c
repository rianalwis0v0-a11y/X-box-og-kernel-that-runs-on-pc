#include <stdint.h>
#include "xbox_pc_boot.h"
#include "pc_idt.h"

#define COM1 0x3F8u
#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002u
#define MULTIBOOT_INFO_MEMORY  (1u << 0)
#define MULTIBOOT_INFO_MMAP    (1u << 6)
#define PCI_CONFIG_ADDRESS 0xCF8u
#define PCI_CONFIG_DATA    0xCFCu

typedef struct multiboot_info_min {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
} multiboot_info_min;

typedef struct multiboot_mmap_entry {
    uint32_t size;
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} __attribute__((packed)) multiboot_mmap_entry;

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}
static inline void outl(uint16_t port, uint32_t value) {
    __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
}
static inline uint32_t inl(uint16_t port) {
    uint32_t value;
    __asm__ volatile ("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}
static void serial_init(void) {
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}
static void serial_putc(char c) {
    unsigned int timeout = 1000000;
    while (!(inb(COM1 + 5) & 0x20u) && --timeout) { }
    if (timeout) outb(COM1, (uint8_t)c);
}
static void serial_write(const char *s) {
    while (*s) {
        if (*s == '\n') serial_putc('\r');
        serial_putc(*s++);
    }
}
static void serial_hex(uint32_t value) {
    static const char digits[] = "0123456789ABCDEF";
    int shift;
    for (shift = 28; shift >= 0; shift -= 4)
        serial_putc(digits[(value >> shift) & 0xFu]);
}
static uint32_t pci_read32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t address = 0x80000000u | ((uint32_t)bus << 16) |
        ((uint32_t)device << 11) | ((uint32_t)function << 8) |
        ((uint32_t)offset & 0xFCu);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}
static void report_cpu(void) {
    uint32_t eax = 0, ebx, ecx = 0, edx;
    char vendor[13];
    __asm__ volatile ("cpuid" : "+a"(eax), "=b"(ebx), "+c"(ecx), "=d"(edx));
    *(uint32_t *)&vendor[0] = ebx;
    *(uint32_t *)&vendor[4] = edx;
    *(uint32_t *)&vendor[8] = ecx;
    vendor[12] = 0;
    serial_write("PC-HAL: CPU vendor ");
    serial_write(vendor);
    serial_write("\n");
}
static void report_memory(const multiboot_info_min *info) {
    uint32_t offset = 0;
    uint64_t usable = 0;
    if (!(info->flags & MULTIBOOT_INFO_MMAP) || !info->mmap_addr) {
        serial_write("PC-HAL: memory map unavailable (bootloader did not supply it)\n");
        if (info->flags & MULTIBOOT_INFO_MEMORY) {
            serial_write("PC-HAL: lower KiB=");
            serial_hex(info->mem_lower);
            serial_write(" upper KiB=");
            serial_hex(info->mem_upper);
            serial_write("\n");
        }
        return;
    }
    while (offset + sizeof(uint32_t) <= info->mmap_length) {
        const multiboot_mmap_entry *entry =
            (const multiboot_mmap_entry *)(uintptr_t)(info->mmap_addr + offset);
        uint32_t step = entry->size + sizeof(entry->size);
        if (entry->size < 20u || step > info->mmap_length - offset) {
            serial_write("PC-HAL: malformed memory map entry; stopping safely\n");
            return;
        }
        if (entry->type == 1u && UINT64_MAX - usable >= entry->len)
            usable += entry->len;
        offset += step;
    }
    serial_write("PC-HAL: usable RAM bytes (low 32 bits)=");
    serial_hex((uint32_t)usable);
    serial_write("\nPC-HAL: Multiboot memory map parsed\n");
}
static void report_pci(void) {
    uint32_t found = 0;
    uint32_t nv2a = 0;
    uint32_t bus, device, function;
    for (bus = 0; bus < 256; ++bus) {
        for (device = 0; device < 32; ++device) {
            uint32_t id0 = pci_read32((uint8_t)bus, (uint8_t)device, 0, 0);
            uint32_t vendor0 = id0 & 0xFFFFu;
            uint32_t header = pci_read32((uint8_t)bus, (uint8_t)device, 0, 0x0C);
            uint32_t functions = ((header >> 16) & 0x80u) ? 8u : 1u;
            if (vendor0 == 0xFFFFu) continue;
            for (function = 0; function < functions; ++function) {
                uint32_t id = function == 0 ? id0 :
                    pci_read32((uint8_t)bus, (uint8_t)device, (uint8_t)function, 0);
                uint32_t vendor = id & 0xFFFFu;
                uint32_t product = id >> 16;
                if (vendor == 0xFFFFu) continue;
                ++found;
                if (vendor == 0x10DEu && product == 0x02A0u) ++nv2a;
            }
        }
    }
    serial_write("PC-HAL: PCI functions found=");
    serial_hex(found);
    serial_write("\n");
    serial_write(nv2a ? "PC-HAL: NV2A PCI function detected\n" :
                        "PC-HAL: NV2A not present (expected on ordinary PCs)\n");
}
void kernel_main(uint32_t multiboot_magic, uint32_t multiboot_info_address) {
    const multiboot_info_min *info = (const multiboot_info_min *)(uintptr_t)multiboot_info_address;
    serial_init();
    PcIdtInstallExceptions();
    serial_write("PC-HAL: exception IDT installed (vectors 0-31)\n");
    serial_write("PC-HARNESS: entered 32-bit bare-metal kernel\n");
    if (multiboot_magic != MULTIBOOT_BOOTLOADER_MAGIC || !info) {
        serial_write("PC-HARNESS: ERROR invalid Multiboot handoff\n");
        for (;;) __asm__ volatile ("cli; hlt");
    }
    serial_write("PC-HARNESS: Multiboot handoff PASS\n");
    serial_write("PC-HAL: executing ring-0 native x86 hardware probes\n");
    report_cpu();
    report_memory(info);
    report_pci();
    serial_write("PC-HARNESS: native PC HAL probe PASS\n");
    {
        xbox_pc_kernel_image image;
        int result = xbox_pc_find_kernel_module(multiboot_magic,
                                                multiboot_info_address,
                                                &image);
        if (result == XBOX_PC_BOOT_OK) {
            serial_write("XBOX-BOOT: xboxkrnl.exe PE32 module validated\n");
            serial_write("XBOX-BOOT: preferred base=");
            serial_hex(image.preferred_base);
            serial_write(" entry RVA=");
            serial_hex(image.entry_rva);
            serial_write(" image size=");
            serial_hex(image.image_size);
            serial_write(" sections=");
            serial_hex(image.section_count);
            serial_write("\n");
            serial_write("XBOX-BOOT: validation only; no kernel jump performed\n");
        } else if (result == XBOX_PC_BOOT_NO_KERNEL) {
            serial_write("XBOX-BOOT: xboxkrnl.exe module not supplied\n");
        } else if (result == XBOX_PC_BOOT_BAD_PE) {
            serial_write("XBOX-BOOT: supplied xboxkrnl.exe failed PE32 validation\n");
        } else {
            serial_write("XBOX-BOOT: invalid Multiboot boot contract\n");
        }
    }
    serial_write("PC-HARNESS: stopping before Xbox kernel execution\n");
    for (;;) __asm__ volatile ("cli; hlt");
}

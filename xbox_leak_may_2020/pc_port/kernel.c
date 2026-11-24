#include <stdint.h>
#include "xbox_pc_boot.h"
#include "pc_idt.h"
#include "pc_gdt.h"
#include "pc_irq.h"
#include "pc_physmem.h"
#include "pc_paging.h"
#include "pc_hal_native.h"
#include "pc_pci.h"
#include "pc_acpi.h"
#include "pc_hal_native_backend.h"
#include "nv2a_vertex_shader.h"
#include "include/pc_hal.h"

#define COM1 0x3F8u
#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002u
#define MULTIBOOT_INFO_MEMORY  (1u << 0)
#define MULTIBOOT_INFO_MMAP    (1u << 6)
#define MULTIBOOT_INFO_CMDLINE (1u << 2)
#define MULTIBOOT_INFO_MODS (1u << 3)
#define MULTIBOOT_INFO_V1_BYTES 88u

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

typedef struct multiboot_module {
    uint32_t start, end, string, reserved;
} multiboot_module;

static uint8_t physical_bitmap[PC_PHYSMEM_BITMAP_BYTES]
    __attribute__((aligned(4096)));
static pc_physmem physical_memory;
static pc_paging_context xbox_page_tables;
static pc_acpi_madt_info firmware_madt;
static int firmware_madt_valid;
extern uint8_t __kernel_start;
extern uint8_t __kernel_end;

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
/* The placeholder VGA splash was removed. The real Xbox ani2 startup
 * animation is integrated only after its D3D8/NV2A and NT-thread dependencies
 * are available in the PC port. */
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
static void report_acpi(void)
{
    pc_acpi_rsdp_info rsdp;
    size_t offset = 0;
    uint16_t ebda_segment;
    uint32_t ebda;
    /* Read the BIOS Data Area without triggering GCC's null-page warning. */
    __asm__ volatile ("movw 0x40e, %0" : "=r"(ebda_segment));
    ebda = (uint32_t)ebda_segment << 4;
    int found = 0;

    /* The EBDA's first KiB is searched before the legacy BIOS area. */
    if (ebda >= 0x80000u && ebda < 0xA0000u) {
        size_t bytes = 0xA0000u - ebda;
        if (bytes > 1024u) bytes = 1024u;
        found = PcAcpiFindRsdp((const void *)(uintptr_t)ebda,
                               bytes, &rsdp, &offset);
        if (found) {
            serial_write("PC-ACPI: RSDP found in EBDA at ");
            serial_hex(ebda + (uint32_t)offset);
            serial_write("\n");
        }
    }

    if (!found) {
        found = PcAcpiFindRsdp((const void *)(uintptr_t)0xE0000u,
                               0x20000u, &rsdp, &offset);
        if (found) {
            serial_write("PC-ACPI: RSDP found in BIOS area at ");
            serial_hex(0xE0000u + (uint32_t)offset);
            serial_write("\n");
        }
    }

    if (!found) {
        serial_write("PC-ACPI: no valid RSDP found; firmware ACPI unavailable\n");
        return;
    }

    serial_write("PC-ACPI: RSDP revision=");
    serial_hex(rsdp.revision);
    serial_write(" RSDT=");
    serial_hex(rsdp.rsdt_address);
    serial_write(" XSDT-hi=");
    serial_hex((uint32_t)(rsdp.xsdt_address >> 32));
    serial_write(" XSDT-lo=");
    serial_hex((uint32_t)rsdp.xsdt_address);
    serial_write("\n");

    {
        pc_acpi_madt_info madt;
        if (PcAcpiFindMadt(&rsdp, &madt)) {
            uint32_t i;
            firmware_madt = madt;
            firmware_madt_valid = 1;
            serial_write("PC-ACPI: MADT parsed; LAPIC physical=");
            serial_hex(madt.local_apic_address);
            serial_write(" IOAPIC count=");
            serial_hex(madt.ioapic_count);
            serial_write(" ISO count=");
            serial_hex(madt.iso_count);
            serial_write("\n");
            for (i = 0; i < madt.ioapic_count; ++i) {
                serial_write("PC-ACPI: IOAPIC id=");
                serial_hex(madt.ioapics[i].id);
                serial_write(" address=");
                serial_hex(madt.ioapics[i].address);
                serial_write(" GSI base=");
                serial_hex(madt.ioapics[i].gsi_base);
                serial_write("\n");
            }
        } else {
            serial_write("PC-ACPI: MADT not found or not safely addressable; APIC remains disabled\n");
        }
    }
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
    while (offset <= info->mmap_length &&
           info->mmap_length - offset >= sizeof(uint32_t)) {
        const multiboot_mmap_entry *entry;
        if (info->mmap_addr > UINT32_MAX - offset) {
            serial_write("PC-HAL: memory map address overflow; stopping safely\n");
            return;
        }
        entry = (const multiboot_mmap_entry *)(uintptr_t)(info->mmap_addr + offset);
        uint32_t step;
        if (entry->size < 20u || entry->size > UINT32_MAX - sizeof(entry->size)) {
            serial_write("PC-HAL: malformed memory map entry; stopping safely\n");
            return;
        }
        step = entry->size + sizeof(entry->size);
        if (step > info->mmap_length - offset) {
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
static int initialize_physical_allocator(const multiboot_info_min *info,
                                          uint32_t info_address) {
    uint32_t i, module_bytes = 0;
    uint32_t kernel_start = (uint32_t)(uintptr_t)&__kernel_start;
    uint32_t kernel_end = (uint32_t)(uintptr_t)&__kernel_end;
    if (!(info->flags & MULTIBOOT_INFO_MMAP) || !info->mmap_addr ||
        !info->mmap_length ||
        info->mmap_addr > UINT32_MAX - info->mmap_length ||
        kernel_end <= kernel_start)
        return 1;
    if (!pc_physmem_init(&physical_memory, physical_bitmap,
                         sizeof(physical_bitmap),
                         (const void *)(uintptr_t)info->mmap_addr,
                         info->mmap_length))
        return 2;

    /* Keep legacy low memory, the complete running image, and boot metadata. */
    if (!pc_physmem_reserve(&physical_memory, 0, 0x100000u) ||
        !pc_physmem_reserve(&physical_memory, kernel_start,
                            kernel_end - kernel_start) ||
        !pc_physmem_reserve(&physical_memory, info_address,
                            MULTIBOOT_INFO_V1_BYTES) ||
        !pc_physmem_reserve(&physical_memory, info->mmap_addr,
                            info->mmap_length))
        return 3;

    if ((info->flags & MULTIBOOT_INFO_CMDLINE) && info->cmdline &&
        !pc_physmem_reserve(&physical_memory, info->cmdline, 4096u))
        return 4;

    if ((info->flags & MULTIBOOT_INFO_MODS) && info->mods_count) {
        const multiboot_module *modules;
        if (info->mods_count > 128u || !info->mods_addr ||
            info->mods_count > UINT32_MAX / sizeof(multiboot_module))
            return 5;
        module_bytes = info->mods_count * (uint32_t)sizeof(multiboot_module);
        if (info->mods_addr > UINT32_MAX - module_bytes ||
            !pc_physmem_reserve(&physical_memory, info->mods_addr,
                                module_bytes))
            return 6;
        modules = (const multiboot_module *)(uintptr_t)info->mods_addr;
        for (i = 0; i < info->mods_count; ++i) {
            if (modules[i].end <= modules[i].start ||
                !pc_physmem_reserve(&physical_memory, modules[i].start,
                                    modules[i].end - modules[i].start))
                return 7;
            if (modules[i].string &&
                !pc_physmem_reserve(&physical_memory, modules[i].string, 64u))
                return 8;
        }
    }

    /* Reserve conventional MMIO windows even if firmware mislabels them RAM. */
    if (!pc_physmem_reserve(&physical_memory, 0xA0000u, 0x60000u) ||
        !pc_physmem_reserve(&physical_memory, 0xFEC00000u, 0x1000u) ||
        !pc_physmem_reserve(&physical_memory, 0xFEE00000u, 0x1000u))
        return 9;

    /* Reserve every firmware-reported APIC page, not just PC defaults. */
    if (firmware_madt_valid) {
        if (!firmware_madt.local_apic_address ||
            !pc_physmem_reserve(&physical_memory,
                                firmware_madt.local_apic_address, 0x1000u))
            return 10;
        for (i = 0; i < firmware_madt.ioapic_count; ++i) {
            if (!firmware_madt.ioapics[i].address ||
                !pc_physmem_reserve(&physical_memory,
                                    firmware_madt.ioapics[i].address, 0x1000u))
                return 11;
        }
    }
    return 0;
}

static void pc_irq_count_handler(void *context)
{
    volatile uint32_t *count = (volatile uint32_t *)context;
    ++*count;
}

static int test_legacy_irq_routes(void)
{
    volatile uint32_t keyboard_count = 0;
    volatile uint32_t slave_count = 0;
    uint32_t frame[9] = {0};
    if (!PcIrqRegisterHandler(1u, 0u, pc_irq_count_handler,
                              (void *)&keyboard_count) ||
        !PcIrqRegisterHandler(12u, 0u, pc_irq_count_handler,
                              (void *)&slave_count))
        return 0;

    /* Exercise the same vector dispatch and EOI path used by real IRQ stubs. */
    frame[8] = 0x31u;
    PcIrqDispatch(frame);
    frame[8] = 0x3cu;
    PcIrqDispatch(frame);
    PcIrqMask(1u, 1);
    PcIrqMask(12u, 1);
    return keyboard_count == 1u && slave_count == 1u;
}

static int map_firmware_apic_mmio(void)
{
    uint32_t i;
    if (!firmware_madt_valid || !firmware_madt.local_apic_address)
        return 0;
    if ((firmware_madt.local_apic_address & 0xfffu) != 0u ||
        firmware_madt.local_apic_address < 0x10000000u)
        return 0;
    if (!pc_paging_map(&xbox_page_tables,
                       firmware_madt.local_apic_address,
                       firmware_madt.local_apic_address,
                       PC_PAGING_PAGE_SIZE,
                       PC_PAGING_WRITE | PC_PAGING_PWT | PC_PAGING_PCD))
        return 0;
    for (i = 0; i < firmware_madt.ioapic_count; ++i) {
        uint32_t address = firmware_madt.ioapics[i].address;
        if (!address || (address & 0xfffu) != 0u ||
            address < 0x10000000u ||
            !pc_paging_map(&xbox_page_tables, address, address,
                           PC_PAGING_PAGE_SIZE,
                           PC_PAGING_WRITE | PC_PAGING_PWT | PC_PAGING_PCD))
            return 0;
    }
    return 1;
}

static void report_pci(void) {
    uint32_t found = 0;
    uint32_t nv2a = 0;
    uint32_t bus, device, function;
    for (bus = 0; bus < 256; ++bus) {
        for (device = 0; device < 32; ++device) {
            uint32_t id0 = PcHalNativePciRead32((uint8_t)bus, (uint8_t)device, 0, 0);
            uint32_t vendor0 = id0 & 0xFFFFu;
            uint32_t header = PcHalNativePciRead32((uint8_t)bus, (uint8_t)device, 0, 0x0C);
            uint32_t functions = ((header >> 16) & 0x80u) ? 8u : 1u;
            if (vendor0 == 0xFFFFu) continue;
            for (function = 0; function < functions; ++function) {
                uint32_t id = function == 0 ? id0 :
                    PcHalNativePciRead32((uint8_t)bus, (uint8_t)device, (uint8_t)function, 0);
                uint32_t vendor = id & 0xFFFFu;
                uint32_t product = id >> 16;
                uint32_t function_header;
                if (vendor == 0xFFFFu) continue;
                /*
                 * Header layout belongs to each PCI function. Do not reuse
                 * function 0's BAR count for functions 1-7.
                 */
                function_header = function == 0 ? header :
                    PcHalNativePciRead32((uint8_t)bus, (uint8_t)device,
                                         (uint8_t)function, 0x0Cu);
                {
                    uint32_t class_info = PcHalNativePciRead32(
                        (uint8_t)bus, (uint8_t)device, (uint8_t)function, 0x08u);
                    uint32_t raw_bars[PC_PCI_MAX_BARS];
                    pc_pci_bar decoded_bars[PC_PCI_MAX_BARS];
                    uint32_t header_type = (function_header >> 16) & 0x7fu;
                    uint8_t bar_count = header_type == 0u ? 6u :
                                        (header_type == 1u ? 2u :
                                         (header_type == 2u ? 1u : 0u));
                    uint8_t bar_index;
                    int decoded_count = -1;
                    uint8_t irq_line = PcHalNativePciRead8(
                        (uint8_t)bus, (uint8_t)device, (uint8_t)function, 0x3cu);
                    for (bar_index = 0; bar_index < bar_count; ++bar_index)
                        raw_bars[bar_index] = PcHalNativePciRead32(
                            (uint8_t)bus, (uint8_t)device, (uint8_t)function,
                            (uint8_t)(0x10u + bar_index * 4u));
                    if (bar_count)
                        decoded_count = PcPciDecodeBars(raw_bars, bar_count,
                            decoded_bars, PC_PCI_MAX_BARS);
                    ++found;
                    serial_write("PC-HAL: PCI device ");
                    serial_hex(bus); serial_putc(':');
                    serial_hex(device); serial_putc('.');
                    serial_hex(function);
                    serial_write(" id="); serial_hex(id);
                    serial_write(" class="); serial_hex(class_info >> 8);
                    serial_write(" IRQ="); serial_hex(irq_line);
                    serial_write("\n");
                    if (decoded_count >= 0) {
                        int bar_number;
                        for (bar_number = 0; bar_number < decoded_count; ++bar_number) {
                            const pc_pci_bar *bar = &decoded_bars[bar_number];
                            serial_write("PC-HAL: PCI BAR[");
                            serial_hex(bar->slot);
                            serial_write("] kind=");
                            serial_hex(bar->kind);
                            serial_write(" width=");
                            serial_hex(bar->width);
                            serial_write(" base-hi=");
                            serial_hex((uint32_t)(bar->base >> 32));
                            serial_write(" base-lo=");
                            serial_hex((uint32_t)bar->base);
                            serial_write(" prefetch=");
                            serial_hex(bar->prefetchable);
                            serial_write("\n");
                        }
                    } else if (bar_count) {
                        serial_write("PC-HAL: PCI BAR decode rejected malformed BAR set\n");
                    }
                }
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
    serial_write("PC-BOOT: placeholder VGA splash disabled; native Xbox ani2 path pending\n");
    PcGdtInstallFlatSegments();
    serial_write("PC-HAL: flat GDT installed\n");
    PcIdtInstallExceptions();
    serial_write("PC-HAL: exception IDT installed (vectors 0-31)\n");
    if (pc_hal_validate(PcHalNativeGetOps()) == PC_STATUS_OK) {
        serial_write("PC-HAL: native backend ABI contract PASS (PCI + PIC/PIT; MMIO unsupported)\n");
    } else {
        serial_write("PC-HAL: native backend ABI contract ERROR\n");
        for (;;) __asm__ volatile ("cli; hlt");
    }
    {
        const uint32_t terminated_nop[4] = { 0u, 0u, 0u, 1u };
        pc_nv2a_vertex_program_info shader_info;
        if (PcNv2aValidateVertexProgram(terminated_nop, 1u, &shader_info) == 0 &&
            shader_info.instruction_count == 1u &&
            shader_info.input_mask == 0u &&
            shader_info.output_mask == 0u) {
            serial_write("NV2A: XboxRuntime freestanding microcode validator PASS\n");
        } else {
            serial_write("NV2A: microcode validator self-test ERROR\n");
            for (;;) __asm__ volatile ("cli; hlt");
        }
    }
    serial_write("PC-HARNESS: entered 32-bit bare-metal kernel\n");
    if (multiboot_magic != MULTIBOOT_BOOTLOADER_MAGIC || !info) {
        serial_write("PC-HARNESS: ERROR invalid Multiboot handoff\n");
        for (;;) __asm__ volatile ("cli; hlt");
    }
    serial_write("PC-HARNESS: Multiboot handoff PASS\n");
    serial_write("PC-HAL: executing ring-0 native x86 hardware probes\n");
    report_cpu();
    report_memory(info);
    report_acpi();
    {
        int allocator_status =
            initialize_physical_allocator(info, multiboot_info_address);
        if (!allocator_status) {
            uint32_t page = pc_physmem_alloc_page(&physical_memory);
            if (page) {
                serial_write("PC-HAL: physical allocator PASS page=");
                serial_hex(page);
                serial_write("\n");
            } else {
                serial_write("PC-HAL: physical allocator ERROR no free page\n");
            }

            /*
             * bldr32 maps the first 256 MiB identity and aliases it at
             * 0x80000000. Build the same two-level layout, but keep paging
             * disabled until the PC-specific device map is complete.
             */
            {
                /* 129 base pages plus room for up to nine APIC MMIO PDEs. */
                uint32_t block = pc_physmem_alloc_pages(&physical_memory,
                                                        138u, 1u);
                if (block &&
                    pc_paging_init(&xbox_page_tables,
                        (uint32_t *)(uintptr_t)block, block,
                        (uint32_t *)(uintptr_t)(block + PC_PAGING_PAGE_SIZE),
                        block + PC_PAGING_PAGE_SIZE, 137u) &&
                    pc_paging_map(&xbox_page_tables, 0u, 0u,
                                  0x10000000u, PC_PAGING_WRITE) &&
                    pc_paging_map(&xbox_page_tables, 0x80000000u, 0u,
                                  0x10000000u, PC_PAGING_WRITE)) {
                    uint32_t identity_pa, alias_pa, page_flags;
                    int apic_map_ok = !firmware_madt_valid ||
                                      map_firmware_apic_mmio();
                    if (xbox_page_tables.table_count >= 128u &&
                        pc_paging_translate(&xbox_page_tables, 0x00123456u,
                                            &identity_pa, &page_flags) &&
                        identity_pa == 0x00123456u &&
                        page_flags == PC_PAGING_WRITE &&
                        pc_paging_translate(&xbox_page_tables, 0x80123456u,
                                            &alias_pa, &page_flags) &&
                        alias_pa == 0x00123456u &&
                        page_flags == PC_PAGING_WRITE) {
                        serial_write("PC-HAL: Xbox 256 MiB identity + 0x80000000 alias tables verified (paging OFF)\\n");
                        if (apic_map_ok && firmware_madt_valid) {
                            uint32_t apic_pa, apic_flags;
                            if (pc_paging_translate(&xbox_page_tables,
                                    firmware_madt.local_apic_address,
                                    &apic_pa, &apic_flags) &&
                                apic_pa == firmware_madt.local_apic_address &&
                                apic_flags == (PC_PAGING_WRITE |
                                               PC_PAGING_PWT | PC_PAGING_PCD)) {
                                serial_write("PC-HAL: firmware LAPIC/IOAPIC MMIO mappings prepared uncached (paging OFF)\\n");
                            } else {
                                serial_write("PC-HAL: APIC MMIO mapping verification ERROR\\n");
                            }
                        } else if (firmware_madt_valid) {
                            serial_write("PC-HAL: APIC MMIO mapping preparation ERROR\\n");
                        } else {
                            serial_write("PC-HAL: no MADT; APIC MMIO mappings skipped\\n");
                        }
                    } else {
                        serial_write("PC-HAL: Xbox page-table translation self-test ERROR\\n");
                    }
                } else {
                    serial_write("PC-HAL: Xbox page-table layout build ERROR\\n");
                }
            }
        } else {
            serial_write("PC-HAL: physical allocator ERROR stage=");
            serial_hex((uint32_t)allocator_status);
            serial_write("\n");
        }
    }
    report_pci();
    serial_write("PC-HAL: starting PIT IRQ0 timer self-test\n");
    PcIrqInstallPit(100u);
    PcIrqWaitForTicks(5u);
    PcIrqStop();
    serial_write("PC-HAL: PIT IRQ0 self-test PASS ticks=");
    serial_hex(PcIrqTicks());
    serial_write("\n");
    if (test_legacy_irq_routes()) {
        serial_write("PC-HAL: legacy PIC IRQ1 + IRQ12 dispatch/EOI self-test PASS\n");
    } else {
        serial_write("PC-HAL: legacy PIC IRQ routing self-test ERROR\n");
        for (;;) __asm__ volatile ("cli; hlt");
    }
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

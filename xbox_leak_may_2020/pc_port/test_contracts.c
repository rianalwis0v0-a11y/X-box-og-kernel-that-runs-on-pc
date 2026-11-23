#include "include/pc_hal.h"
#include "include/nv2a_pc_bridge.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static pc_status map_mmio(void *c, uint64_t b, uint64_t n, volatile void **v)
{ (void)c; (void)b; (void)n; (void)v; return PC_STATUS_UNSUPPORTED; }
static pc_status pci_find(void *c, uint16_t v, uint16_t d, pc_pci_device *o)
{ (void)c; (void)v; (void)d; (void)o; return PC_STATUS_UNSUPPORTED; }
static pc_status pci_read(void *c, const pc_pci_device *d, uint16_t o, uint32_t *v)
{ (void)c; (void)d; (void)o; (void)v; return PC_STATUS_UNSUPPORTED; }
static pc_status pci_write(void *c, const pc_pci_device *d, uint16_t o, uint32_t v)
{ (void)c; (void)d; (void)o; (void)v; return PC_STATUS_UNSUPPORTED; }
static pc_status irq_register(void *c, uint32_t g, pc_irq_trigger t, void (*h)(void *), void *x)
{ (void)c; (void)g; (void)t; (void)h; (void)x; return PC_STATUS_UNSUPPORTED; }
static pc_status irq_mask(void *c, uint32_t g, int m)
{ (void)c; (void)g; (void)m; return PC_STATUS_UNSUPPORTED; }
static pc_status clock_ns(void *c, uint64_t *n)
{ (void)c; (void)n; return PC_STATUS_UNSUPPORTED; }
static pc_status sleep_ns(void *c, uint64_t n)
{ (void)c; (void)n; return PC_STATUS_UNSUPPORTED; }

static pc_status create_surface(void *c, const nv2a_pc_surface_desc *d, uint64_t *s)
{ (void)c; (void)d; (void)s; return PC_STATUS_UNSUPPORTED; }
static pc_status upload_surface(void *c, uint64_t s, const void *p, size_t n)
{ (void)c; (void)s; (void)p; (void)n; return PC_STATUS_UNSUPPORTED; }
static pc_status translate_shader(void *c, const uint32_t *t, size_t n, uint64_t *s)
{ (void)c; (void)t; (void)n; (void)s; return PC_STATUS_UNSUPPORTED; }
static pc_status draw_indexed(void *c, uint64_t s, const void *v, size_t b, const uint32_t *i, size_t n)
{ (void)c; (void)s; (void)v; (void)b; (void)i; (void)n; return PC_STATUS_UNSUPPORTED; }
static pc_status present(void *c, uint64_t s)
{ (void)c; (void)s; return PC_STATUS_UNSUPPORTED; }
static void destroy_surface(void *c, uint64_t s) { (void)c; (void)s; }

int main(void)
{
    pc_hal_ops hal;
    nv2a_pc_host_ops host;
    nv2a_pc_bridge bridge;
    memset(&hal, 0, sizeof(hal));
    memset(&host, 0, sizeof(host));
    memset(&bridge, 0, sizeof(bridge));

    assert(pc_hal_validate(0) == PC_STATUS_INVALID_ARGUMENT);
    assert(pc_hal_validate(&hal) == PC_STATUS_UNSUPPORTED);
    hal.abi_version = PC_HAL_ABI_VERSION;
    hal.struct_size = sizeof(hal);
    hal.map_mmio = map_mmio; hal.pci_find = pci_find;
    hal.pci_read32 = pci_read; hal.pci_write32 = pci_write;
    hal.irq_register = irq_register; hal.irq_mask = irq_mask;
    hal.monotonic_time_ns = clock_ns; hal.sleep_ns = sleep_ns;
    assert(pc_hal_validate(&hal) == PC_STATUS_OK);

    assert(nv2a_pc_bridge_validate(0) == PC_STATUS_INVALID_ARGUMENT);
    bridge.abi_version = NV2A_PC_BRIDGE_ABI_VERSION;
    bridge.struct_size = sizeof(bridge);
    bridge.host = &host;
    assert(nv2a_pc_bridge_validate(&bridge) == PC_STATUS_UNSUPPORTED);
    host.abi_version = NV2A_PC_BRIDGE_ABI_VERSION;
    host.struct_size = sizeof(host);
    host.create_surface = create_surface;
    host.upload_surface = upload_surface;
    host.translate_shader = translate_shader;
    host.draw_indexed = draw_indexed;
    host.present = present;
    host.destroy_surface = destroy_surface;
    assert(nv2a_pc_bridge_validate(&bridge) == PC_STATUS_OK);
    puts("PC-HAL/NV2A-BRIDGE: contract tests PASS");
    return 0;
}

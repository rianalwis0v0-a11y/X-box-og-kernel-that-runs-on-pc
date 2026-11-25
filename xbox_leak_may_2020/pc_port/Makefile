CROSS ?= i686-elf-
CC := $(CROSS)gcc
AS := $(CROSS)gcc
LD := $(CROSS)gcc

CFLAGS := -m32 -std=c11 -ffreestanding -fno-pie -fno-stack-protector -fno-builtin -Wall -Wextra -Werror -O2
LDFLAGS := -m32 -nostdlib -no-pie -Wl,-T,linker.ld -Wl,--build-id=none

.PHONY: all clean check-tools iso
all: pc-harness.elf iso

check-tools:
	@command -v $(CC) >/dev/null || { echo "Missing $(CC); install an i686-elf GCC toolchain"; exit 1; }

boot.o: boot.S | check-tools
	$(AS) $(CFLAGS) -c $< -o $@

kernel.o: kernel.c xbox_pc_boot.h pc_idt.h pc_gdt.h pc_irq.h pc_paging.h pc_physmem.h pc_hal_native.h pc_hal_native_backend.h include/pc_hal.h pc_pci.h pc_intel_gpu.h pc_acpi.h | check-tools
	$(CC) $(CFLAGS) -c $< -o $@

pc_paging.o: pc_paging.c pc_paging.h | check-tools
	$(CC) $(CFLAGS) -c $< -o $@

pc_gdt.o: pc_gdt.c pc_gdt.h | check-tools
	$(CC) $(CFLAGS) -c $< -o $@

pc_irq.o: pc_irq.c pc_irq.h | check-tools
	$(CC) $(CFLAGS) -c $< -o $@

irq_stubs.o: irq_stubs.S | check-tools
	$(AS) $(CFLAGS) -c $< -o $@

pc_idt.o: pc_idt.c pc_idt.h | check-tools
	$(CC) $(CFLAGS) -c $< -o $@

exception_stubs.o: exception_stubs.S | check-tools
	$(AS) $(CFLAGS) -c $< -o $@

xbox_pc_boot.o: xbox_pc_boot.c xbox_pc_boot.h | check-tools
	$(CC) $(CFLAGS) -c $< -o $@

pc_physmem.o: pc_physmem.c pc_physmem.h | check-tools
	$(CC) $(CFLAGS) -c $< -o $@

pc_hal_native.o: pc_hal_native.c pc_hal_native.h | check-tools
	$(CC) $(CFLAGS) -c $< -o $@

pc_hal_contract.o: pc_hal_contract.c include/pc_hal.h | check-tools
	$(CC) $(CFLAGS) -c $< -o $@

pc_hal_native_backend.o: pc_hal_native_backend.c pc_hal_native_backend.h include/pc_hal.h pc_hal_native.h pc_irq.h | check-tools
	$(CC) $(CFLAGS) -c $< -o $@

i915_winhal_compat.o: i915_winhal_compat.c i915_winhal_compat.h include/pc_hal.h | check-tools
	$(CC) $(CFLAGS) -c $< -o $@

nv2a_vertex_shader.o: nv2a_vertex_shader.c nv2a_vertex_shader.h | check-tools
	$(CC) $(CFLAGS) -c $< -o $@

pc_pci.o: pc_pci.c pc_pci.h | check-tools
	$(CC) $(CFLAGS) -c $< -o $@

pc_intel_gpu.o: pc_intel_gpu.c pc_intel_gpu.h pc_pci.h | check-tools
	$(CC) $(CFLAGS) -c $< -o $@

pc_intel_gen9.o: pc_intel_gen9.c pc_intel_gen9.h pc_intel_gpu.h | check-tools
	$(CC) $(CFLAGS) -c $< -o $@

pc_intel_driver.o: pc_intel_driver.c pc_intel_driver.h i915_winhal_compat.h pc_intel_gen9.h pc_intel_gpu.h pc_pci.h | check-tools
	$(CC) $(CFLAGS) -c $< -o $@

pc_acpi.o: pc_acpi.c pc_acpi.h | check-tools
	$(CC) $(CFLAGS) -c $< -o $@

pc_apic.o: pc_apic.c pc_apic.h pc_acpi.h | check-tools
	$(CC) $(CFLAGS) -c $< -o $@

pc-harness.elf: boot.o kernel.o nv2a_vertex_shader.o xbox_pc_boot.o pc_paging.o pc_physmem.o pc_hal_native.o pc_hal_contract.o pc_hal_native_backend.o i915_winhal_compat.o pc_pci.o pc_intel_gpu.o pc_intel_gen9.o pc_intel_driver.o pc_acpi.o pc_apic.o pc_gdt.o pc_idt.o pc_irq.o exception_stubs.o irq_stubs.o linker.ld
	$(LD) $(LDFLAGS) -o $@ boot.o kernel.o nv2a_vertex_shader.o xbox_pc_boot.o pc_paging.o pc_physmem.o pc_hal_native.o pc_hal_contract.o pc_hal_native_backend.o i915_winhal_compat.o pc_pci.o pc_intel_gpu.o pc_intel_gen9.o pc_intel_driver.o pc_acpi.o pc_apic.o pc_gdt.o pc_idt.o pc_irq.o exception_stubs.o irq_stubs.o
	@file $@

iso: pc-harness.elf grub.cfg
	@command -v grub-mkrescue >/dev/null || { echo "Missing grub-mkrescue (install grub-pc-bin and xorriso)"; exit 1; }
	rm -rf iso-root
	mkdir -p iso-root/boot/grub
	cp pc-harness.elf iso-root/boot/pc-harness.elf
	cp grub.cfg iso-root/boot/grub/grub.cfg
	grub-mkrescue -o pc-port-harness.iso iso-root

clean:
	rm -rf iso-root
	rm -f boot.o kernel.o nv2a_vertex_shader.o xbox_pc_boot.o pc_paging.o pc_physmem.o pc_hal_native.o pc_hal_contract.o pc_hal_native_backend.o i915_winhal_compat.o pc_pci.o pc_intel_gpu.o pc_intel_gen9.o pc_intel_driver.o pc_acpi.o pc_apic.o pc_gdt.o pc_idt.o pc_irq.o exception_stubs.o irq_stubs.o pc-harness.elf pc-port-harness.iso

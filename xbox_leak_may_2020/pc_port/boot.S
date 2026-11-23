/* Multiboot v1 entry: GRUB enters in 32-bit protected mode. */
.set MB_MAGIC,    0x1BADB002
.set MB_FLAGS,    0x00000003
.set MB_CHECKSUM, -(MB_MAGIC + MB_FLAGS)

.section .multiboot,"a"
.align 4
.long MB_MAGIC
.long MB_FLAGS
.long MB_CHECKSUM

.section .bss
.align 16
stack_bottom:
.skip 16384
stack_top:

.section .text
.global _start
.type _start, @function
.extern kernel_main
_start:
    cli
    mov $stack_top, %esp
    xor %ebp, %ebp
    /* Multiboot v1: EAX=boot magic, EBX=info pointer. */
    push %ebx
    push %eax
    call kernel_main
1:
    cli
    hlt
    jmp 1b
.size _start, . - _start

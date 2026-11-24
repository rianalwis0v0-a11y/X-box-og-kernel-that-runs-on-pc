BITS 16
ORG 0x8000
stage2_entry:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    in al, 0x92
    or al, 0x02
    and al, 0xFE
    out 0x92, al
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:protected_entry

BITS 32
protected_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x0009F000
    cld
    mov dx, 0x3F9
    xor al, al
    out dx, al
    mov dx, 0x3FB
    mov al, 0x80
    out dx, al
    mov dx, 0x3F8
    mov al, 0x01
    out dx, al
    mov dx, 0x3F9
    xor al, al
    out dx, al
    mov dx, 0x3FB
    mov al, 0x03
    out dx, al
    mov dx, 0x3FA
    mov al, 0xC7
    out dx, al
    mov dx, 0x3FC
    mov al, 0x0B
    out dx, al
    mov esi, stage2_message
    call serial_puts
    mov esi, pending_message
    call serial_puts
.halt:
    cli
    hlt
    jmp .halt

serial_puts:
    lodsb
    test al, al
    jz .done
.wait:
    mov dx, 0x3FD
    in al, dx
    test al, 0x20
    jz .wait
    mov dx, 0x3F8
    mov al, [esi-1]
    out dx, al
    jmp serial_puts
.done:
    ret

align 8
gdt_start:
    dq 0
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF
gdt_end:
gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start
stage2_message: db 'PC Xbox bootloader: stage 2 entered protected mode.', 13, 10, 0
pending_message: db 'Kernel PE loading and Xbox handoff are NOT implemented yet.', 13, 10, 0

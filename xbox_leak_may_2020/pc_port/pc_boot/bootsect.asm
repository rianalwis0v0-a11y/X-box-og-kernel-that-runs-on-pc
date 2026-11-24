BITS 16
ORG 0x7C00
start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti
    mov [boot_drive], dl
    mov bx, 0x55AA
    mov ah, 0x41
    int 0x13
    jc disk_error
    cmp bx, 0xAA55
    jne disk_error
    test cx, 1
    jz disk_error
    mov si, dap
    mov dl, [boot_drive]
    mov ah, 0x42
    int 0x13
    jc disk_error
    jmp 0x0000:0x8000
disk_error:
    mov si, error_text
.print:
    lodsb
    test al, al
    jz .halt
    mov ah, 0x0E
    mov bx, 0x0007
    int 0x10
    jmp .print
.halt:
    cli
.hang:
    hlt
    jmp .hang
boot_drive: db 0
align 4
dap:
    db 0x10, 0
    dw 32
    dw 0x8000, 0
    dq 1
error_text: db 'PC boot: BIOS EDD disk read failed', 0
times 510-($-$$) db 0
dw 0xAA55

/* 32-bit protected-mode exception stubs. Error-code vectors are normalized. */
.section .text
.code32
.extern PcIdtExceptionDispatch
.global PcIsrStubTable
.align 4
PcIsrStubTable:
.long PcIsr0
.long PcIsr1
.long PcIsr2
.long PcIsr3
.long PcIsr4
.long PcIsr5
.long PcIsr6
.long PcIsr7
.long PcIsr8
.long PcIsr9
.long PcIsr10
.long PcIsr11
.long PcIsr12
.long PcIsr13
.long PcIsr14
.long PcIsr15
.long PcIsr16
.long PcIsr17
.long PcIsr18
.long PcIsr19
.long PcIsr20
.long PcIsr21
.long PcIsr22
.long PcIsr23
.long PcIsr24
.long PcIsr25
.long PcIsr26
.long PcIsr27
.long PcIsr28
.long PcIsr29
.long PcIsr30
.long PcIsr31

.global PcIsr0
PcIsr0:
    pushl $0
    pushl $0
    jmp PcIsrCommon
.global PcIsr1
PcIsr1:
    pushl $0
    pushl $1
    jmp PcIsrCommon
.global PcIsr2
PcIsr2:
    pushl $0
    pushl $2
    jmp PcIsrCommon
.global PcIsr3
PcIsr3:
    pushl $0
    pushl $3
    jmp PcIsrCommon
.global PcIsr4
PcIsr4:
    pushl $0
    pushl $4
    jmp PcIsrCommon
.global PcIsr5
PcIsr5:
    pushl $0
    pushl $5
    jmp PcIsrCommon
.global PcIsr6
PcIsr6:
    pushl $0
    pushl $6
    jmp PcIsrCommon
.global PcIsr7
PcIsr7:
    pushl $0
    pushl $7
    jmp PcIsrCommon
.global PcIsr8
PcIsr8:
    pushl $8
    jmp PcIsrCommon
.global PcIsr9
PcIsr9:
    pushl $0
    pushl $9
    jmp PcIsrCommon
.global PcIsr10
PcIsr10:
    pushl $10
    jmp PcIsrCommon
.global PcIsr11
PcIsr11:
    pushl $11
    jmp PcIsrCommon
.global PcIsr12
PcIsr12:
    pushl $12
    jmp PcIsrCommon
.global PcIsr13
PcIsr13:
    pushl $13
    jmp PcIsrCommon
.global PcIsr14
PcIsr14:
    pushl $14
    jmp PcIsrCommon
.global PcIsr15
PcIsr15:
    pushl $0
    pushl $15
    jmp PcIsrCommon
.global PcIsr16
PcIsr16:
    pushl $0
    pushl $16
    jmp PcIsrCommon
.global PcIsr17
PcIsr17:
    pushl $17
    jmp PcIsrCommon
.global PcIsr18
PcIsr18:
    pushl $0
    pushl $18
    jmp PcIsrCommon
.global PcIsr19
PcIsr19:
    pushl $0
    pushl $19
    jmp PcIsrCommon
.global PcIsr20
PcIsr20:
    pushl $0
    pushl $20
    jmp PcIsrCommon
.global PcIsr21
PcIsr21:
    pushl $21
    jmp PcIsrCommon
.global PcIsr22
PcIsr22:
    pushl $0
    pushl $22
    jmp PcIsrCommon
.global PcIsr23
PcIsr23:
    pushl $0
    pushl $23
    jmp PcIsrCommon
.global PcIsr24
PcIsr24:
    pushl $0
    pushl $24
    jmp PcIsrCommon
.global PcIsr25
PcIsr25:
    pushl $0
    pushl $25
    jmp PcIsrCommon
.global PcIsr26
PcIsr26:
    pushl $0
    pushl $26
    jmp PcIsrCommon
.global PcIsr27
PcIsr27:
    pushl $0
    pushl $27
    jmp PcIsrCommon
.global PcIsr28
PcIsr28:
    pushl $0
    pushl $28
    jmp PcIsrCommon
.global PcIsr29
PcIsr29:
    pushl $29
    jmp PcIsrCommon
.global PcIsr30
PcIsr30:
    pushl $30
    jmp PcIsrCommon
.global PcIsr31
PcIsr31:
    pushl $0
    pushl $31
    jmp PcIsrCommon

PcIsrCommon:
    cld
    pushal
    pushl %esp
    call PcIdtExceptionDispatch
1:  cli
    hlt
    jmp 1b

/* Legacy 8259 IRQ entry stubs, mapped to vectors 0x30-0x3f. */
.section .text
.code32
.extern PcIrqDispatch
.global PcIrqStubTable
.align 4
PcIrqStubTable:
.long PcIrq0
.long PcIrq1
.long PcIrq2
.long PcIrq3
.long PcIrq4
.long PcIrq5
.long PcIrq6
.long PcIrq7
.long PcIrq8
.long PcIrq9
.long PcIrq10
.long PcIrq11
.long PcIrq12
.long PcIrq13
.long PcIrq14
.long PcIrq15

.global PcIrq0
PcIrq0:
    pushl $0
    pushl $48
    jmp PcIrqCommon
.global PcIrq1
PcIrq1:
    pushl $0
    pushl $49
    jmp PcIrqCommon
.global PcIrq2
PcIrq2:
    pushl $0
    pushl $50
    jmp PcIrqCommon
.global PcIrq3
PcIrq3:
    pushl $0
    pushl $51
    jmp PcIrqCommon
.global PcIrq4
PcIrq4:
    pushl $0
    pushl $52
    jmp PcIrqCommon
.global PcIrq5
PcIrq5:
    pushl $0
    pushl $53
    jmp PcIrqCommon
.global PcIrq6
PcIrq6:
    pushl $0
    pushl $54
    jmp PcIrqCommon
.global PcIrq7
PcIrq7:
    pushl $0
    pushl $55
    jmp PcIrqCommon
.global PcIrq8
PcIrq8:
    pushl $0
    pushl $56
    jmp PcIrqCommon
.global PcIrq9
PcIrq9:
    pushl $0
    pushl $57
    jmp PcIrqCommon
.global PcIrq10
PcIrq10:
    pushl $0
    pushl $58
    jmp PcIrqCommon
.global PcIrq11
PcIrq11:
    pushl $0
    pushl $59
    jmp PcIrqCommon
.global PcIrq12
PcIrq12:
    pushl $0
    pushl $60
    jmp PcIrqCommon
.global PcIrq13
PcIrq13:
    pushl $0
    pushl $61
    jmp PcIrqCommon
.global PcIrq14
PcIrq14:
    pushl $0
    pushl $62
    jmp PcIrqCommon
.global PcIrq15
PcIrq15:
    pushl $0
    pushl $63
    jmp PcIrqCommon

PcIrqCommon:
    cld
    pushal
    pushl %esp
    call PcIrqDispatch
    addl $4, %esp
    popal
    addl $8, %esp
    iret

/* Local-APIC spurious interrupts do not carry an error code and need no EOI. */
.global PcApicSpuriousStub
PcApicSpuriousStub:
    cld
    pushal
    popal
    iret

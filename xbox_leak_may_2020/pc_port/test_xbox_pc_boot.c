#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "xbox_pc_boot.h"

static uint8_t image[0x400];
static void put16(uint32_t o, uint16_t v) {
    image[o]=(uint8_t)v; image[o+1]=(uint8_t)(v>>8);
}
static void put32(uint32_t o, uint32_t v) {
    image[o]=(uint8_t)v; image[o+1]=(uint8_t)(v>>8);
    image[o+2]=(uint8_t)(v>>16); image[o+3]=(uint8_t)(v>>24);
}
static void make_image(void) {
    const uint32_t pe=0x80, coff=pe+4, opt=coff+20, sec=opt+224;
    memset(image,0,sizeof(image));
    put16(0,0x5a4d); put32(0x3c,pe); put32(pe,0x00004550);
    put16(coff,0x014c); put16(coff+2,1); put16(coff+16,224);
    put16(opt,0x010b); put32(opt+16,0x1000); put32(opt+28,0x80010000);
    put32(opt+32,0x1000); put32(opt+36,0x200);
    put32(opt+56,0x2000); put32(opt+60,0x200);
    memcpy(image+sec,".text",5);
    put32(sec+8,0x100); put32(sec+12,0x1000);
    put32(sec+16,0x200); put32(sec+20,0x200);
    put32(sec+36,0x60000020);
    memset(image+0x200,0x90,0x200);
}
int main(void) {
    xbox_pc_kernel_image out;
    make_image();
    assert(xbox_pc_validate_pe32_file(image,sizeof(image),&out));
    assert(out.entry_rva==0x1000 && out.section_count==1);

    make_image(); put32(0x80+24+16,0x1800);
    assert(!xbox_pc_validate_pe32_file(image,sizeof(image),&out));

    make_image(); put32(0x80+24+224+36,0x40000040);
    assert(!xbox_pc_validate_pe32_file(image,sizeof(image),&out));

    make_image(); put32(0x80+24+60,0x100);
    assert(!xbox_pc_validate_pe32_file(image,sizeof(image),&out));

    make_image(); put32(0x80+24+224+20,0x100);
    assert(!xbox_pc_validate_pe32_file(image,sizeof(image),&out));

    puts("XBOX-PC-BOOT: PE32 validator tests PASS");
    return 0;
}

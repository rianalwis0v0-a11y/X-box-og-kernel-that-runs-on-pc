#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "xbox_pc_boot.h"

static uint8_t image[0x600];
static void put16(uint32_t o, uint16_t v) {
    image[o]=(uint8_t)v; image[o+1]=(uint8_t)(v>>8);
}
static void put32(uint32_t o, uint32_t v) {
    image[o]=(uint8_t)v; image[o+1]=(uint8_t)(v>>8);
    image[o+2]=(uint8_t)(v>>16); image[o+3]=(uint8_t)(v>>24);
}
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8) |
           ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
static void make_image(void) {
    const uint32_t pe=0x80, coff=pe+4, opt=coff+20, sec=opt+224;
    memset(image,0,sizeof(image));
    put16(0,0x5a4d); put32(0x3c,pe); put32(pe,0x00004550);
    put16(coff,0x014c); put16(coff+2,1); put16(coff+16,224);
    put16(opt,0x010b); put32(opt+92,16); put32(opt+16,0x1000); put32(opt+28,0x80010000);
    put32(opt+32,0x1000); put32(opt+36,0x200);
    put32(opt+56,0x2000); put32(opt+60,0x200);
    memcpy(image+sec,".text",5);
    put32(sec+8,0x100); put32(sec+12,0x1000);
    put32(sec+16,0x200); put32(sec+20,0x200);
    put32(sec+36,0x60000020);
    memset(image+0x200,0x90,0x200);
}

static uint8_t loaded[0x3000];
static void make_reloc_image(void) {
    const uint32_t pe=0x80, coff=pe+4, opt=coff+20, sec=opt+224;
    make_image();
    put16(coff+2,2);
    put32(opt+56,0x3000);
    put32(opt+96+5*8,0x2000);
    put32(opt+96+5*8+4,12);
    memcpy(image+sec+40,".reloc",6);
    put32(sec+40+8,12); put32(sec+40+12,0x2000);
    put32(sec+40+16,0x200); put32(sec+40+20,0x400);
    put32(sec+40+36,0x42000040);
    put32(0x204,0x80123456);
    put32(0x400,0x1000); put32(0x404,12);
    put16(0x408,0x3004); put16(0x40a,0);
}
int main(void) {
    xbox_pc_kernel_image out;
    make_image();
    assert(xbox_pc_validate_pe32_file(image,sizeof(image),&out));
    assert(out.entry_rva==0x1000 && out.section_count==1);

    make_image(); put32(0x80+24+96+8,0x1500); put32(0x80+24+96+12,0x40);
    assert(!xbox_pc_validate_pe32_file(image,sizeof(image),&out));

    make_image(); put32(0x80+24+96+9*8,0x1600); put32(0x80+24+96+9*8+4,0x20);
    assert(!xbox_pc_validate_pe32_file(image,sizeof(image),&out));

    make_image(); put32(0x80+24+96+13*8,0x1700); put32(0x80+24+96+13*8+4,0x20);
    assert(!xbox_pc_validate_pe32_file(image,sizeof(image),&out));

    make_image(); put32(0x80+24+16,0x1800);
    assert(!xbox_pc_validate_pe32_file(image,sizeof(image),&out));

    make_image(); put32(0x80+24+224+36,0x40000040);
    assert(!xbox_pc_validate_pe32_file(image,sizeof(image),&out));

    make_image(); put32(0x80+24+60,0x100);
    assert(!xbox_pc_validate_pe32_file(image,sizeof(image),&out));

    make_image(); put32(0x80+24+224+20,0x100);
    assert(!xbox_pc_validate_pe32_file(image,sizeof(image),&out));


    {
        uint32_t entry=0;
        xbox_pc_kernel_image loaded_image;
        make_reloc_image();
        assert(xbox_pc_validate_pe32_file(image,sizeof(image),&loaded_image));
        assert(xbox_pc_load_pe32_image(&loaded_image,loaded,sizeof(loaded),
                                       0x80010000u,&entry));
        assert(entry==0x80011000u);
        assert(loaded[0x1000]==0x90);
        assert(get32(loaded+0x1004)==0x80123456u);
        assert(xbox_pc_load_pe32_image(&loaded_image,loaded,sizeof(loaded),
                                       0x81010000u,&entry));
        assert(get32(loaded+0x1004)==0x81123456u);
        assert(!xbox_pc_load_pe32_image(&loaded_image,loaded,
                                        (uint32_t)sizeof(loaded)-1u,
                                        0x80010000u,&entry));
        assert(!xbox_pc_load_pe32_image(&loaded_image,loaded,sizeof(loaded),
                                        0xfffff000u,&entry));
        loaded_image.entry_rva++;
        assert(!xbox_pc_load_pe32_image(&loaded_image,loaded,sizeof(loaded),
                                        0x80010000u,&entry));
        make_reloc_image();
        assert(xbox_pc_validate_pe32_file(image,sizeof(image),&loaded_image));
        put16(0x40a,0x5000); /* Unsupported relocation type must fail. */
        assert(xbox_pc_validate_pe32_file(image,sizeof(image),&loaded_image));
        assert(!xbox_pc_load_pe32_image(&loaded_image,loaded,sizeof(loaded),
                                        0x81010000u,&entry));
    }
    puts("XBOX-PC-BOOT: PE32 validator and mapper tests PASS");
    return 0;
}

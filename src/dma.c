#include <stdio.h>

#include "hexwar.h"
#include "dma.h"

byte dma_byte;

void mega65_io_enable(void)
{
    // Gate C65 IO enable
    POKE(0xd02fU, 0x47);
    POKE(0xd02fU, 0x53);
    // Force to full speed
    POKE(0, 65);
}

#define EXTENDED_COMMANDS \
    unsigned char option_0b; \
    unsigned char option_80; \
    unsigned char source_mb; \
    unsigned char option_81; \
    unsigned char dest_mb;

#define F018B_COMMANDS \
    unsigned char end_of_options; \
    unsigned char command; \
    unsigned int count; \
    unsigned int source_addr; \
    unsigned char source_bank; \
    unsigned int dest_addr; \
    unsigned char dest_bank; \
    unsigned char sub_cmd; \
    unsigned int modulo;

struct {
    EXTENDED_COMMANDS
    F018B_COMMANDS
} dma_peek_list;

struct {
    EXTENDED_COMMANDS
    F018B_COMMANDS
} dma_poke_list;

struct {
    EXTENDED_COMMANDS
    F018B_COMMANDS
} dma_copy_list;

struct {
    EXTENDED_COMMANDS
    unsigned char option_82;
    unsigned char source_skip_bit;
    unsigned char option_83;
    unsigned char source_skip_byte;
    unsigned char option_84;
    unsigned char dest_skip_bit;
    unsigned char option_85;
    unsigned char dest_skip_byte;
    F018B_COMMANDS
} dma_copy_skip_list;

struct {
    EXTENDED_COMMANDS
    F018B_COMMANDS
} dma_fill_list;

struct {
    EXTENDED_COMMANDS
    unsigned char option_82;
    unsigned char source_skip_bit;
    unsigned char option_83;
    unsigned char source_skip_byte;
    unsigned char option_84;
    unsigned char dest_skip_bit;
    unsigned char option_85;
    unsigned char dest_skip_byte;
    F018B_COMMANDS
} dma_fill_skip_list;

void dma_init() {
    dma_peek_list.option_0b = 0x0b;
    dma_peek_list.option_80 = 0x80;
    dma_peek_list.option_81 = 0x81;
    dma_peek_list.dest_mb = 0x00; // dma_byte lives in 1st MB
    dma_peek_list.end_of_options = 0x00;
    dma_peek_list.command = 0x00; // copy
    dma_peek_list.sub_cmd = 0x00;
    dma_peek_list.count = 1;
    dma_peek_list.dest_addr = (unsigned int)&dma_byte;
    dma_peek_list.dest_bank = 0;

    dma_poke_list.option_0b = 0x0b;
    dma_poke_list.option_80 = 0x80;
    dma_poke_list.source_mb = 0x00; // dma_byte lives in 1st MB
    dma_poke_list.option_81 = 0x81;
    dma_poke_list.end_of_options = 0x00;
    dma_poke_list.command = 0x00; // copy
    dma_poke_list.sub_cmd = 0x00;
    dma_poke_list.count = 1;
    dma_poke_list.source_addr = (unsigned int)&dma_byte;
    dma_poke_list.source_bank = 0;

    dma_copy_list.option_0b = 0x0b;
    dma_copy_list.option_80 = 0x80;
    dma_copy_list.option_81 = 0x81;
    dma_copy_list.end_of_options = 0x00;
    dma_copy_list.sub_cmd = 0x00;
    dma_copy_list.command = 0x00; // copy
    dma_copy_list.sub_cmd = 0;

    dma_copy_skip_list.option_0b = 0x0b;
    dma_copy_skip_list.option_80 = 0x80;
    dma_copy_skip_list.option_81 = 0x81;
    dma_copy_skip_list.option_82 = 0x82;
    dma_copy_skip_list.option_83 = 0x83;
    dma_copy_skip_list.option_84 = 0x84;
    dma_copy_skip_list.option_85 = 0x85;
    dma_copy_skip_list.end_of_options = 0x00;
    dma_copy_skip_list.sub_cmd = 0x00;
    dma_copy_skip_list.command = 0x00; // copy
    dma_copy_skip_list.sub_cmd = 0;

    dma_fill_list.option_0b = 0x0b;
    dma_fill_list.option_80 = 0x80;
    dma_fill_list.source_mb = 0x00;
    dma_fill_list.option_81 = 0x81;
    dma_fill_list.end_of_options = 0x00;
    dma_fill_list.command = 0x03; // fill
    dma_fill_list.sub_cmd = 0;

    dma_fill_skip_list.option_0b = 0x0b;
    dma_fill_skip_list.option_80 = 0x80;
    dma_fill_skip_list.source_mb = 0x00;
    dma_fill_skip_list.option_81 = 0x81;
    dma_fill_skip_list.option_82 = 0x82;
    dma_fill_skip_list.option_83 = 0x83;
    dma_fill_skip_list.option_84 = 0x84;
    dma_fill_skip_list.option_85 = 0x85;
    dma_fill_skip_list.end_of_options = 0x00;
    dma_fill_skip_list.command = 0x03; // fill_skip
    dma_fill_skip_list.sub_cmd = 0;
}

void do_dma(unsigned int dmalist)
{
    mega65_io_enable();
    POKE(0xd702U, 0);
    POKE(0xd704U, 0x00); // List is in $00xxxxx
    POKE(0xd701U, dmalist >> 8);
    POKE(0xd705U, dmalist & 0xff); // triggers enhanced DMA
}

unsigned char dma_peek(dword address)
{
    dma_peek_list.source_mb = (address >> 20);
    dma_peek_list.source_addr = address & 0xffff;
    dma_peek_list.source_bank = (address >> 16) & 0x0f;
    do_dma((unsigned int) &dma_peek_list);
    return dma_byte;
}

void dma_poke(dword address, byte value)
{
    dma_byte = value;
    dma_poke_list.dest_mb = (address >> 20);
    dma_poke_list.dest_addr = address & 0xffff;
    dma_poke_list.dest_bank = (address >> 16) & 0x0f;
    do_dma((unsigned int) &dma_poke_list);
}

void dma_copy(dword source_address, dword destination_address, word count)
{
    dma_copy_list.source_mb = source_address >> 20;
    dma_copy_list.dest_mb = (destination_address >> 20);
    dma_copy_list.count = count;
    dma_copy_list.source_addr = source_address & 0xffff;
    dma_copy_list.source_bank = (source_address >> 16) & 0x0f;
    dma_copy_list.dest_addr = destination_address & 0xffff;
    dma_copy_list.dest_bank = (destination_address >> 16) & 0x0f;
    do_dma((unsigned int) &dma_copy_list);
}

void dma_copy_skip(dword source_address, dword destination_address, 
    word count, word source_skip, word dest_skip)
{
    dma_copy_skip_list.source_skip_bit = source_skip & 0xff;
    dma_copy_skip_list.source_skip_byte = source_skip >> 8;
    dma_copy_skip_list.dest_skip_bit = dest_skip & 0xff;
    dma_copy_skip_list.dest_skip_byte = dest_skip >> 8;
    dma_copy_skip_list.source_mb = source_address >> 20;
    dma_copy_skip_list.dest_mb = (destination_address >> 20);
    dma_copy_skip_list.count = count;
    dma_copy_skip_list.source_addr = source_address & 0xffff;
    dma_copy_skip_list.source_bank = (source_address >> 16) & 0x0f;
    dma_copy_skip_list.dest_addr = destination_address & 0xffff;
    dma_copy_skip_list.dest_bank = (destination_address >> 16) & 0x0f;
    do_dma((unsigned int) &dma_copy_skip_list);
}

void dma_fill(dword destination_address, byte value, word count)
{
    dma_fill_list.dest_mb = destination_address >> 20;
    dma_fill_list.count = count;
    dma_fill_list.source_addr = value;
    dma_fill_list.dest_addr = destination_address & 0xffff;
    dma_fill_list.dest_bank = (destination_address >> 16) & 0x0f;
    do_dma((unsigned int) &dma_fill_list);
}

void dma_fill_skip(dword destination_address, byte value, word count,
    word source_skip, word dest_skip)
{
    dma_fill_skip_list.source_skip_bit = source_skip & 0xff;
    dma_fill_skip_list.source_skip_byte = source_skip >> 8;
    dma_fill_skip_list.dest_skip_bit = dest_skip & 0xff;
    dma_fill_skip_list.dest_skip_byte = dest_skip >> 8;
    dma_fill_skip_list.dest_mb = destination_address >> 20;
    dma_fill_skip_list.count = count;
    dma_fill_skip_list.source_addr = value;
    dma_fill_skip_list.dest_addr = destination_address & 0xffff;
    dma_fill_skip_list.dest_bank = (destination_address >> 16) & 0x0f;
    do_dma((unsigned int) &dma_fill_skip_list);
}


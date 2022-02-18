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
} dma_peeklist;

struct {
    EXTENDED_COMMANDS
    F018B_COMMANDS
} dma_pokelist;

struct {
    EXTENDED_COMMANDS
    F018B_COMMANDS
} dma_copylist;

struct {
    EXTENDED_COMMANDS
    unsigned char option_85;
    unsigned char dest_skip;
    F018B_COMMANDS
} dma_filllist;

void dma_init() {
    dma_peeklist.option_0b = 0x0b;
    dma_peeklist.option_80 = 0x80;
    dma_peeklist.option_81 = 0x81;
    dma_peeklist.dest_mb = 0x00; // dma_byte lives in 1st MB
    dma_peeklist.end_of_options = 0x00;
    dma_peeklist.command = 0x00; // copy
    dma_peeklist.sub_cmd = 0x00;
    dma_peeklist.count = 1;
    dma_peeklist.dest_addr = (unsigned int)&dma_byte;
    dma_peeklist.dest_bank = 0;

    dma_pokelist.option_0b = 0x0b;
    dma_pokelist.option_80 = 0x80;
    dma_pokelist.source_mb = 0x00; // dma_byte lives in 1st MB
    dma_pokelist.option_81 = 0x81;
    dma_pokelist.end_of_options = 0x00;
    dma_pokelist.command = 0x00; // copy
    dma_pokelist.sub_cmd = 0x00;
    dma_pokelist.count = 1;
    dma_pokelist.source_addr = (unsigned int)&dma_byte;
    dma_pokelist.source_bank = 0;

    dma_copylist.option_0b = 0x0b;
    dma_copylist.option_80 = 0x80;
    dma_copylist.option_81 = 0x81;
    dma_copylist.end_of_options = 0x00;
    dma_copylist.sub_cmd = 0x00;
    dma_copylist.command = 0x00; // copy
    dma_copylist.sub_cmd = 0;

    dma_filllist.option_0b = 0x0b;
    dma_filllist.option_80 = 0x80;
    dma_filllist.source_mb = 0x00;
    dma_filllist.option_81 = 0x81;
    dma_filllist.option_85 = 0x85;
    dma_filllist.end_of_options = 0x00;
    dma_filllist.command = 0x03; // fill
    dma_filllist.sub_cmd = 0;
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
    dma_peeklist.source_mb = (address >> 20);
    dma_peeklist.source_addr = address & 0xffff;
    dma_peeklist.source_bank = (address >> 16) & 0x0f;
    do_dma((unsigned int) &dma_peeklist);
    return dma_byte;
}

void dma_poke(dword address, byte value)
{
    dma_byte = value;
    dma_pokelist.dest_mb = (address >> 20);
    dma_pokelist.dest_addr = address & 0xffff;
    dma_pokelist.dest_bank = (address >> 16) & 0x0f;
    do_dma((unsigned int) &dma_pokelist);
}

void dma_copy(dword source_address, dword destination_address, word count)
{
    dma_copylist.source_mb = source_address >> 20;
    dma_copylist.dest_mb = (destination_address >> 20);
    dma_copylist.count = count;
    dma_copylist.source_addr = source_address & 0xffff;
    dma_copylist.source_bank = (source_address >> 16) & 0x0f;
    dma_copylist.dest_addr = destination_address & 0xffff;
    dma_copylist.dest_bank = (destination_address >> 16) & 0x0f;
    do_dma((unsigned int) &dma_copylist);
}

void dma_fill(dword destination_address, byte value, word count)
{
    dma_filllist.dest_mb = destination_address >> 20;
    dma_filllist.count = count;
    dma_filllist.source_addr = value;
    dma_filllist.dest_skip = 1;
    dma_filllist.dest_addr = destination_address & 0xffff;
    dma_filllist.dest_bank = (destination_address >> 16) & 0x0f;
    do_dma((unsigned int) &dma_filllist);
    return;
}

void dma_fill_skip(dword destination_address, byte value, word count, byte skip)
{
    dma_filllist.dest_mb = destination_address >> 20;
    dma_filllist.count = count;
    dma_filllist.source_addr = value;
    dma_filllist.dest_skip = skip;
    dma_filllist.dest_addr = destination_address & 0xffff;
    dma_filllist.dest_bank = (destination_address >> 16) & 0x0f;
    do_dma((unsigned int) &dma_filllist);
    return;
}


#include "mega65.h"

// **One pre-initialised DMA list per operation, not one shared list.**
// mega65-libc's memory.c rebuilds all fourteen fields of a single list on
// every call; the cc65 version this replaces (old-cc65/src/dma.c) already had
// the better arrangement, where the fields that never change for a given
// operation are static initialisers, loaded with the program, and a call
// stores only the four or five that do. It matters here because drawing the map view is thousands of
// these in a row.
//
// The lists must live in the first 64 KB -- do_dma() below tells the DMAgic
// the list is in $00xxxxx -- which they are, being ordinary globals.

#define EXTENDED_OPTIONS \
    byte option_0b;      \
    byte option_80;      \
    byte source_mb;      \
    byte option_81;      \
    byte dest_mb;

// The F018B job proper. Field order is the hardware's, not ours.
#define F018B_JOB    \
    byte end_of_options; \
    byte command;    \
    word count;      \
    word source_addr;\
    byte source_bank;\
    word dest_addr;  \
    byte dest_bank;  \
    byte sub_cmd;    \
    word modulo;

struct dma_plain {
    EXTENDED_OPTIONS
    F018B_JOB
};

struct dma_skip {
    EXTENDED_OPTIONS
    byte option_85;
    byte dest_skip;
    F018B_JOB
};

struct dma_transparent {
    EXTENDED_OPTIONS
    byte option_07;
    byte option_86;
    byte transparent_value;
    F018B_JOB
};

// The fields every job of a kind shares, as initialisers: they come in with
// the PRG and cost nothing at run time. They used to be set by a dma_init()
// on the first call, 249 bytes of code to store 134 constants. The enhanced
// options: $0B F018B format, $80 and $81 the source and destination
// megabytes, $85 the destination skip, $07 and $86 transparency.
#define DMA_OPTIONS .option_0b = 0x0b, .option_80 = 0x80, .option_81 = 0x81

static struct dma_plain peek_list = { DMA_OPTIONS, .count = 1 };
static struct dma_plain poke_list = { DMA_OPTIONS, .count = 1 };
static struct dma_plain copy_list = { DMA_OPTIONS };
static struct dma_plain fill_list = { DMA_OPTIONS, .command = 0x03 };
static struct dma_skip fill_skip_list = {
    DMA_OPTIONS, .option_85 = 0x85, .command = 0x03
};
static struct dma_transparent copy_transparent_list = {
    DMA_OPTIONS, .option_07 = 0x07, .option_86 = 0x86
};

// The single byte lpeek reads into and lpoke writes out of. It has to be
// addressable in the first 64 KB, which a global is.
static byte dma_byte;

void mega65_io_enable(void)
{
    // Gate C65 I/O enable: the knock that makes the VIC-IV registers reachable
    POKE(0xd02fU, 0x47);
    POKE(0xd02fU, 0x53);
    // Force to full speed (40 MHz)
    POKE(0, 65);
}

static void do_dma(word list)
{
    mega65_io_enable();
    POKE(0xd702U, 0);
    POKE(0xd704U, 0x00);      // the list itself is in $00xxxxx
    POKE(0xd701U, list >> 8);
    POKE(0xd705U, list & 0xff); // writing $D705 triggers an enhanced DMA
}

// The fast path. Same list shape as copy_transparent_list, kept separate so a
// run of blits cannot be disturbed by an lpeek or an lfill in between.
static struct dma_transparent blit_list = {
    DMA_OPTIONS, .option_07 = 0x07, .option_86 = 0x86
};

void blit_begin(byte source_mb, byte source_bank, byte dest_mb,
                byte transparent)
{
    blit_list.source_mb = source_mb;
    blit_list.source_bank = source_bank;
    blit_list.dest_mb = dest_mb;
    blit_list.transparent_value = transparent;

    // $D702 and $D704 are part of the trigger and never change either, so
    // they are set here rather than on every job. That is also why blit()
    // does not call mega65_io_enable(): measured at 1.3 us a job, and the
    // registers stay unlocked once something else has done the knock.
    mega65_io_enable();
    POKE(0xd702U, 0);
    POKE(0xd704U, 0x00);
}

void blit(word source_addr, byte dest_bank, word dest_addr, word count)
{
    blit_list.source_addr = source_addr;
    blit_list.dest_bank = dest_bank;
    blit_list.dest_addr = dest_addr;
    blit_list.count = count;
    POKE(0xd701U, (word)&blit_list >> 8);
    POKE(0xd705U, (word)&blit_list & 0xff);
}

byte lpeek(dword address)
{
    peek_list.dest_addr = (word)&dma_byte;
    peek_list.source_mb = address >> 20;
    peek_list.source_addr = address & 0xffff;
    peek_list.source_bank = (address >> 16) & 0x0f;
    do_dma((word)&peek_list);
    return dma_byte;
}

void lpoke(dword address, byte value)
{
    dma_byte = value;
    poke_list.source_addr = (word)&dma_byte;
    poke_list.dest_mb = address >> 20;
    poke_list.dest_addr = address & 0xffff;
    poke_list.dest_bank = (address >> 16) & 0x0f;
    do_dma((word)&poke_list);
}

void lcopy(dword source, dword destination, word count)
{
    copy_list.source_mb = source >> 20;
    copy_list.dest_mb = destination >> 20;
    copy_list.count = count;
    copy_list.source_addr = source & 0xffff;
    copy_list.source_bank = (source >> 16) & 0x0f;
    copy_list.dest_addr = destination & 0xffff;
    copy_list.dest_bank = (destination >> 16) & 0x0f;
    do_dma((word)&copy_list);
}

void lcopy_transparent(dword source, dword destination, word count,
                       byte transparent)
{
    copy_transparent_list.transparent_value = transparent;
    copy_transparent_list.source_mb = source >> 20;
    copy_transparent_list.dest_mb = destination >> 20;
    copy_transparent_list.count = count;
    copy_transparent_list.source_addr = source & 0xffff;
    copy_transparent_list.source_bank = (source >> 16) & 0x0f;
    copy_transparent_list.dest_addr = destination & 0xffff;
    copy_transparent_list.dest_bank = (destination >> 16) & 0x0f;
    do_dma((word)&copy_transparent_list);
}

void lfill(dword destination, byte value, word count)
{
    fill_list.dest_mb = destination >> 20;
    fill_list.count = count;
    // In a fill job the source address field holds the byte to write.
    fill_list.source_addr = value;
    fill_list.dest_addr = destination & 0xffff;
    fill_list.dest_bank = (destination >> 16) & 0x0f;
    do_dma((word)&fill_list);
}

void lfill_skip(dword destination, byte value, word count, byte skip)
{
    fill_skip_list.dest_skip = skip;
    fill_skip_list.dest_mb = destination >> 20;
    fill_skip_list.count = count;
    fill_skip_list.source_addr = value;
    fill_skip_list.dest_addr = destination & 0xffff;
    fill_skip_list.dest_bank = (destination >> 16) & 0x0f;
    do_dma((word)&fill_skip_list);
}

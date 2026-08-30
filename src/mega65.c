#include "mega65.h"

// **One pre-initialised DMA list per operation, not one shared list.**
// mega65-libc's memory.c rebuilds all fourteen fields of a single list on
// every call; the cc65 version this replaces (old-cc65/src/dma.c) already had
// the better arrangement, where the fields that never change for a given
// operation are written once in dma_init() and a call stores only the four or
// five that do. It matters here because drawing the map view is thousands of
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

static struct dma_plain peek_list;
static struct dma_plain poke_list;
static struct dma_plain copy_list;
static struct dma_plain fill_list;
static struct dma_skip fill_skip_list;
static struct dma_transparent copy_transparent_list;

// The single byte lpeek reads into and lpoke writes out of. It has to be
// addressable in the first 64 KB, which a global is.
static byte dma_byte;

// Set to 1 by the first call into any of these, so a caller cannot forget.
// mega65-libc has an explicit dma_init(); one fewer ordering rule is better.
static byte initialised = 0;

void mega65_io_enable(void)
{
    // Gate C65 I/O enable: the knock that makes the VIC-IV registers reachable
    POKE(0xd02fU, 0x47);
    POKE(0xd02fU, 0x53);
    // Force to full speed (40 MHz)
    POKE(0, 65);
}

static void dma_init(void)
{
    initialised = 1;

    peek_list.option_0b = 0x0b;
    peek_list.option_80 = 0x80;
    peek_list.option_81 = 0x81;
    peek_list.dest_mb = 0x00; // dma_byte lives in the 1st MB
    peek_list.end_of_options = 0x00;
    peek_list.command = 0x00; // copy
    peek_list.sub_cmd = 0x00;
    peek_list.count = 1;
    peek_list.dest_addr = (word)&dma_byte;
    peek_list.dest_bank = 0;

    poke_list.option_0b = 0x0b;
    poke_list.option_80 = 0x80;
    poke_list.source_mb = 0x00;
    poke_list.option_81 = 0x81;
    poke_list.end_of_options = 0x00;
    poke_list.command = 0x00; // copy
    poke_list.sub_cmd = 0x00;
    poke_list.count = 1;
    poke_list.source_addr = (word)&dma_byte;
    poke_list.source_bank = 0;

    copy_list.option_0b = 0x0b;
    copy_list.option_80 = 0x80;
    copy_list.option_81 = 0x81;
    copy_list.end_of_options = 0x00;
    copy_list.command = 0x00; // copy
    copy_list.sub_cmd = 0x00;

    fill_list.option_0b = 0x0b;
    fill_list.option_80 = 0x80;
    fill_list.source_mb = 0x00;
    fill_list.option_81 = 0x81;
    fill_list.end_of_options = 0x00;
    fill_list.command = 0x03; // fill
    fill_list.sub_cmd = 0x00;

    fill_skip_list.option_0b = 0x0b;
    fill_skip_list.option_80 = 0x80;
    fill_skip_list.source_mb = 0x00;
    fill_skip_list.option_81 = 0x81;
    fill_skip_list.option_85 = 0x85;
    fill_skip_list.end_of_options = 0x00;
    fill_skip_list.command = 0x03; // fill
    fill_skip_list.sub_cmd = 0x00;

    copy_transparent_list.option_0b = 0x0b;
    copy_transparent_list.option_07 = 0x07; // enable transparency
    copy_transparent_list.option_80 = 0x80;
    copy_transparent_list.option_81 = 0x81;
    copy_transparent_list.option_86 = 0x86; // the value to treat as transparent
    copy_transparent_list.end_of_options = 0x00;
    copy_transparent_list.command = 0x00; // copy
    copy_transparent_list.sub_cmd = 0x00;
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
static struct dma_transparent blit_list;

void blit_begin(byte source_mb, byte source_bank, byte dest_mb,
                byte transparent)
{
    if (!initialised) dma_init();

    blit_list.option_0b = 0x0b;
    blit_list.option_07 = 0x07;
    blit_list.option_80 = 0x80;
    blit_list.option_81 = 0x81;
    blit_list.option_86 = 0x86;
    blit_list.end_of_options = 0x00;
    blit_list.command = 0x00;   // copy
    blit_list.sub_cmd = 0x00;

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
    if (!initialised) dma_init();
    peek_list.source_mb = address >> 20;
    peek_list.source_addr = address & 0xffff;
    peek_list.source_bank = (address >> 16) & 0x0f;
    do_dma((word)&peek_list);
    return dma_byte;
}

void lpoke(dword address, byte value)
{
    if (!initialised) dma_init();
    dma_byte = value;
    poke_list.dest_mb = address >> 20;
    poke_list.dest_addr = address & 0xffff;
    poke_list.dest_bank = (address >> 16) & 0x0f;
    do_dma((word)&poke_list);
}

void lcopy(dword source, dword destination, word count)
{
    if (!initialised) dma_init();
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
    if (!initialised) dma_init();
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
    if (!initialised) dma_init();
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
    if (!initialised) dma_init();
    fill_skip_list.dest_skip = skip;
    fill_skip_list.dest_mb = destination >> 20;
    fill_skip_list.count = count;
    fill_skip_list.source_addr = value;
    fill_skip_list.dest_addr = destination & 0xffff;
    fill_skip_list.dest_bank = (destination >> 16) & 0x0f;
    do_dma((word)&fill_skip_list);
}

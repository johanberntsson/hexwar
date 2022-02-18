#ifndef __HEXWAR_H
#define __HEXWAR_H

#include <stdbool.h>

typedef unsigned int word;
typedef unsigned char byte;
typedef unsigned long dword;

#define SCREENBASE 0xe000u
#define SCREENBASE_UNDER_RAM
#define SCREENBASE_FIRST_64K

#define TMPBUFFER 0x03fbu
#define TMPBUFFER_SIZE 200

// temp variables (much faster to keep them global)
extern word i, j;
extern byte x, y;

void go_8bit();
void fatal(const char *format, ...);
void start_benchmark();
void stop_benchmark();
void main(void) ;

#endif

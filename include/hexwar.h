#ifndef __HEXWAR_H
#define __HEXWAR_H

#include <stdbool.h>

typedef unsigned int word;
typedef unsigned char byte;
typedef unsigned long dword;

#define SCREENBASE 0xe000
#define TMPBUFFER 0x3fb
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

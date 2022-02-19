#ifndef __HEXWAR_H
#define __HEXWAR_H

#include <stdbool.h>

// use these types as much as possible
typedef unsigned int word;
typedef unsigned char byte;
typedef unsigned long dword;

// temp variables (much faster to keep them global)
extern word i, j;
extern byte x, y;

// Screen location (with shortcuts if in bank 0)
#define SCREENBASE 0xe000u
#define SCREENBASE_UNDER_RAM
#define SCREENBASE_FIRST_64K

// Screen resolution: defaults to H320, V200 if not defined
#define H640
#define V400

// work buffer
#define TMPBUFFER 0x03fbu
#define TMPBUFFER_SIZE 200

void go_8bit();
void fatal(const char *format, ...);
void start_benchmark();
void stop_benchmark();
void main(void) ;

#endif

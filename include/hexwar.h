#ifndef __HEXWAR_H
#define __HEXWAR_H

#include <stdbool.h>

// use these types as much as possible
typedef unsigned int word;
typedef unsigned char byte;
typedef unsigned long dword;

// temp variables (much faster to keep them global)
extern word i, j;
extern byte x, y, u, v;

// Screen location (with shortcuts if in bank 0)
#define SCREENBASE 0xe000u
#define SCREENBASE_UNDER_RAM
#define SCREENBASE_FIRST_64K

// Screen resolution: defaults to H320, V200 if not defined
#define H640
#define V400

// map view limits
#define VIEW_LEFT 10
#define VIEW_RIGHT 21
#define VIEW_TOP 10
#define VIEW_BOTTOM 13

// work buffer
#define TMPBUFFER 0x03fbu
#define TMPBUFFER_SIZE 200

void go_8bit();
void fatal(const char *format, ...);
void start_benchmark();
void stop_benchmark();
void main(void) ;

#endif

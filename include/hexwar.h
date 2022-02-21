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
#define SCREENBASE 0xe000U
#define SCREENBASE_UNDER_RAM
#define SCREENBASE_FIRST_64K
// default color base ff80000 has DOS interference
#define COLORBASE 0xff81000L

// Screen resolution: defaults to H320, V200 if not defined
#define H640
//#define V400

// map view limits
#define VIEW_LEFT 19
#define VIEW_RIGHT 78
#define VIEW_TOP 1
#define VIEW_BOTTOM 23
#define VIEW_WIDTH (1 + VIEW_RIGHT - VIEW_LEFT)
#define VIEW_HEIGHT (1 + VIEW_BOTTOM - VIEW_TOP)
#define VIEW_CENTER_X (VIEW_LEFT + (VIEW_WIDTH >> 1))
#define VIEW_CENTER_Y (VIEW_TOP + (VIEW_HEIGHT >> 1))

#if (64L * VIEW_WIDTH * VIEW_HEIGHT) >= 0x30000) 
#error view is too big (> $30000)
#endif

// work buffer
#define TMPBUFFER 0x0334U
#define TMPBUFFER_SIZE 200

void go_8bit();
void fatal(const char *format, ...);
void start_benchmark();
void stop_benchmark();
void main(void) ;

#endif

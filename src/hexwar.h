// Things main() needs that have nowhere better to live.
#ifndef HEXWAR_HEXWAR_H
#define HEXWAR_HEXWAR_H

#include <stdio.h>

// Where the view starts, in map characters. Overridable from the Makefile
// (`make START_OFF_X=41`) because that is the only way a headless run can
// photograph the view at a different offset -- see main().
#ifndef START_OFF_X
#define START_OFF_X 40
#endif
#ifndef START_OFF_Y
#define START_OFF_Y 30
#endif

#endif // HEXWAR_HEXWAR_H

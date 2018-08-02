#ifndef LIBATARI800_INPUT_H_
#define LIBATARI800_INPUT_H_

#include "atari.h"

#define LIBATARI800_FLAG_DELTA_MOUSE 0
#define LIBATARI800_FLAG_DIRECT_MOUSE 1

typedef struct {
    UBYTE keychar;
    UBYTE keycode;
    UBYTE special;
    UBYTE shift;
    UBYTE control;
    UBYTE start;
    UBYTE select;
    UBYTE option;
    UBYTE joy0;
    UBYTE trig0;
    UBYTE joy1;
    UBYTE trig1;
    UBYTE joy2;
    UBYTE trig2;
    UBYTE joy3;
    UBYTE trig3;
    UBYTE mousex;
    UBYTE mousey;
    UBYTE mouse_buttons;
    UBYTE mouse_mode;
} input_template_t;

extern input_template_t *LIBATARI800_Input_array;

int LIBATARI800_Input_Initialise(int *argc, char *argv[]);

void LIBATARI800_Mouse(void);

#endif /* LIBATARI800_INPUT_H_ */

#ifndef LIBATARI800_H_
#define LIBATARI800_H_

#include "atari.h"

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

void libatari800_clear_input_array(input_template_t *input);

int libatari800_next_frame(input_template_t *input);

#endif /* LIBATARI800_H_ */

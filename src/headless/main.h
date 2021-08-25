#ifndef HEADLESS_MAIN_H_
#define HEADLESS_MAIN_H_

#include <stdio.h>

#include "config.h"

extern int HEADLESS_debug_screen;
extern int HEADLESS_keydown_time;
extern int HEADLESS_keyup_time;

void HEADLESS_Frame(void);

#endif /* HEADLESS_MAIN_H_ */

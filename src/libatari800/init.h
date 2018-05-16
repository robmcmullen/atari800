#ifndef LIBATARI800_INIT_H_
#define LIBATARI800_INIT_H_

#include "atari.h"
#include "screen.h"
#ifdef SOUND
#include "sound.h"
#endif
#include "../statesav.h"

#define Screen_USABLE_WIDTH 336

#define LIBATARI800_VIDEO_SIZE (Screen_USABLE_WIDTH * Screen_HEIGHT)
#define LIBATARI800_SOUND_SIZE 2048

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

typedef struct {
	ULONG frame_number;
	UBYTE frame_finished;
	UBYTE breakpoint_hit;
	UBYTE unused1;
	UBYTE unused2;
	UBYTE video[LIBATARI800_VIDEO_SIZE];
	UBYTE audio[LIBATARI800_SOUND_SIZE];
	UBYTE state[STATESAV_MAX_SIZE];
} output_template_t;

extern input_template_t *LIBATARI800_Input_array;
extern UBYTE *LIBATARI800_Video_array;
extern UBYTE *LIBATARI800_Sound_array;
extern UBYTE *LIBATARI800_Save_state;

extern ULONG frame_number;

int LIBATARI800_Initialise(void);
void LIBATARI800_Exit(void);

#endif /* LIBATARI800_INIT_H_ */

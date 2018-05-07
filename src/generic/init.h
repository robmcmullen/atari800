#ifndef GENERIC_INIT_H_
#define GENERIC_INIT_H_

#include "atari.h"
#include "screen.h"
#ifdef SOUND
#include "sound.h"
#endif
#include "../statesav.h"

#define Screen_USABLE_WIDTH 336

#define GENERIC_INPUT_OFFSET 0
#define GENERIC_INPUT_SIZE 2048
#define GENERIC_VIDEO_OFFSET (GENERIC_INPUT_SIZE)
#define GENERIC_VIDEO_SIZE (Screen_USABLE_WIDTH * Screen_HEIGHT)
#define GENERIC_SOUND_OFFSET (GENERIC_VIDEO_OFFSET + GENERIC_VIDEO_SIZE)
#define GENERIC_SOUND_SIZE 2048
#define GENERIC_STATE_OFFSET (GENERIC_SOUND_OFFSET + GENERIC_SOUND_SIZE)
#define GENERIC_TOTAL_SIZE (GENERIC_INPUT_SIZE + GENERIC_SOUND_SIZE + GENERIC_VIDEO_SIZE + STATESAV_MAX_SIZE)

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
	UBYTE video[GENERIC_VIDEO_SIZE];
	UBYTE audio[GENERIC_SOUND_SIZE];
	UBYTE state[STATESAV_MAX_SIZE];
} output_template_t;

extern input_template_t *GENERIC_Input_array;
extern UBYTE *GENERIC_Video_array;
extern UBYTE *GENERIC_Sound_array;
extern UBYTE *GENERIC_Save_state;

extern ULONG frame_number;

int GENERIC_Initialise(void);
void GENERIC_Exit(void);

#endif /* GENERIC_INIT_H_ */

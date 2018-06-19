#ifndef LIBATARI800_STATESAV_H_
#define LIBATARI800_STATESAV_H_

#include <stdio.h>

#include "config.h"
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

/* byte offsets into output_template.state array of groups of data
   to prevent the need for a full parsing of the save state data to
   be able to find parts needed for the visualizer display
 */
typedef struct {
    ULONG cpu;
    ULONG pc;
    ULONG base_ram;
    ULONG base_ram_attrib;
    ULONG antic;
    ULONG gtia;
    ULONG pia;
    ULONG pokey;
} statesav_tags_t;

typedef struct {
    ULONG frame_number;
    UBYTE frame_finished;
    UBYTE breakpoint_hit;
    UBYTE unused1;
    UBYTE unused2;
    UBYTE video[LIBATARI800_VIDEO_SIZE];
    UBYTE audio[LIBATARI800_SOUND_SIZE];
    statesav_tags_t tags;
    UBYTE state[STATESAV_MAX_SIZE];
} output_template_t;

extern input_template_t *LIBATARI800_Input_array;
extern output_template_t *LIBATARI800_Output_array;
extern UBYTE *LIBATARI800_Video_array;
extern UBYTE *LIBATARI800_Sound_array;
extern UBYTE *LIBATARI800_Save_state;

extern ULONG frame_number;

void LIBATARI800_StateSave();
void LIBATARI800_StateLoad();

#endif /* LIBATARI800_STATESAV_H_ */

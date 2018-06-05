/*
 * libatari800/main.c - Atari800 as a libraryrestore - main interface
 *
 * Copyright (c) 2001-2002 Jacek Poplawski
 * Copyright (C) 2001-2014 Atari800 development team (see DOC/CREDITS)
 * Copyright (c) 2016-2017 Rob McMullen
 *
 * This file is part of the Atari800 emulator project which emulates
 * the Atari 400, 800, 800XL, 130XE, and 5200 8-bit computers.
 *
 * Atari800 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Atari800 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Atari800; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "config.h"
#include <stdio.h>
#include <string.h>

/* Atari800 includes */
#include "atari.h"
#include "akey.h"
#include "binload.h"
#include "../input.h"
#include "log.h"
#include "monitor.h"
#include "cpu.h"
#include "platform.h"
#ifdef SOUND
#include "../sound.h"
#endif
#include "util.h"
#include "videomode.h"
#include "sio.h"
#include "libatari800/init.h"
#include "libatari800/input.h"
#include "libatari800/video.h"
#include "libatari800/statesav.h"

/* mainloop includes */
#include "antic.h"
#include "devices.h"
#include "gtia.h"
#include "pokey.h"
#ifdef PBI_BB
#include "pbi_bb.h"
#endif
#if defined(PBI_XLD) || defined (VOICEBOX)
#include "votraxsnd.h"
#endif

typedef void (*monitor_callback_ptr)(void);

monitor_callback_ptr monitor_callback;

extern int debug_sound;

extern ULONG frame_number;

int PLATFORM_Configure(char *option, char *parameters)
{
	return TRUE;
}

void PLATFORM_ConfigSave(FILE *fp)
{
	;
}

int PLATFORM_Initialise(int *argc, char *argv[])
{
	int i, j;
	int help_only = FALSE;

	debug_sound = FALSE;
	for (i = j = 1; i < *argc; i++) {
		if (strcmp(argv[i], "-help") == 0) {
			help_only = TRUE;
		}
		if (strcmp(argv[i], "-libatari800-debug-sound") == 0) {
			debug_sound = TRUE;
		}
		argv[j++] = argv[i];
	}
	*argc = j;

	if (!help_only) {
		if (!LIBATARI800_Initialise()) {
			return FALSE;
		}
	}

	if (!LIBATARI800_Video_Initialise(argc, argv)
		|| !Sound_Initialise(argc, argv)
		|| !LIBATARI800_Input_Initialise(argc, argv))
		return FALSE;

	return TRUE;
}

int PLATFORM_Exit(int run_monitor)
{
	Log_flushlog();

	LIBATARI800_Output_array->breakpoint_hit = TRUE;
	(* monitor_callback)();

	return 1;  /* always continue. Leave it to the client to exit */
}


void LIBATARI800_Frame(void)
{
	switch (INPUT_key_code) {
	case AKEY_COLDSTART:
		Atari800_Coldstart();
		break;
	case AKEY_WARMSTART:
		Atari800_Warmstart();
		break;
	case AKEY_UI:
		PLATFORM_Exit(TRUE);  /* run monitor */
		break;
	default:
		break;
	}

#ifdef PBI_BB
	PBI_BB_Frame(); /* just to make the menu key go up automatically */
#endif
#if defined(PBI_XLD) || defined (VOICEBOX)
	VOTRAXSND_Frame(); /* for the Votrax */
#endif
	Devices_Frame();
	INPUT_Frame();
	GTIA_Frame();
	ANTIC_Frame(TRUE);
	INPUT_DrawMousePointer();
	Screen_DrawAtariSpeed(Util_time());
	Screen_DrawDiskLED();
	Screen_Draw1200LED();
	POKEY_Frame();
#ifdef SOUND
	Sound_Update();
#endif
	Atari800_nframes++;
}


/* User-visible functions */

int a8_init(int argc, char **argv, monitor_callback_ptr *ptr)
{
	/* initialise Atari800 core */
	if (!Atari800_Initialise(&argc, argv))
		return 3;

	/* turn off frame sync, return frames as fast as possible and let whatever
	 calls process_frame to manage syncing to NTSC or PAL */
	Atari800_turbo = TRUE;

	monitor_callback = ptr;
}

void a8_clear_state_arrays(input_template_t *input, output_template_t *output)
{
	/* Initialize input and output arrays to zero */
	memset(input, 0, sizeof(input_template_t));
	memset(output, 0, sizeof(output_template_t));
	output->frame_number = 0;
	output->frame_finished = FALSE;
	output->breakpoint_hit = FALSE;
}

void a8_configure_state_arrays(input_template_t *input, output_template_t *output)
{
	/* Initialize input array and calculate size of output array based on the
	machine type*/
	LIBATARI800_Input_array = input;
	LIBATARI800_Output_array = output;
	LIBATARI800_Video_array = output->video;
	LIBATARI800_Sound_array = output->audio;
	LIBATARI800_Save_state = output->state;

	INPUT_key_code = AKEY_NONE;
	LIBATARI800_Mouse();
	LIBATARI800_Frame();
	LIBATARI800_StateSave();
	Atari800_Coldstart();  /* reset so a8_next_frame will start correctly */
}

void a8_next_frame(input_template_t *input, output_template_t *output)
{
	/* increment frame count before screen so error when generating
		this screen will reflect the frame number that caused it */
	frame_number++;
	LIBATARI800_Input_array = input;
	LIBATARI800_Output_array = output;
	output->frame_number = frame_number;
	output->frame_finished = FALSE;
	output->breakpoint_hit = FALSE;
	LIBATARI800_Video_array = output->video;
	LIBATARI800_Sound_array = output->audio;
	LIBATARI800_Save_state = output->state;

	INPUT_key_code = PLATFORM_Keyboard();
	LIBATARI800_Mouse();
	LIBATARI800_Frame();
	output->frame_finished = TRUE;
	LIBATARI800_StateSave();
	PLATFORM_DisplayScreen();
}

int a8_mount_disk_image(int diskno, const char *filename, int readonly)
{
	return SIO_Mount(diskno, filename, readonly);
}

int a8_reboot_with_file(const char *filename)
{
	BINLOAD_Loader(filename);
	Atari800_Coldstart();
}

void a8_get_current_state(output_template_t *output)
{
	output->frame_number = frame_number;
	LIBATARI800_Video_array = output->video;
	LIBATARI800_Sound_array = output->audio;
	LIBATARI800_Save_state = output->state;
	LIBATARI800_StateSave();
}

void a8_restore_state(output_template_t *restore)
{
	LIBATARI800_Save_state = restore->state;
	LIBATARI800_StateLoad();
	frame_number = restore->frame_number;
}

/* Monitor commands */

void a8_monitor_clear() {
	MONITOR_break_step = FALSE;
	MONITOR_break_ret = FALSE;
}

void a8_monitor_summary() {
	printf("a8_monitor: break_step=%d break_ret=%d break_addr=%04x\n", MONITOR_break_step, MONITOR_break_ret, MONITOR_break_addr);
}

int a8_monitor_step(int addr) {
	if(addr > 0) CPU_regPC = addr;
	MONITOR_break_step = TRUE;
	printf("a8_monitor_step\n");
	return TRUE; /* resume emulation */
}

int a8_breakpoint_set(int addr) {
	if(addr > 0) MONITOR_break_addr = addr;
	printf("a8_breakpoint_set: set %04x\n", addr);
	return TRUE; /* resume emulation */
}

int a8_breakpoint_clear() {
	MONITOR_break_addr = 0xd000;
	printf("a8_breakpoint_clear\n");
	return TRUE; /* resume emulation */
}

/*
vim:ts=4:sw=4:
*/

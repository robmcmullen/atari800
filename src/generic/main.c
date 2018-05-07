/*
 * generic/main.c - Generic interface port code - main interface
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
#include "../input.h"
#include "log.h"
#include "monitor.h"
#include "platform.h"
#ifdef SOUND
#include "../sound.h"
#endif
#include "util.h"
#include "videomode.h"
#include "sio.h"
#include "generic/init.h"
#include "generic/input.h"
#include "generic/video.h"
#include "generic/statesav.h"

extern int debug_sound;

extern unsigned int frame_count;

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
		if (strcmp(argv[i], "-generic-debug-sound") == 0) {
			debug_sound = TRUE;
		}
		argv[j++] = argv[i];
	}
	*argc = j;

	if (!help_only) {
		if (!GENERIC_Initialise()) {
			return FALSE;
		}
	}

	if (!GENERIC_Video_Initialise(argc, argv)
#ifdef SOUND
		|| !Sound_Initialise(argc, argv)
#endif
		|| !GENERIC_Input_Initialise(argc, argv))
		return FALSE;

	return TRUE;
}

int PLATFORM_Exit(int run_monitor)
{
	Log_flushlog();

	if (run_monitor) {
#ifdef SOUND
		Sound_Pause();
#endif
		if (MONITOR_Run()) {
	#ifdef SOUND
			Sound_Continue();
	#endif
			return 1;
		}
	}
	GENERIC_Exit();

	return 0;
}

int start_generic(int argc, char **argv)
{
	/* initialise Atari800 core */
	if (!Atari800_Initialise(&argc, argv))
		return 3;

	/* turn off frame sync, return frames as fast as possible and let whatever
	 calls process_frame to manage syncing to NTSC or PAL */
	Atari800_turbo = TRUE;
}

void process_frame(input_template_t *input, output_template_t *output)
{
	/* increment frame count before screen so error when generating
		this screen will reflect the frame number that caused it */
	frame_count++;

	GENERIC_Input_array = input;

	output->frame_count = frame_count;
	GENERIC_Video_array = output->video;
	GENERIC_Sound_array = output->audio;
	GENERIC_Save_state = output->state;

	INPUT_key_code = PLATFORM_Keyboard();
	GENERIC_Mouse();
	Atari800_Frame();
	GENERIC_StateSave();
	PLATFORM_DisplayScreen();
}

int mount_disk_image(int diskno, const char *filename, int readonly)
{
	return SIO_Mount(diskno, filename, readonly);
}

void load_save_state(output_template_t *restore)
{
	GENERIC_Save_state = restore->state;
	GENERIC_StateLoad();
}


/*
vim:ts=4:sw=4:
*/

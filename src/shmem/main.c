/*
 * shmem/main.c - Shared memory port code - main interface
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
#include "shmem/init.h"
#include "shmem/input.h"
#include "shmem/video.h"
#include "shmem/statesav.h"

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
		if (strcmp(argv[i], "-shmem-debug-sound") == 0) {
			debug_sound = TRUE;
		}
		argv[j++] = argv[i];
	}
	*argc = j;

	if (!help_only) {
		if (!SHMEM_Initialise()) {
			return FALSE;
		}
	}

	if (!SHMEM_Video_Initialise(argc, argv)
#ifdef SOUND
		|| !Sound_Initialise(argc, argv)
#endif
		|| !SHMEM_Input_Initialise(argc, argv))
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
	SHMEM_Exit();

	return 0;
}

static void process_frame(input_template_t *input, callback_ptr cb)
{
	/* increment frame count before screen so error when generating
		this screen will reflect the frame number that caused it */
	frame_count++;
	input->frame_count = frame_count;
	SHMEM_TakeInputArraySnapshot();
	INPUT_key_code = PLATFORM_Keyboard();
	SHMEM_Mouse();
	Atari800_Frame();
	SHMEM_StateSave();
	if (Atari800_display_screen)
		PLATFORM_DisplayScreen();
	input->main_semaphore = 1; /* screen ready! */
	if (cb) {
		(*cb)(shared_memory);
	}
}

int start_shmem(int argc, char **argv, unsigned char *raw, int len, callback_ptr cb)
{
	input_template_t *input;

	if (raw) SHMEM_UseMemory(raw, len);

	/* initialise Atari800 core */
	if (!Atari800_Initialise(&argc, argv))
		return 3;

	input = SHMEM_GetInputArray();

	/* main loop */
	frame_count = 0;
	for (;;) {
		/* block until input ready */
		switch(input->main_semaphore) {
			case 0: /* generate a frame! */
#ifdef DEBUG
				printf("Found 0; getting frame");
#endif
				process_frame(input, cb);
				break;

			case 0xd0: /* load a disk image */
#ifdef DEBUG
				printf("Found 0xd0; loading D%d:%s!\n", input->arg_byte_1, input->arg_string);
#endif
				if (!SIO_Mount(input->arg_byte_1, input->arg_string, FALSE))
					input->arg_byte_1 = 0;  /* set error */
				input->main_semaphore = 1;
				break;

			case 0xe0: /* load a save state file */
#ifdef DEBUG
				printf("Found 0xe0; loading save state!\n");
#endif
				SHMEM_StateLoad();
				input->main_semaphore = 1;
				break;

			case 0xff: /* shutdown */
#ifdef DEBUG
				printf("Found 0xff; stopping!\n");
#endif
				goto end;

			default:
#ifdef DEBUG
				printf("invalid semaphore: %d\n", input->main_semaphore);
#endif
				break;
		}
		Util_sleep(0.0001);
	}
end:
	return 0;
}

void callback(unsigned char *mem) {
	input_template_t *input;

	input = SHMEM_GetInputArray();
	SHMEM_DebugVideo(mem);
	input->main_semaphore = 0; /* next frame! */
}

int main(int argc, char **argv)
{
	return start_shmem(argc, argv, NULL, 0, callback);
}

/*
vim:ts=4:sw=4:
*/

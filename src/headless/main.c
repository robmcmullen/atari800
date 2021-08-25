/*
 * headless/main.c - Scriptable atari800 - main interface
 *
 * Copyright (c) 2001-2002 Jacek Poplawski
 * Copyright (C) 2001-2014 Atari800 development team (see DOC/CREDITS)
 * Copyright (c) 2016-2021 Rob McMullen
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
#include <stdlib.h>
#include <string.h>

/* headless configuration forces the following defines to be TRUE:

   SOUND
   LIBATARI800
   AVI_VIDEO_RECORDING

*/

/* Atari800 includes */
#include "atari.h"
#include "akey.h"
#include "afile.h"
#include "../input.h"
#include "antic.h"
#include "cpu.h"
#include "platform.h"
#include "memory.h"
#include "screen.h"
#include "../sound.h"
#include "util.h"
#include "videomode.h"
#include "sio.h"
#include "cartridge.h"
#include "ui.h"
#include "cfg.h"
#include "log.h"
#include "multimedia.h"
#include "libatari800/main.h"
#include "libatari800/init.h"
#include "libatari800/input.h"
#include "libatari800/video.h"
#include "libatari800/sound.h"
#include "libatari800/statesav.h"
#include "headless/globals.h"

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

int HEADLESS_debug_screen = FALSE;

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
	int count;
	int help_only = FALSE;

	for (i = j = 1; i < *argc; i++) {
		int i_a = (i + 1 < *argc); /* is argument available? */
		int a_m = FALSE; /* error, argument missing! */
		int a_i = FALSE; /* error, argument invalid! */
		char *a_e = NULL; /* error message */

		if (strcmp(argv[i], "-o") == 0) {
			if (i_a) {
				if (!GLOBALS_SetOutputFile(argv[++i])) {
					a_i = TRUE;
					a_e = "Specify .wav to save sound file; .avi to save movie file";
				}
			}
			else a_m = TRUE;
		}
		else if (strcmp(argv[i], "-step") == 0) {
			if (i_a) {
				Util_sscansdec(argv[++i], &count);
				a_i = !GLOBALS_AddIntCommand(COMMAND_STEP, count);
			}
			else a_m = TRUE;
		}
		else if (strcmp(argv[i], "-rec") == 0) {
			if (i_a) {
				char *mode = argv[++i];
				if (strcmp(mode, "on") == 0)
					a_i = !GLOBALS_AddIntCommand(COMMAND_RECORD, 1);
				else if (strcmp(mode, "off") == 0)
					a_i = !GLOBALS_AddIntCommand(COMMAND_RECORD, 0);
				else {
					a_i = TRUE;
				}
			}
			else a_m = TRUE;
		}
		else if (strcmp(argv[i], "-type") == 0) {
			if (i_a) {
				a_i = !GLOBALS_AddStrCommand(COMMAND_TYPE, argv[++i]);
			}
			else a_m = TRUE;
		}
		else if (strcmp(argv[i], "-debug-screen") == 0) {
			HEADLESS_debug_screen = TRUE;
		}
		else {
			if (strcmp(argv[i], "-help") == 0) {
				help_only = TRUE;
				Log_print("\t-o <file>            Set output file (.wav or .avi)");
				Log_print("\t-rec on|off          Control if frames are recorded to media file");
				Log_print("\t-step <num>          Run emulator for <num> frames");
				Log_print("\t-type <keystrokes>   Type keystrokes into emulator");
			}
			argv[j++] = argv[i];
		}

		if (a_m) {
			Log_print("Missing argument for '%s'", argv[i]);
			return FALSE;
		} else if (a_i) {
			if (a_e)
				Log_print("Invalid argument for '%s': %s", argv[--i], a_e);
			else
				Log_print("Invalid argument for '%s'", argv[--i]);
			return FALSE;
		}
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
	Multimedia_WriteVideo();
	Sound_Update();
	Atari800_nframes++;
}


/* Stub routines to replace text-based UI */

int UI_SelectCartType(int k) {
	libatari800_error_code = LIBATARI800_UNIDENTIFIED_CART_TYPE;
	return CARTRIDGE_NONE;
}

int UI_Initialise(int *argc, char *argv[]) {
	return TRUE;
}

void UI_Run(void) {
	;
}

int UI_is_active;
int UI_alt_function;
int UI_current_function;
char UI_atari_files_dir[UI_MAX_DIRECTORIES][FILENAME_MAX];
char UI_saved_files_dir[UI_MAX_DIRECTORIES][FILENAME_MAX];
int UI_n_atari_files_dir;
int UI_n_saved_files_dir;
int UI_show_hidden_files = FALSE;


int main(int argc, char **argv) {
	int status;

	status = libatari800_init(argc, argv);
	if (!status) {
		return status;
	}

	printf("emulation: fps=%f\n", libatari800_get_fps());
	printf("output file: %s\n", output_media_file);
	printf("commands:\n");
	GLOBALS_ShowCommands();

	GLOBALS_RunCommands();

	libatari800_exit();
}

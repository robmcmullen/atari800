/*
 * headless/args.c - Scriptable atari800 - command line arguments
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

#include "util.h"
#include "log.h"
#include "headless/args.h"
#include "headless/commands.h"


int Headless_Initialise(int *argc, char *argv[])
{
	int i, j;
	int count;

	for (i = j = 1; i < *argc; i++) {
		int i_a = (i + 1 < *argc); /* is argument available? */
		int a_m = FALSE; /* error, argument missing! */
		int a_i = FALSE; /* error, argument invalid! */
		char *a_e = NULL; /* error message */

		if (strcmp(argv[i], "-o") == 0) {
			if (i_a) {
				if (!Headless_SetOutputFile(argv[++i])) {
					a_i = TRUE;
					a_e = "Specify .wav to save sound file; .avi to save movie file";
				}
			}
			else a_m = TRUE;
		}
		else if (strcmp(argv[i], "-step") == 0) {
			if (i_a) {
				scanframes(argv[++i], &count);
				a_i = !Headless_AddIntCommand(COMMAND_STEP, count);
			}
			else a_m = TRUE;
		}
		else if (strcmp(argv[i], "-rec") == 0) {
			if (i_a) {
				char *mode = argv[++i];
				if (strcmp(mode, "on") == 0)
					a_i = !Headless_AddIntCommand(COMMAND_RECORD, 1);
				else if (strcmp(mode, "off") == 0)
					a_i = !Headless_AddIntCommand(COMMAND_RECORD, 0);
				else {
					a_i = TRUE;
				}
			}
			else a_m = TRUE;
		}
		else if (strcmp(argv[i], "-type") == 0) {
			if (i_a) {
				a_i = !Headless_AddStrCommand(COMMAND_TYPE, argv[++i]);
			}
			else a_m = TRUE;
		}
		else if (strcmp(argv[i], "-debug-screen") == 0) {
			HEADLESS_debug_screen = TRUE;
		}
		else if (strcmp(argv[i], "-keydown-time") == 0) {
			if (i_a) {
				HEADLESS_keydown_time = Util_sscandec(argv[++i]);
				if (HEADLESS_keydown_time < 1) {
					Log_print("Invalid keydown time, must be greater than 0");
					return FALSE;
				}
			}
			else a_m = TRUE;
		}
		else if (strcmp(argv[i], "-keyup-time") == 0) {
			if (i_a) {
				HEADLESS_keyup_time = Util_sscandec(argv[++i]);
			}
			else a_m = TRUE;
		}
		else {
			if (strcmp(argv[i], "-help") == 0) {
				Log_print("\t-o <file>            Set output file (.wav or .avi)");
				Log_print("\t-rec on|off          Control if frames are recorded to media file");
				Log_print("\t-step <num>          Run emulator for <num> frames");
				Log_print("\t-type <keystrokes>   Type keystrokes into emulator");
				Log_print("\t-keydown-time <num>  Number of frames to hold key down");
				Log_print("\t-keyup-time <num>    Number of frames to release all keys before next key down");
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

	return TRUE;
}

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

#include "atari.h"
#include "multimedia.h"
#include "util.h"
#include "libatari800/libatari800.h"

#include "headless/main.h"
#include "headless/globals.h"


char output_media_file[FILENAME_MAX];
int output_media_type = OUTPUT_MEDIA_TYPE_UNKNOWN;

#define COMMAND_LIST_MAX 1024
COMMAND_entry command_list[COMMAND_LIST_MAX];
int command_list_size = 0;

#define STRING_STORAGE_CHUNK 1024
static char *string_storage = NULL;
static int string_storage_size = 0;
static int string_storage_index = 0;
static int start_recording_at_valid_dlist = TRUE;
static int booted = FALSE;

static input_template_t input;
static int current_recording_state;
static int keypress_time;
static int keyrelease_time;

int GLOBALS_SetOutputFile(char *filename)
{
	Util_strlcpy(output_media_file, filename, sizeof(output_media_file));
	if (Util_striendswith(output_media_file, ".wav"))
		output_media_type = OUTPUT_MEDIA_TYPE_WAV;
	else if (Util_striendswith(output_media_file, ".avi"))
		output_media_type = OUTPUT_MEDIA_TYPE_AVI;
	else
		output_media_type = OUTPUT_MEDIA_TYPE_UNKNOWN;
	return output_media_type;
}

int GLOBALS_AddIntCommand(int cmd, int arg)
{
	int entry;

	entry = command_list_size++;
	command_list[entry].command = cmd;
	command_list[entry].number = arg;

	return TRUE;
}

static int reserve_string(char *str)
{
	int len;
	int index;

	len = strlen(str);
	if ((len + string_storage_index) > string_storage_size) {
		int new_size = string_storage_size + STRING_STORAGE_CHUNK;
		string_storage = Util_realloc(string_storage, new_size);
	}

	return len;
}

static int add_string(char *str)
{
	int len;
	int index;

	len = reserve_string(str);
	index = string_storage_index;
	strcpy(string_storage + index, str);
	string_storage_index += len + 1; /* include space for nul char */
	return index;
}

static char *get_string(int index)
{
	return string_storage + index;
}

int keystrokes_to_input(char *str, input_template_t *out)
{
	char c;
	int len;

	len = 1;
	c = *str++;
	if (out) out->special = 0; /* used as flag to indicate ready to process key */
	if (c == '{') {
		if (Util_strnicmp(str, "ret}", 4) == 0) {
			if (out) out->keychar = '\n';
			len+= 4;
		}
		else if (Util_strnicmp(str, "esc}", 4) == 0) {
			if (out) out->keychar = '\033';
			len+= 4;
		}
		else if (Util_strnicmp(str, "tab}", 4) == 0) {
			if (out) out->keychar = '\t';
			len+= 4;
		}
		else if (Util_strnicmp(str, "del}", 4) == 0) {
			if (out) out->keychar = 127;
			len+= 4;
		}
		else if (Util_strnicmp(str, "delete}", 7) == 0) {
			if (out) out->keychar = 127;
			len+= 7;
		}
		else if (Util_strnicmp(str, "ins}", 4) == 0) {
			if (out) out->keycode = AKEY_INSERT_CHAR;
			len+= 4;
		}
		else if (Util_strnicmp(str, "insert}", 7) == 0) {
			if (out) out->keycode = AKEY_INSERT_CHAR;
			len+= 7;
		}
		else if (Util_strnicmp(str, "back}", 5) == 0) {
			if (out) out->keychar = '\b';
			len+= 5;
		}
		else if (Util_strnicmp(str, "backspace}", 10) == 0) {
			if (out) out->keychar = '\b';
			len+= 10;
		}
		else if (Util_strnicmp(str, "atari}", 6) == 0) {
			if (out) out->keycode = AKEY_ATARI;
			len+= 6;
		}
		else if (Util_strnicmp(str, "left}", 5) == 0) {
			if (out) out->keycode = AKEY_LEFT;
			len+= 5;
		}
		else if (Util_strnicmp(str, "right}", 6) == 0) {
			if (out) out->keycode = AKEY_RIGHT;
			len+= 6;
		}
		else if (Util_strnicmp(str, "up}", 3) == 0) {
			if (out) out->keycode = AKEY_UP;
			len+= 3;
		}
		else if (Util_strnicmp(str, "down}", 5) == 0) {
			if (out) out->keycode = AKEY_DOWN;
			len+= 5;
		}
		else if (Util_strnicmp(str, "caps}", 5) == 0) {
			if (out) out->keycode = AKEY_CAPSLOCK;
			len+= 5;
		}
		else if (Util_strnicmp(str, "help}", 5) == 0) {
			if (out) out->keycode = AKEY_HELP;
			len+= 5;
		}
		else if (Util_strnicmp(str, "shift}", 6) == 0) {
			if (out) {
				out->shift = 1;
				out->special = 255;
			}
			len+= 6;
		}
		else if (Util_strnicmp(str, "ctrl}", 5) == 0) {
			if (out) {
				out->control = 1;
				out->special = 255;
			}
			len+= 5;
		}
		else {
			return -1;
		}
	}
	else if (out) {
		out->keychar = c;
	}
	return len;
}

static int count_keystrokes(char *text)
{
	int size;
	int num_chars;
	int len = strlen(text);

	num_chars = 0;
	libatari800_clear_input_array(&input);
	while (len > 0) {
		size = keystrokes_to_input(text, &input);
		if (size <= 0) break; /* error, shouldn't happen because string is already parsed */
		if (input.special != 255) {
			num_chars++;
		}
		text += size;
		len -= size;
	}

	return num_chars;
}

int GLOBALS_AddStrCommand(int cmd, char *arg)
{
	int count;
	int index;
	int entry;
	char *str;
	int len = strlen(arg);

	str = arg;
	while (len > 0) {
		count = keystrokes_to_input(str, NULL);
		if (count < 0) {
			printf("Unknown key definition starting at '%s'\n", str);
			return FALSE;
		}
		str += count;
		len -= count;
	}

	index = add_string(arg);
	entry = command_list_size++;
	command_list[entry].command = cmd;
	command_list[entry].number = index;

	return TRUE;
}

static void print_command(char *prefix, int i) {
	COMMAND_entry *e;

	e = &command_list[i];
	printf("%s %d: command=%d number=%d\t", prefix, i, e->command, e->number);
	switch (e->command) {
		case COMMAND_RECORD:
			if (e->number) printf("record on\n");
			else printf("record off\n");
			break;
		case COMMAND_STEP:
			printf("step %d\n", e->number);
			break;
		case COMMAND_TYPE:
			printf("type '%s'\n", get_string(e->number));
			break;
		default:
			printf("UNKNOWN\n");
			break;
	}
}

void GLOBALS_ShowCommands(void)
{
	int i;

	for (i = 0; i < command_list_size; i++) {
		print_command("entry", i);
	}
}

/* These are hard-coded UTF-8 encoded Unicode characters. Doing it this way
	has the advantage that the compiler (and the developer's text editor)
	doesn't have to have Unicode support, and the C library doesn't have to
	support wide characters. The disadvantage is that no other encodings
	besides UTF-8 can be supported (which shouldn't be a real problem these
	days). */
static const char *utf8_chars[] = {
/* 32 - 39 */ " ", "!", "\"", "#", "$", "%", "&", "'",
/* 40 - 47 */ "(", ")", "*", "+", ",", "-", ".", "/",
/* 48 - 55 */ "0", "1", "2", "3", "4", "5", "6", "7",
/* 56 - 63 */ "8", "9", ":", ";", "<", "=", ">", "?",
/* 64 - 71 */ "@", "A", "B", "C", "D", "E", "F", "G",
/* 72 - 79 */ "H", "I", "J", "K", "L", "M", "N", "O",
/* 80 - 87 */ "P", "Q", "R", "S", "T", "U", "V", "W",
/* 88 - 95 */ "X", "Y", "Z", "[", "\\", "]", "^", "_",
/* 0 - 7 */ "\xe2\x99\xa5", "\xe2\x94\xa3", "\xe2\x94\x83", "\xe2\x94\x9b", "\xe2\x94\xab", "\xe2\x94\x93", "\xe2\x95\xb1", "\xe2\x95\xb2",
/* 8 - 15 */ "\xe2\x97\xa2", "\xe2\x96\x97", "\xe2\x97\xa3", "\xe2\x96\x9d", "\xe2\x96\x98", "\xe2\x96\x94", "\xe2\x96\x81", "\xe2\x96\x96",
/* 16 - 23 */ "\xe2\x99\xa3", "\xe2\x94\x8f", "\xe2\x94\x81", "\xe2\x95\x8b", "\xe2\x9a\xab", "\xe2\x96\x84", "\xe2\x96\x8e", "\xe2\x94\xb3",
/* 24 - 31 */ "\xe2\x94\xbb", "\xe2\x96\x8c", "\xe2\x94\x97", "\xe2\x90\x9b", "\xe2\x86\x91", "\xe2\x86\x93", "\xe2\x86\x90", "\xe2\x86\x92",
/* 96 - 103 */ "\xe2\x97\x86", "a", "b", "c", "d", "e", "f", "g",
/* 104 - 111 */ "h", "i", "j", "k", "l", "m", "n", "o",
/* 112 - 119 */ "p", "q", "r", "s", "t", "u", "v", "w",
/* 120 - 127 */ "x", "y", "z", "\xe2\x99\xa0", "|", "\xe2\x86\xb0", "\xe2\x97\x80", "\xe2\x96\xb6",
};

/* Print an ATASCII character, with support for graphics characters if
	UTF-8 is available, and inverse video if ANSI is available. */
static void print_atascii_char(UBYTE c, int ansi) {
	int inv = c & 0x80;

	if (ansi) {
		/* ESC[7m = reverse video attribute on */
		if(inv) printf("\x1b[7m");
	}
	else {
		if(inv) {
			putchar('.');
			return;
		}
	}

	printf("%s", utf8_chars[c & 0x7f]);

	if (ansi) {
		/* ESC[0m = all attributes off */
		if(inv) printf("\x1b[0m");
	}
}

static void debug_screen(int i, int count, char *label)
{
	/* print out portion of screen, assuming graphics 0 display list */
	UBYTE *mem = libatari800_get_main_memory_ptr();
	UBYTE *dlist = mem + (mem[0x230] + 256 * mem[0x231]);
	UBYTE *screen = mem + (dlist[4] + 256 * dlist[5]);
	int x, y;

	printf("Frame %d: %s", libatari800_get_frame_number(), current_recording_state ? "recorded" : "not recorded");
	if (i < 0) {
		printf(" (%s)", label);
	}
	else {
		printf(" (%s %d of %d)", label, i, count);
	}
	printf(" dlist=$%04x screen=$%04x\n", (int)(dlist - mem), (int)(screen - mem));
	if (HEADLESS_debug_screen && current_recording_state && dlist[3] == 0x42) {
		for (y = 0; y < 24; y++) {
			for (x = 0; x < 40; x++) {
				unsigned char c = screen[x];
				print_atascii_char(c, TRUE);
			}
			printf("\n");
			screen += 40;
		}
	}
}

static void record(int state)
{
	current_recording_state = state;
	Multimedia_Pause(!current_recording_state);
	start_recording_at_valid_dlist = FALSE;
}

static int process_frame(int i, int count, char *label)
{
	UBYTE *mem;
	UBYTE *dlist;
	UBYTE *screen;

	libatari800_next_frame(&input);
	mem = libatari800_get_main_memory_ptr();
	dlist = mem + (mem[0x230] + 256 * mem[0x231]);
	if ((int)(dlist - mem) > 0) {
		screen = mem + (dlist[4] + 256 * dlist[5]);
	}
	else {
		screen = NULL;
	}

	debug_screen(i, count, label);
	if (screen && start_recording_at_valid_dlist && !current_recording_state) {
		record(TRUE);
		start_recording_at_valid_dlist = FALSE;
	}
	return (int)(dlist - mem);
}

static void step(int count)
{
	int i;

	for (i = 0; i < count; i++) {
		process_frame(i + 1, count, "step");
	}
}

static void step_until_valid_dlist()
{
	int dlist;

	do {
		dlist = process_frame(-1, -1, "booting");
	} while (!dlist && libatari800_get_frame_number() < 100);
	booted = TRUE;
}

static void type(int index)
{
	int i;
	int size;
	char *text = get_string(index);
	char *str;
	UBYTE c;
	int len = strlen(text);
	int num_chars = count_keystrokes(text);

	index = 1;
	while (len > 0) {
		size = keystrokes_to_input(text, &input);
		if (size <= 0) break; /* error, shouldn't happen because string is already parsed */
		if (input.special != 255) {
			for (i = 0; i < keypress_time; i++) {
				process_frame(index, num_chars, "key pressed");
			}
			libatari800_clear_input_array(&input);
			for (i = 0; i < keyrelease_time; i++) {
				process_frame(index, num_chars, "key released");
			}
			index++;
		}
		text += size;
		len -= size;
	}
}

void GLOBALS_RunCommands(void)
{
	int i;
	int val;
	int show_screen = TRUE;

	libatari800_clear_input_array(&input);

	keypress_time = 1;
	keyrelease_time = 3;
	current_recording_state = FALSE;
	booted = FALSE;

	if (output_media_type == OUTPUT_MEDIA_TYPE_WAV) {
		i = Multimedia_OpenSoundFile(output_media_file);
	}
	else if (output_media_type == OUTPUT_MEDIA_TYPE_AVI) {
		i = Multimedia_OpenVideoFile(output_media_file);
	}
	else {
		i = 1;
	}
	if (!i) {
		printf("Error opening %s\n", output_media_file);
		return;
	}
	if (Multimedia_IsFileOpen()) Multimedia_Pause(TRUE);

	for (i = 0; i < command_list_size; i++) {
		print_command("processing", i);
		val = command_list[i].number;
		switch (command_list[i].command) {
			case COMMAND_RECORD:
				record(val);
				break;
			case COMMAND_STEP:
				if (!booted) step_until_valid_dlist();
				step(val);
				break;
			case COMMAND_TYPE:
				if (!booted) step_until_valid_dlist();
				type(val);
				break;
		}
	}
}

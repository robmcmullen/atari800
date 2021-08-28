/*
 * headless/commands.c - Scriptable atari800 - command processing
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

#include "headless/commands.h"


char HEADLESS_media_file[FILENAME_MAX];
int HEADLESS_media_type = HEADLESS_MEDIA_TYPE_UNKNOWN;
int HEADLESS_keydown_time = 1;
int HEADLESS_keyup_time = 3;
int HEADLESS_debug_screen = FALSE;

#define COMMAND_LIST_MAX 1024
COMMAND_t command_list[COMMAND_LIST_MAX];
int command_list_size = 0;

#define STRING_STORAGE_CHUNK 1024
static char *string_storage = NULL;
static int string_storage_size = 0;
static int string_storage_index = 0;
static int start_recording_at_valid_dlist = TRUE;
static int booted = FALSE;

static input_template_t input;
static int current_recording_state;

int Headless_SetOutputFile(char *filename)
{
	Util_strlcpy(HEADLESS_media_file, filename, sizeof(HEADLESS_media_file));
	if (Util_striendswith(HEADLESS_media_file, ".wav"))
		HEADLESS_media_type = HEADLESS_MEDIA_TYPE_WAV;
	else if (Util_striendswith(HEADLESS_media_file, ".avi"))
		HEADLESS_media_type = HEADLESS_MEDIA_TYPE_AVI;
	else
		HEADLESS_media_type = HEADLESS_MEDIA_TYPE_UNKNOWN;
	return HEADLESS_media_type;
}

int Headless_AddIntCommand(int cmd, int arg)
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

static int scanframes(char *str, int *result) {
	char c;
	char *start = str;
	int found_seconds = FALSE;

	*result = 0;
	c = *str;

	/* remove leading blanks or = sign */
	while (c == ' ' || c == '=') {
		c = *str++;
	}

	/* stop at first non-number; successful if '}', unsuccessful if anything else */
	while (c) {
		if (c >= '0' && c <= '9' && !found_seconds) {
			*result = 10 * (*result) + c - '0';
			c = *str++;
		}
		else if ((c == 's' || c == 'S') && !found_seconds) {
			*result = (*result) * (Atari800_tv_mode == Atari800_TV_PAL ? 50 : 60);
			found_seconds = TRUE;
			c = *str++;
		}
		else if (c == '}') {
			break;
		}
		else {
			*result = -1;
			break;
		}
	}
	return (int)(str - start);
}

int next_token_from_string(char *str, COMMAND_t *out)
{
	char c;
	int len;

	len = 0;
	c = *str;
	out->modifier = 0; /* used as flag to indicate ready to process key */
	if (!c) return 0;
	str++;
	len++;
	if (c == '{') {
		if (Util_strnicmp(str, "ret}", 4) == 0) {
			out->command = COMMAND_KEYSTROKE;
			out->keychar = '\n';
			len+= 4;
		}
		else if (Util_strnicmp(str, "esc}", 4) == 0) {
			out->command = COMMAND_KEYSTROKE;
			out->keychar = '\033';
			len+= 4;
		}
		else if (Util_strnicmp(str, "tab}", 4) == 0) {
			out->command = COMMAND_KEYSTROKE;
			out->keychar = '\t';
			len+= 4;
		}
		else if (Util_strnicmp(str, "del}", 4) == 0) {
			out->command = COMMAND_KEYSTROKE;
			out->keychar = 127;
			len+= 4;
		}
		else if (Util_strnicmp(str, "delete}", 7) == 0) {
			out->command = COMMAND_KEYSTROKE;
			out->keychar = 127;
			len+= 7;
		}
		else if (Util_strnicmp(str, "ins}", 4) == 0) {
			out->command = COMMAND_KEYSTROKE;
			out->keycode = AKEY_INSERT_CHAR;
			len+= 4;
		}
		else if (Util_strnicmp(str, "insert}", 7) == 0) {
			out->command = COMMAND_KEYSTROKE;
			out->keycode = AKEY_INSERT_CHAR;
			len+= 7;
		}
		else if (Util_strnicmp(str, "back}", 5) == 0) {
			out->command = COMMAND_KEYSTROKE;
			out->keychar = '\b';
			len+= 5;
		}
		else if (Util_strnicmp(str, "backspace}", 10) == 0) {
			out->command = COMMAND_KEYSTROKE;
			out->keychar = '\b';
			len+= 10;
		}
		else if (Util_strnicmp(str, "atari}", 6) == 0) {
			out->command = COMMAND_KEYSTROKE;
			out->keycode = AKEY_ATARI;
			len+= 6;
		}
		else if (Util_strnicmp(str, "left}", 5) == 0) {
			out->command = COMMAND_KEYSTROKE;
			out->keycode = AKEY_LEFT;
			len+= 5;
		}
		else if (Util_strnicmp(str, "right}", 6) == 0) {
			out->command = COMMAND_KEYSTROKE;
			out->keycode = AKEY_RIGHT;
			len+= 6;
		}
		else if (Util_strnicmp(str, "up}", 3) == 0) {
			out->command = COMMAND_KEYSTROKE;
			out->keycode = AKEY_UP;
			len+= 3;
		}
		else if (Util_strnicmp(str, "down}", 5) == 0) {
			out->command = COMMAND_KEYSTROKE;
			out->keycode = AKEY_DOWN;
			len+= 5;
		}
		else if (Util_strnicmp(str, "caps}", 5) == 0) {
			out->command = COMMAND_KEYSTROKE;
			out->keycode = AKEY_CAPSLOCK;
			len+= 5;
		}
		else if (Util_strnicmp(str, "help}", 5) == 0) {
			out->command = COMMAND_KEYSTROKE;
			out->keycode = AKEY_HELP;
			len+= 5;
		}
		else if (Util_strnicmp(str, "shift}", 6) == 0) {
			out->shift = 1;
			out->modifier = TRUE;
			len+= 6;
		}
		else if (Util_strnicmp(str, "ctrl}", 5) == 0) {
			out->control = 1;
			out->modifier = TRUE;
			len+= 5;
		}
		else if (Util_strnicmp(str, "step", 4) == 0) {
			len+= 4;
			str+= 4;
			c = *str;
			if (c == '}') {
				out->command = COMMAND_STEP;
				out->number = 1;
				len++;
			}
			else {
				out->command = COMMAND_STEP;
				len += scanframes(str, &out->number);
			}
			if (out->number < 0) return -1;
		}
		else if (Util_strnicmp(str, "rec}", 4) == 0) {
			out->command = COMMAND_RECORD;
			out->number = 1;
			len += 4;
		}
		else if (Util_strnicmp(str, "rec on}", 7) == 0) {
			out->command = COMMAND_RECORD;
			out->number = 1;
			len += 7;
		}
		else if (Util_strnicmp(str, "rec off}", 8) == 0) {
			out->command = COMMAND_RECORD;
			out->number = 0;
			len += 8;
		}
		else {
			return -1;
		}
	}
	else {
		out->command = COMMAND_KEYSTROKE;
		out->keychar = c;
	}
	return len;
}

int next_command_from_string(char *str, COMMAND_t *out)
{
	int total;
	int size;

	total = 0;
	out->keycode = 0;
	out->keychar = 0;
	out->shift = 0;
	out->control = 0;
	do {
		size = next_token_from_string(str, out);
		printf("parsing: %s\n", str);
		if (size < 0) return size;
		str += size;
		total += size;
	} while (size && out->modifier);
	return total;
}

static int count_commands(char *text)
{
	int size;
	int num_commands;
	COMMAND_t cmd;

	num_commands = 0;
	cmd.modifier = FALSE;
	do {
		size = next_command_from_string(text, &cmd);
		if (size > 0) {
			num_commands++;
			text += size;
		}
	} while (size > 0);

	return num_commands;
}

int Headless_AddStrCommand(int cmd, char *arg)
{
	int count;
	int index;
	int entry;
	char *str;
	int len = strlen(arg);
	COMMAND_t out;

	str = arg;
	while (len > 0) {
		count = next_token_from_string(str, &out);
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

static void print_command(COMMAND_t *e) {
	printf("command=%d number=%d\t", e->command, e->number);
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
		case COMMAND_KEYSTROKE:
			printf("keystroke: keycode=%d keychar=%d shift=%d control=%d'\n", e->keycode, e->keychar, e->shift, e->control);
			break;
		default:
			printf("UNKNOWN\n");
			break;
	}
}

void Headless_ShowCommands(void)
{
	int i;

	for (i = 0; i < command_list_size; i++) {
		printf("entry %d: ", i);
		print_command(&command_list[i]);
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
	switch (libatari800_error_code) {
		case 0:
			break;
		case LIBATARI800_DLIST_ERROR:
			libatari800_error_code = 0;
			break;
		default:
			return -1;
	}
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

static int step(COMMAND_t *cmd)
{
	int i;
	int result;

	for (i = 0; i < cmd->number; i++) {
		result = process_frame(i + 1, cmd->number, "step");
		if (result < 0) return result;
	}

	return 1;
}

static int step_until_valid_dlist()
{
	int dlist;

	do {
		dlist = process_frame(-1, -1, "booting");
	} while (dlist == 0 && libatari800_get_frame_number() < 100);
	booted = TRUE;
	return dlist;
}

static int keystroke(COMMAND_t *cmd)
{
	int i;
	int result;

	libatari800_clear_input_array(&input);
	input.keycode = cmd->keycode;
	input.keychar = cmd->keychar;
	input.shift = cmd->shift;
	input.control = cmd->control;
	for (i = 0; i < HEADLESS_keydown_time; i++) {
		result = process_frame(cmd->index, cmd->number, "key down");
		if (result < 0) return result;
	}
	libatari800_clear_input_array(&input);
	for (i = 0; i < HEADLESS_keyup_time; i++) {
		result = process_frame(cmd->index, cmd->number, "key up");
		if (result < 0) return result;
	}
	return 1;
}

static int type(COMMAND_t *cmd)
{
	int size;
	char *text = get_string(cmd->number);
	int len = strlen(text);
	int num_commands = count_commands(text);
	COMMAND_t sub_cmd;
	int result;
	
	sub_cmd.number = num_commands;
	sub_cmd.index = 1;
	while (len > 0) {
		size = next_command_from_string(text, &sub_cmd);
		if (size <= 0) break; /* error, shouldn't happen because string is already parsed */
		result = Headless_ProcessCommand(&sub_cmd);
		if (result < 0) return result;
		text += size;
		len -= size;
		sub_cmd.number = num_commands;
		sub_cmd.index++;
	}
	return 1;
}

int Headless_ProcessCommand(COMMAND_t *cmd)
{
	int result = 1;

	if (cmd->command == COMMAND_RECORD) {
		record(cmd->number);
	}
	else {
		if (!booted) result = step_until_valid_dlist();
		if (result < 0) return result;
		switch (cmd->command) {
			case COMMAND_STEP:
				result = step(cmd);
				break;
			case COMMAND_TYPE:
				result = type(cmd);
				break;
			case COMMAND_KEYSTROKE:
				result = keystroke(cmd);
				break;
		}
	}
	return result;
}

void Headless_RunCommands(void)
{
	int i;
	COMMAND_t *cmd;
	int result;

	libatari800_clear_input_array(&input);

	current_recording_state = FALSE;
	booted = FALSE;

	if (HEADLESS_media_type == HEADLESS_MEDIA_TYPE_WAV) {
		i = Multimedia_OpenSoundFile(HEADLESS_media_file);
	}
	else if (HEADLESS_media_type == HEADLESS_MEDIA_TYPE_AVI) {
		i = Multimedia_OpenVideoFile(HEADLESS_media_file);
	}
	else {
		i = 1;
	}
	if (!i) {
		printf("Error opening %s\n", HEADLESS_media_file);
		return;
	}
	if (Multimedia_IsFileOpen()) Multimedia_Pause(TRUE);

	for (i = 0; i < command_list_size; i++) {
		cmd = &command_list[i];
		printf("processing %d: ", i);
		print_command(cmd);
		result = Headless_ProcessCommand(cmd);
		if (result < 0 || libatari800_error_code) {
			printf("Exiting, error encountered: %s\n", libatari800_error_message());
		}
	}
}

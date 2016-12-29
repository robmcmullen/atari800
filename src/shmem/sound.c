/*
 * sdl/sound.c - SDL library specific port code - sound output
 *
 * Copyright (c) 2001-2002 Jacek Poplawski
 * Copyright (C) 2001-2013 Atari800 development team (see DOC/CREDITS)
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

#include "atari.h"
#include "log.h"
#include "platform.h"
#include "sound.h"

static unsigned int hw_buffer_size = 0;

int PLATFORM_SoundSetup(Sound_setup_t *setup)
{
#if 0
	if (setup->buffer_frames == 0)
	    /* Set buffer_frames automatically. */
		setup->buffer_frames = Sound_NextPow2(setup->freq * 4 / 50);

	 = setup->freq;
	 = setup->channels;
	 = setup->sample_size == 2;
	 = TRUE;
	hw_buffer_size = setup->buffer_frames * setup->sample_size * setup->channels;
	if (hw_buffer_size == 0)
	        return FALSE;
	setup->buffer_frames = hw_buffer_size / setup->sample_size / setup->channels;
#endif
	hw_buffer_size = 65536;

	return TRUE;
}

void PLATFORM_SoundExit(void)
{
}

void PLATFORM_SoundPause(void)
{
}

void PLATFORM_SoundContinue(void)
{
}

unsigned int PLATFORM_SoundAvailable(void)
{
	/*return buffer_frames*channels*sample_size;*/
	return hw_buffer_size;
}

void PLATFORM_SoundWrite(UBYTE const *buffer, unsigned int size)
{
}

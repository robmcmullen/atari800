/*
 * sdl/init.c - Shared Memore specific port code - initialisation routines
 *
 * Copyright (c) 2012 Tomasz Krasuski
 * Copyright (C) 2012 Atari800 development team (see DOC/CREDITS)
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

#include <stdlib.h>
#include <string.h>

#include "atari.h"
#include "log.h"
#include "platform.h"
#ifdef SOUND
#include "sound.h"
#endif
#include "shmem/init.h"

static int have_shared = 0;

unsigned char *shared_memory = NULL;

static unsigned char input_snapshot[sizeof(input_template_t)];

static unsigned char fake_shared_memory[SHMEM_TOTAL_SIZE];

unsigned char *SHMEM_DebugGetFakeMemory(void) {
    return &fake_shared_memory[0];
}
int SHMEM_Initialise(void)
{
	/* initialize shared memory here */
    if (!have_shared) {
        if (!SHMEM_AcquireMemory())
            return FALSE;
        have_shared = 1;
        atexit(SHMEM_ReleaseMemory);
    }
	return TRUE;
}

void SHMEM_Exit(void)
{
}

/* use memory that's not managed by the C code */
int SHMEM_UseMemory(unsigned char *raw, int len) {
    if (len >= SHMEM_TOTAL_SIZE) {
        shared_memory = raw;
        have_shared = 2;
        return TRUE;
    }
    return FALSE;
}

int SHMEM_AcquireMemory(void)
{
	/* get shared memory */
    memset(fake_shared_memory, 0, SHMEM_TOTAL_SIZE);
    shared_memory = &fake_shared_memory[0];
    return TRUE;
}

void SHMEM_ReleaseMemory(void)
{
    if (have_shared == 1) {
        /* release shared memory that was allocated by SHMEM_AcquireMemory */
        ;
    }
    return;
}

input_template_t *SHMEM_GetInputArray(void) {
    return (input_template_t *)&shared_memory[SHMEM_INPUT_OFFSET];
}

/* Operate on a copy of the input array so that the values aren't changed out
 from underneath by the front end (which is running in another process)
*/
void SHMEM_TakeInputArraySnapshot(void) {
    memcpy(input_snapshot, &shared_memory[SHMEM_INPUT_OFFSET], sizeof(input_template_t));
}

input_template_t *SHMEM_GetInputArraySnapshot(void) {
    return (input_template_t *)&input_snapshot;
}

#ifdef SOUND
Sound_setup_t *SHMEM_GetSoundArray(void) {
    return (Sound_setup_t *)&shared_memory[SHMEM_SOUND_OFFSET];
}
#endif

unsigned char *SHMEM_GetVideoArray(void) {
    return &shared_memory[SHMEM_VIDEO_OFFSET];
}

unsigned char *SHMEM_GetStateSaveArray(void) {
    return &shared_memory[SHMEM_STATE_OFFSET];
}

/*
vim:ts=4:sw=4:
*/
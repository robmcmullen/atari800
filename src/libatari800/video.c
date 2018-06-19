/*
 * libatari800/video.c - Atari800 as a library - video display
 *
 * Copyright (c) 2001-2002 Jacek Poplawski
 * Copyright (C) 2001-2010 Atari800 development team (see DOC/CREDITS)
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

#include <stdio.h>
#include <string.h>

#include "platform.h"
#include "screen.h"
#include "libatari800/video.h"
#include "libatari800/statesav.h"

int frame_count;

unsigned int debug_video;

#ifdef BENCHMARK
/* debug times:
   unoptimized: 10000 x copy_slow: 2.414133
   gcc -O3: 10000 x copy_slow: 0.094432
            10000 x copy_slow_aligned: 0.077450
            10000 x copy_slow_unrolled: 0.300327
            10000 x copy_slow_unrolled: 0.095876

   the moral: it doesn't really matter aligned or unaligned, just
   use optimization!
*/
void copy_slow(unsigned char *dest, unsigned char *src, int x, int x_offset, int y)
{
	int xi;

	src = src + x_offset;
	x_offset *= 2;
	while (y > 0) {
		xi = x;
		while (xi > 0) {
			*dest++ = *src++;
			xi--;
		}
		src += x_offset;
		y--;
	}
}


void copy_slow_aligned(unsigned int *dest, unsigned int *src, int x, int x_offset, int y)
{
	int xi;

	x /= sizeof(int);
	x_offset /= sizeof(int);
	src = src + x_offset;
	x_offset *= 2;
	while (y > 0) {
		xi = x;
		while (xi > 0) {
			*dest++ = *src++;
			xi--;
		}
		src += x_offset;
		y--;
	}
}


#include <sys/time.h>
#include <sys/resource.h>

static double get_time()
{
    struct timeval t;
    struct timezone tzp;
    gettimeofday(&t, &tzp);
    return t.tv_sec + t.tv_usec*1e-6;
}

static void copy_screen(unsigned int *dest, unsigned int *src, int x, int x_offset, int y) {
	/* slow, simple copy */
	do {
		double t0 = get_time();
		int i;

		for(i=0; i<10000; i++) {
			copy_slow(dest, src, x, x_offset, y);
		}
		printf("%d x copy_slow: %lf\n", i, get_time() - t0);
	} while(0);
	/* aligned copy */
	do {
		double t0 = get_time();
		int i;

		for(i=0; i<10000; i++) {
			copy_slow_aligned((unsigned int *)dest, (unsigned int *)src, x, x_offset, y);
		}
		printf("%d x copy_slow_aligned: %lf\n", i, get_time() - t0);
	} while(0);
}
#else
static void copy_screen(unsigned char *dest, unsigned char *src, int x, int x_offset, int y)
{
	int xi;

	src = src + x_offset;
	x_offset *= 2;
	while (y > 0) {
		xi = x;
		while (xi > 0) {
			*dest++ = *src++;
			xi--;
		}
		src += x_offset;
		y--;
	}
}
#endif

void PLATFORM_DisplayScreen(void)
{
	unsigned char *src, *dest;
	int x, y;
	int x_offset;
	int x_count;

	if (debug_video)
		printf("display frame #%d (%d-%d, %d-%d)\n", frame_count, Screen_visible_x1, Screen_visible_x2, Screen_visible_y1, Screen_visible_y2);

	/* set up screen copy of middle 336 pixels to shared memory buffer */
	x = Screen_visible_x2 - Screen_visible_x1;
	x_offset = (Screen_WIDTH - x) / 2;
	y = Screen_visible_y2 - Screen_visible_y1;

	src = (unsigned char *)Screen_atari;
	dest = LIBATARI800_Video_array;

	copy_screen(dest, src, x, x_offset, y);

	if (debug_video) {
	/* let emulator stabilize, then print out sample of screen bytes */
		if (frame_count > 100) {
			LIBATARI800_DebugVideo(LIBATARI800_Video_array);
		}
	}
}

void LIBATARI800_DebugVideo(unsigned char *mem) {
	int x, y;

	printf("frame %d\n", frame_count);
	mem += 336 * 24;
	for (y = 0; y < 24; y++) {
		for (x = 8; x < 87; x++) {
			/*printf(" %02x", src[x]);*/
			/* print out text version of screen, assuming graphics 0 memo pad boot screen */
			unsigned char c = mem[x];
			if (c == 0)
				printf(" ");
			else if (c == 0x94)
				printf(".");
			else if (c == 0x9a)
				printf("X");
			else
				printf("?");
		}
		putchar('\n');
		mem += 336;
	}
}

int LIBATARI800_Video_Initialise(int *argc, char *argv[]) {
	return TRUE;
}

void LIBATARI800_Video_Exit(void) {
}

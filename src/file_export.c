/*
 * file_export.c - low level interface for saving to various file formats
 *
 * Copyright (C) 1995-1998 David Firth
 * Copyright (C) 1998-2005 Atari800 development team (see DOC/CREDITS)
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
#include <stdlib.h>
#include "file_export.h"
#include "screen.h"
#include "colours.h"
#include "cfg.h"
#include "util.h"
#include "log.h"

#if !defined(BASIC) && !defined(CURSES_BASIC)
#include "codecs/image.h"
#endif

#ifdef MULTIMEDIA
#include "codecs/container.h"

#ifdef SOUND
#include "codecs/audio.h"
#endif

#ifdef AVI_VIDEO_RECORDING
#include "codecs/video.h"
#endif

#endif /* MULTIMEDIA */


#if defined(HAVE_LIBPNG) || defined(HAVE_LIBZ)
int FILE_EXPORT_compression_level = 6;
#endif

#ifdef MULTIMEDIA

#ifdef SOUND
#define DEFAULT_SOUND_FILENAME_FORMAT "atari###.wav"
static char sound_filename_format[FILENAME_MAX];
static int sound_no_last = -1;
static int sound_no_max = 0;
#endif /* SOUND */

#ifdef AVI_VIDEO_RECORDING
#define DEFAULT_VIDEO_FILENAME_FORMAT "atari###.avi"
static char video_filename_format[FILENAME_MAX];
static int video_no_last = -1;
static int video_no_max = 0;
#endif /* AVI_VIDEO_RECORDING */

#endif /* MULTIMEDIA */


int File_Export_Initialise(int *argc, char *argv[])
{
#if defined(HAVE_LIBPNG) || defined(HAVE_LIBZ) || defined(MULTIMEDIA)
	int i;
	int j;

	for (i = j = 1; i < *argc; i++) {
		int i_a = (i + 1 < *argc); /* is argument available? */
		int a_m = FALSE; /* error, argument missing! */
		int a_i = FALSE; /* error, argument invalid! */

		if (0) {}
#if defined(HAVE_LIBPNG) || defined(HAVE_LIBZ)
		else if (strcmp(argv[i], "-compression-level") == 0) {
			if (i_a) {
				FILE_EXPORT_compression_level = Util_sscandec(argv[++i]);
				if (FILE_EXPORT_compression_level < 0 || FILE_EXPORT_compression_level > 9) {
					Log_print("Invalid png/zlib compression level - must be between 0 and 9");
					return FALSE;
				}
			}
			else a_m = TRUE;
		}
#endif
#ifdef SOUND
		else if (strcmp(argv[i], "-aname") == 0) {
			if (i_a)
				sound_no_max = Util_filenamepattern(argv[++i], sound_filename_format, FILENAME_MAX, DEFAULT_SOUND_FILENAME_FORMAT);
			else a_m = TRUE;
		}
#endif
#ifdef AVI_VIDEO_RECORDING
		else if (strcmp(argv[i], "-vname") == 0) {
			if (i_a)
				video_no_max = Util_filenamepattern(argv[++i], video_filename_format, FILENAME_MAX, DEFAULT_VIDEO_FILENAME_FORMAT);
			else a_m = TRUE;
		}
#endif
		else {
			if (strcmp(argv[i], "-help") == 0) {
#if defined(HAVE_LIBPNG) || defined(HAVE_LIBZ)
				Log_print("\t-compression-level <n>");
				Log_print("\t                 Set zlib/PNG compression level 0-9 (default 6)");
#endif
#ifdef SOUND
				Log_print("\t-aname <p>       Set filename pattern for audio recording");
#endif
#ifdef AVI_VIDEO_RECORDING
				Log_print("\t-vname <p>       Set filename pattern for video recording");
#endif
			}
			argv[j++] = argv[i];
		}

		if (a_m) {
			Log_print("Missing argument for '%s'", argv[i]);
			return FALSE;
		} else if (a_i) {
			Log_print("Invalid argument for '%s'", argv[--i]);
			return FALSE;
		}
	}
	*argc = j;
#endif /* defined(HAVE_LIBPNG) || defined(HAVE_LIBZ) || defined(MULTIMEDIA) */

	return TRUE
#ifdef SOUND
		&& CODECS_AUDIO_Initialise(argc, argv)
#endif
#ifdef AVI_VIDEO_RECORDING
		&& CODECS_VIDEO_Initialise(argc, argv)
#endif
	;
}

int File_Export_ReadConfig(char *string, char *ptr)
{
	if (0) {}
#if defined(HAVE_LIBPNG) || defined(HAVE_LIBZ)
	else if (strcmp(string, "COMPRESSION_LEVEL") == 0) {
		int num = Util_sscandec(ptr);
		if (num >= 0 && num <= 9)
			FILE_EXPORT_compression_level = num;
		else return FALSE;
	}
#endif
#ifdef AVI_VIDEO_RECORDING
	else if (CODECS_VIDEO_ReadConfig(string, ptr)) {
	}
#endif
#ifdef SOUND
	else if (CODECS_AUDIO_ReadConfig(string, ptr)) {
	}
#endif
	else return FALSE; /* no match */
	return TRUE; /* matched something */
}

void File_Export_WriteConfig(FILE *fp)
{
#if defined(HAVE_LIBPNG) || defined(HAVE_LIBZ)
	fprintf(fp, "COMPRESSION_LEVEL=%d\n", FILE_EXPORT_compression_level);
#endif
#ifdef AVI_VIDEO_RECORDING
	CODECS_VIDEO_WriteConfig(fp);
#endif
#ifdef SOUND
	CODECS_AUDIO_WriteConfig(fp);
#endif
}


/* fputw and fputl are utility functions to write values as little-endian format
   regardless of the endianness of the platform. */

/* write 16-bit word as little endian */
void fputw(UWORD x, FILE *fp)
{
	fputc(x & 0xff, fp);
	fputc((x >> 8) & 0xff, fp);
}

/* write 32-bit long as little endian */
void fputl(ULONG x, FILE *fp)
{
	fputc(x & 0xff, fp);
	fputc((x >> 8) & 0xff, fp);
	fputc((x >> 16) & 0xff, fp);
	fputc((x >> 24) & 0xff, fp);
}


#ifdef MULTIMEDIA

/* File_Export_IsRecording simply returns true if any multimedia file is
   currently open and able to receive writes.

   Recording multiple files at the same time is not allowed. This is not a
   common use case, and not worth the additional code and user interface changes
   necessary to support this.

   RETURNS: TRUE is file is open, FALSE if it is not */
int File_Export_IsRecording(void)
{
	return container != NULL;
}

/* File_Export_StopRecording should be called when the program is exiting, or
   when all data required has been written to the file. This is needed because
   some containers need to update the header with information about the final
   size of the file, otherwise the container will not be valid.

   This will also be called automatically when a call is made to
   File_Export_StartRecording, or an error is encountered while writing audio or
   video.

   RETURNS: TRUE if file closed with no problems, FALSE if failure during close
   */
int File_Export_StopRecording(void)
{
	int result = TRUE;

	if (container) {
		result = container->close();
		container = NULL;
	}

	return result;
}

/* File_Export_StartRecording will open a new multimedia file, the type of which
   is determined by the filename. If an existing multimedia file is open, it will
   close that and start the new one.

   RETURNS: TRUE if file opened with no problems, FALSE if failure during open
   */
int File_Export_StartRecording(const char *filename)
{
	File_Export_StopRecording();

	CODECS_CONTAINER_Open(filename);

	return container != NULL;
}

#ifdef SOUND
/* Get the next filename in the sound_file_format pattern.
   RETURNS: True if filename is available, false if no filenames left in the pattern. */
int File_Export_GetNextSoundFile(char *buffer, int bufsize) {
	if (!sound_no_max) {
		sound_no_max = Util_filenamepattern(DEFAULT_SOUND_FILENAME_FORMAT, sound_filename_format, FILENAME_MAX, NULL);
	}
	return Util_findnextfilename(sound_filename_format, &sound_no_last, sound_no_max, buffer, bufsize, FALSE);
}

/* File_Export_WriteAudio will save audio data to the currently open container.
   The best way to do this for Atari800 is probably to call it directly after
   POKEYSND_Process(buffer, size) with the same values (buffer, size).

   If using video, there must be a call to File_Export_WriteAudio for each call
   to File_Export_WriteAudio, but the functions may be called in either order.

   RETURNS: Non-zero if no error; zero if error */
int File_Export_WriteAudio(const UBYTE *samples, int num_samples)
{
	int result = 0;

	if (container && audio_codec && container->save_audio) {
		result = container->save_audio(samples, num_samples);
	}

	return result;
}
#endif /* SOUND */

#ifdef AVI_VIDEO_RECORDING
/* Get the next filename in the video_file_format pattern.
   RETURNS: True if filename is available, false if no filenames left in the pattern. */
int File_Export_GetNextVideoFile(char *buffer, int bufsize) {
	if (!video_no_max) {
		video_no_max = Util_filenamepattern(DEFAULT_VIDEO_FILENAME_FORMAT, video_filename_format, FILENAME_MAX, NULL);
	}
	return Util_findnextfilename(video_filename_format, &video_no_last, video_no_max, buffer, bufsize, FALSE);
}

/* File_Export_WriteVideo will dump the Atari screen to the AVI file. A call to
   File_Export_WriteAudio must be called before calling File_Export_WriteVideo
   again, but the audio and video functions may be called in either order.

   RETURNS: the number of bytes written to the file (should be equivalent to the
   input uiSize parm) */
int File_Export_WriteVideo()
{
	int result = 0;

	if (container && video_codec && container->save_video) {
		result = container->save_video();
	}

	return result;
}

#endif /* AVI_VIDEO_RECORDING */

/* File_Export_GetStats gets the elapsed time in seconds and size in kilobytes of
   the currently recording file.

   RETURNS: TRUE if a file is currently being written, FALSE if not
   */
int File_Export_GetRecordingStats(int *seconds, int *size, char **media_type)
{
	if (container) {
		*seconds = (int)(video_frame_count / fps);
		*size = byteswritten;
		*media_type = description;
		return 1;
	}
	return 0;
}

#endif /* MULTIMEDIA */

#if !defined(BASIC) && !defined(CURSES_BASIC)

int File_Export_ImageTypeSupported(const char *id) {
	return CODECS_IMAGE_Init(id);
}

int File_Export_SaveScreen(const char *filename, UBYTE *ptr1, UBYTE *ptr2) {
	int result = 0;
	FILE *fp;

	CODECS_IMAGE_Init(filename);
	if (image_codec) {
		fp = fopen(filename, "wb");
		if (fp == NULL)
			return 0;
		result = image_codec->to_file(fp, ptr1, ptr2);
		fclose(fp);
	}
	return result;
}

#endif /* !defined(BASIC) && !defined(CURSES_BASIC) */

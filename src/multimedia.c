/*
 * multimedia.c - reading and writing multimedia files
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
#include "sound.h"
#include "pokeysnd.h"
#include "colours.h"
#include "multimedia.h"

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

/* sndoutput is just the file pointer for the current sound file */
static FILE *sndoutput = NULL;

#ifndef CURSES_BASIC
/* avioutput is just the file pointer for the current video file */
static FILE *avioutput = NULL;
#endif

/* number of bytes written to the currently open multimedia file */
static ULONG byteswritten;

/* Multimedia_IsFileOpen simply returns true if any multimedia file is currently
   open and able to receive writes.

   Recording both video and audio at the same time is not allowed. This is not
   a common use case, and not worth the additional code and user interface changes
   necessary to support this.

   RETURNS: TRUE is file is open, FALSE if it is not */

int Multimedia_IsFileOpen(void)
{
	return sndoutput != NULL || avioutput != NULL;
}


/* Multimedia_CloseFile should be called when the program is exiting, or
   when all data required has been written to the file.
   Multimedia_CloseFile will also be called automatically when a call is
   made to Multimedia_OpenSoundFile/Multimedia_OpenVideoFile, or an error is made in
   Multimedia_WriteToSoundFile. Note that both types of media files have to update
   the file headers with information regarding the length of the file.

   RETURNS: TRUE if file closed with no problems, FALSE if failure during close
   */

int Multimedia_CloseFile(void)
{
	int bSuccess = TRUE;
	char aligned = 0;

	if (sndoutput != NULL) {
		bSuccess = WAV_CloseFile(sndoutput, byteswritten);
		sndoutput = NULL;
	}
#ifndef CURSES_BASIC
	else if (avioutput != NULL) {
		bSuccess = AVI_CloseFile(avioutput);
		avioutput = NULL;
	}
#endif

	return bSuccess;
}


/* Multimedia_OpenSoundFile will start a new sound file and write out the
   header. If an existing sound file is already open it will be closed first,
   and the new file opened in its place.

   RETURNS: TRUE if file opened with no problems, FALSE if failure during open
   */

int Multimedia_OpenSoundFile(const char *szFileName)
{
	Multimedia_CloseFile();

	sndoutput = WAV_OpenFile(szFileName);

	return sndoutput != NULL;
}

/* Multimedia_WriteToSoundFile will dump PCM data to the WAV file. The best way
   to do this for Atari800 is probably to call it directly after
   POKEYSND_Process(buffer, size) with the same values (buffer, size)

   RETURNS: the number of bytes written to the file (should be equivalent to the
   input uiSize parm) */

int Multimedia_WriteToSoundFile(const unsigned char *ucBuffer, unsigned int uiSize)
{
	/* XXX FIXME: doesn't work with big-endian architectures */
	if (sndoutput && ucBuffer && uiSize) {
		int result;
		int sample_size;

		sample_size = POKEYSND_snd_flags & POKEYSND_BIT16? 2 : 1;
		result = WAV_WriteSamples(ucBuffer, uiSize, sample_size, sndoutput);
		if (result == 0) {
			Multimedia_CloseFile();
		}
		else {
			byteswritten += result;
		}

		return result;
	}

	return 0;
}

#ifndef CURSES_BASIC
/* Multimedia_OpenVideoFile will start a new video file and write out the
   header. If an existing video file is already open it will be closed first,
   and the new file opened in its place.

   RETURNS: TRUE if file opened with no problems, FALSE if failure during open
   */

int Multimedia_OpenVideoFile(const char *szFileName)
{
	Multimedia_CloseFile();

	avioutput = AVI_OpenFile(szFileName);

	return avioutput != NULL;
}
#endif

/* WAV_CloseFile must be called to create a valid WAV file, because the header
   at the beginning of the file must be modified to indicate the number of
   samples in the file.

   RETURNS: TRUE if file closed with no problems, FALSE if failure during close
   */

int WAV_CloseFile(FILE *fp, int num_bytes)
{
	int bSuccess = TRUE;
	char aligned = 0;

	if (fp != NULL) {
		/* A RIFF file's chunks must be word-aligned. So let's align. */
		if (num_bytes & 1) {
			if (putc(0, fp) == EOF)
				bSuccess = FALSE;
			else
				aligned = 1;
		}

		if (bSuccess) {
			/* Sound file is finished, so modify header and close it. */
			if (fseek(fp, 4, SEEK_SET) != 0)	/* Seek past RIFF */
				bSuccess = FALSE;
			else {
				/* RIFF header's size field must equal the size of all chunks
				 * with alignment, so the alignment byte is added.
				 */
				fputl(num_bytes + 36 + aligned, fp);
				if (fseek(fp, 40, SEEK_SET) != 0)
					bSuccess = FALSE;
				else {
					/* But in the "data" chunk size field, the alignment byte
					 * should be ignored. */
					fputl(num_bytes, fp);
				}
			}
		}
		fclose(fp);
	}

	return bSuccess;
}


/* WAV_OpenFile will start a new sound file and write out the header. Note that
   the file will not be valid until the it is closed with WAV_CloseFile because
   the length information contained in the header must be updated with the
   number of samples in the file.

   RETURNS: TRUE if file opened with no problems, FALSE if failure during open
   */

FILE *WAV_OpenFile(const char *szFileName)
{
	FILE *fp;

	if (!(fp = fopen(szFileName, "wb")))
		return NULL;
	/*
	The RIFF header:

	  Offset  Length   Contents
	  0       4 bytes  'RIFF'
	  4       4 bytes  <file length - 8>
	  8       4 bytes  'WAVE'

	The fmt chunk:

	  12      4 bytes  'fmt '
	  16      4 bytes  0x00000010     // Length of the fmt data (16 bytes)
	  20      2 bytes  0x0001         // Format tag: 1 = PCM
	  22      2 bytes  <channels>     // Channels: 1 = mono, 2 = stereo
	  24      4 bytes  <sample rate>  // Samples per second: e.g., 44100
	  28      4 bytes  <bytes/second> // sample rate * block align
	  32      2 bytes  <block align>  // channels * bits/sample / 8
	  34      2 bytes  <bits/sample>  // 8 or 16

	The data chunk:

	  36      4 bytes  'data'
	  40      4 bytes  <length of the data block>
	  44        bytes  <sample data>

	All chunks must be word-aligned.

	Good description of WAVE format: http://www.sonicspot.com/guide/wavefiles.html
	*/

	if (fwrite("RIFF\0\0\0\0WAVEfmt \x10\0\0\0\1\0", 1, 22, fp) != 22) {
		fclose(fp);
		return NULL;
	}

	fputc(POKEYSND_num_pokeys, fp);
	fputc(0, fp);
	fputl(POKEYSND_playback_freq, fp);


	fputl(POKEYSND_playback_freq * (POKEYSND_snd_flags & POKEYSND_BIT16 ? POKEYSND_num_pokeys << 1 : POKEYSND_num_pokeys), fp);

	fputc(POKEYSND_snd_flags & POKEYSND_BIT16 ? POKEYSND_num_pokeys << 1 : POKEYSND_num_pokeys, fp);
	fputc(0, fp);

	fputc(POKEYSND_snd_flags & POKEYSND_BIT16? 16: 8, fp);

	if (fwrite("\0data\0\0\0\0", 1, 9, fp) != 9) {
		fclose(fp);
		return NULL;
	}

	byteswritten = 0;
	return fp;
}

/* Multimedia_WriteToSoundFile will dump PCM data to the WAV file. The best way
   to do this for Atari800 is probably to call it directly after
   POKEYSND_Process(buffer, size) with the same values (buffer, size)

   RETURNS: the number of bytes written to the file (should be equivalent to the
   input num_samples * sample size) */

int WAV_WriteSamples(const unsigned char *buf, unsigned int num_samples, unsigned int sample_size, FILE *fp)
{
	/* XXX FIXME: doesn't work with big-endian architectures */
	if (fp && buf && num_samples && (sample_size == 1 || sample_size == 2)) {
		int result;

		result = fwrite(buf, sample_size, num_samples, fp);
		if (result != num_samples) {
			result = 0;
		}
		else {
			result *= sample_size;
		}

		return result;
	}

	return 0;
}

#ifndef CURSES_BASIC

/* AVI requires the header at the beginning of the file contains sizes of each
   chunk, so the header will be rewritten upon the close of the file to update
   the header values with the final totals. *;
*/
static ULONG size_riff;
static ULONG size_movi;

/* We also need to track some other variables for the header; these will be
   updated as frames are added to the video
*/

static ULONG num_frames;
static ULONG fps;
static ULONG num_samples_padded;

/* AVI_WriteHeader will start a new video file and write out the header. Note that
   the file will not be valid until the it is closed with AVI_CloseFile because
   the length information contained in the header must be updated with the
   number of samples in the file.

   RETURNS: TRUE if file opened with no problems, FALSE if failure during open
   */

static int AVI_WriteHeader(FILE *fp) {
	int i;

	fseek(fp, 0, SEEK_SET);
	fputs("RIFF", fp);
	fputl(size_riff, fp); /* length of entire file minus 8 bytes */
	fputs("AVI ", fp);

	/* write_avi_header_chunk */
	fputs("LIST", fp);
	fputl(12 + (56) + 8 + (12 + 56 + 8 + 40 + 256*4) + (8 + (12 + 56 + 8 + 18)), fp); /* length of header payload */

		fputs("hdrl", fp);
		fputs("avih", fp);
		fputl(56, fp); /* length of avih payload: 14 x 4 byte words */

			fputl(1000000 / fps, fp);
			fputl(336 * 240 * 3, fp); /* approximate bytes per second of video + audio FIXME: should likely be (width * height * 3 + audio) * fps */
			fputl(0, fp); /* reserved */
			fputl(0x10, fp); /* flags; 0x10 indicates the index at the end of the file */
			fputl(num_frames, fp); /* number of frames in the video */
			fputl(0, fp); /* initial frames, always zero for us */
			fputl(2, fp); /* 2 streams, both video and audio */
			fputl(336 * 240 * 3, fp); /* suggested buffer size */
			fputl(336, fp); /* video width */
			fputl(240, fp); /* video height */
			fputl(0, fp); /* reserved */
			fputl(0, fp);
			fputl(0, fp);
			fputl(0, fp);

		/* video stream format */
		fputs("LIST", fp);
		fputl(12 + 56 + 8 + 40 + 256*4, fp); /* length of strl + strh + strf */

			fputs("strl", fp);
			fputs("strh", fp);
			fputl(56, fp); /* length of strh payload: 14 x 4 byte words */

				fputs("vids", fp); /* video stream */
				fputs("MRLE", fp); /* Microsoft Run-Length Encoding format */
				fputl(0, fp); /* flags */
				fputw(0, fp); /* priority */
				fputw(0, fp); /* language */
				fputl(0, fp); /* initial_frames */
				fputl(1, fp); /* scale */
				fputl(fps, fp); /* rate */
				fputl(0, fp); /* start */
				fputl(num_frames, fp); /* length (i.e. number of frames) */
				fputl(336 * 240 * 3, fp); /* suggested buffer size */
				fputl(0, fp); /* quality */
				fputl(0, fp); /* sample size (0 = variable sample size) */
				fputl(0, fp); /* rcRect, ignored */
				fputl(0, fp);

			fputs("strf", fp);
			fputl(40 + 256*4, fp); /* length of header + palette info */

				fputl(40, fp); /* header_size */
				fputl(336, fp); /* width */
				fputl(240, fp); /* height */
				fputw(1, fp); /* number of bitplanes */
				fputw(8, fp); /* bits per pixel: 8 = paletted */
				fputs("MRLE", fp); /* compression_type */
				fputl(336 * 240 * 3, fp); /* image_size */
				fputl(0, fp); /* x pixels per meter (!) */
				fputl(0, fp); /* y pikels per meter */
				fputl(256, fp); /* colors_used */
				fputl(0, fp); /* colors_important (0 = all are important) */

				for (i = 0; i < 256; i++) {
					fputc(Colours_GetR(i), fp);
					fputc(Colours_GetG(i), fp);
					fputc(Colours_GetB(i), fp);
					fputc(0, fp);
				}

		/* audio stream format */
		fputs("LIST", fp);
		fputl(12 + 56 + 8 + 18, fp); /* length of payload: strl + strh + strf */

			fputs("strl", fp);
			fputs("strh", fp);
			fputl(56, fp); /* length of strh payload: 14 x 4 byte words */

				fputs("auds", fp); /* video stream */
				fputl(1, fp); /* 1 = uncompressed audio */
				fputl(0, fp); /* flags */
				fputw(0, fp); /* priority */
				fputw(0, fp); /* language */
				fputl(0, fp); /* initial_frames */
				fputl(1, fp); /* scale */
				fputl(Sound_out.freq, fp); /* rate */
				fputl(0, fp); /* start */
				fputl(num_samples_padded, fp); /* length (i.e. number of frames) */
				fputl(Sound_out.freq * Sound_out.channels * Sound_out.sample_size, fp); /* suggested buffer size */
				fputl(0, fp); /* quality (-1 = default quality?) */
				fputl(Sound_out.channels * Sound_out.sample_size, fp); /* sample size */
				fputl(0, fp); /* rcRect, ignored */
				fputl(0, fp);

			fputs("strf", fp);
			fputl(18, fp); /* length of header */

				fputw(1, fp); /* format_type */
				fputw(Sound_out.channels, fp); /* channels */
				fputl(Sound_out.freq, fp); /* sample_rate */
				fputl(Sound_out.freq * Sound_out.channels * Sound_out.sample_size, fp); /* bytes_per_second */
				fputw(Sound_out.channels * Sound_out.sample_size, fp); /* block_align */
				fputw(Sound_out.sample_size * 8, fp); /* bits_per_sample */
				fputw(0, fp); /* size */

	/* start of video & audio chunks */
	fputs("LIST", fp);
	fputl(size_movi, fp); /* length of all video and audio chunks */
	fputs("movi", fp);
}

/* AVI_OpenFile will start a new video file and write out the header. Note that
   the file will not be valid until the it is closed with AVI_CloseFile because
   the length information contained in the header must be updated with the
   number of samples in the file.

   RETURNS: TRUE if file opened with no problems, FALSE if failure during open
   */

FILE *AVI_OpenFile(const char *szFileName)
{
	FILE *fp;

	if (!(fp = fopen(szFileName, "wb")))
		return NULL;

	size_riff = 0;
	size_movi = 0;
	num_frames = 0;
	fps = Atari800_tv_mode == Atari800_TV_PAL ? 50 : 60;
	num_samples_padded = 0;
	AVI_WriteHeader(fp);

	return fp;
}

/* AVI_CloseFile must be called to create a valid AVI file, because the header
   at the beginning of the file must be modified to indicate the number of
   samples in the file.

   RETURNS: TRUE if file closed with no problems, FALSE if failure during close
   */

int AVI_CloseFile(FILE *fp)
{
	AVI_WriteHeader(fp);
	fclose(fp);
}
#endif

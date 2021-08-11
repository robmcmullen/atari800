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
#include <stdlib.h>
#include "pokeysnd.h"
#include "multimedia.h"
#ifdef AVI_VIDEO_RECORDING
#include "screen.h"
#include "colours.h"
#include "util.h"
#include "log.h"
#endif

/* sndoutput is just the file pointer for the current sound file */
static FILE *sndoutput = NULL;

/* number of bytes written to the currently open multimedia file */
static ULONG byteswritten;

/* sample size in bytes; will not change during a recording */
static int sample_size;

#ifdef AVI_VIDEO_RECORDING
#define ATARI_VISIBLE_WIDTH 336
#define ATARI_LEFT_MARGIN 24

/* avioutput is just the file pointer for the current video file */
static FILE *avioutput = NULL;

/* AVI requires the header at the beginning of the file contains sizes of each
   chunk, so the header will be rewritten upon the close of the file to update
   the header values with the final totals. *;
*/
static ULONG size_riff;
static ULONG size_movi;

/* AVI files using the version 1.0 indexes ('idx1') have a 32 bit limit, which
   limits file size to 4GB. Some media players may fail to play videos greater
   than 2GB because of their incorrect use of signed rather than unsigned 32 bit
   values.

   The maximum recording duration depends mostly on the complexity of the video.
   The audio is saved as raw PCM samples which doesn't vary per frame. On NTSC
   at 60Hz using 16 bit samples, this will be just under 1500 bytes per frame.

   The size of each encoded video frame depends on the complexity the screen
   image. The RLE compression is based on scan lines, and performs best when
   neighboring pixels on the scan line are the same color. Due to overhead in
   the compression sceme itself, the best it can do is about 1500 bytes on a
   completely black screen. Complex screens where many neighboring pixels have
   different colors result in video frames of around 30k. This is still a
   significant savings over an uncompressed frame which would be 80k.

   For complex scenes, therefore, this results in about 8 minutes of video
   recording per GB, or about 36 minutes when using the full 4GB file size. Less
   complex video will provide more recording time. For example, recording the
   unchanging BASIC prompt screen would result in about 6 hours of video.

   The video will automatically be stopped should the recording length approach
   the file size limit. */
#define MAX_RECORDING_SIZE (0xfff00000)
static ULONG size_limit;
static ULONG total_video_size;
static ULONG smallest_video_frame;
static ULONG largest_video_frame;

/* We also need to track some other variables for the header; these will be
   updated as frames are added to the video. */
static ULONG frames_written;
static float fps;
static ULONG samples_written;

/* Some data must be dynamically allocated for file creation */
#define FRAME_INDEX_ALLOC_SIZE 1000
static ULONG *frame_indexes;
static int num_frames_allocated;
static int rle_buffer_size;
static UBYTE *rle_buffer = NULL;
static int current_screen_size;
static int audio_buffer_size;
static UBYTE *audio_buffer = NULL;
static int current_audio_samples;

#endif /* AVI_VIDEO_RECORDING */

/* write 32-bit long as little endian */
void fputl(ULONG x, FILE *fp)
{
	fputc(x & 0xff, fp);
	fputc((x >> 8) & 0xff, fp);
	fputc((x >> 16) & 0xff, fp);
	fputc((x >> 24) & 0xff, fp);
}


/* write 16-bit word as little endian */
void fputw(UWORD x, FILE *fp)
{
	fputc(x & 0xff, fp);
	fputc((x >> 8) & 0xff, fp);
}

/* fwritele mimics fwrite but writes data in little endian format if operating
   on a big endian platform.

   On a little endian platform, this function calls fwrite with no alteration to
   the data.

   On big endian platforms, the only valid size parameters are 1 and 2; size of
   1 indicates byte data, which has no endianness and will be written unaltered.
   If the size parameter is 2 (indicating WORD or UWORD 16-bit data), this
   function will reverse bytes in the file output.

   Note that size parameters greater than 2 (e.g. 4 which would indicate LONG &
   ULONG data) are not currently supported on big endian platforms because
   nothing currently use arrays of that size.

   RETURNS: number of elements written, or zero if error on big endial platforms */
size_t fwritele(const void *ptr, size_t size, size_t nmemb, FILE *fp)
{
#ifdef WORDS_BIGENDIAN
	size_t count;
	UBYTE *source;
	UBYTE c;

	if (size == 2) {
		/* fputc doesn't return useful error info as a return value, so simulate
		   the fwrite error condition by checking ferror before and after, and
		   if no error, return the number of elements written. */
		if (ferror(fp)) {
			return 0;
		}
		source = (UBYTE *)ptr;

		/* Instead of using this simple loop over fputc, a faster algorithm
		   could be written by copying the data into a temporary array, swapping
		   bytes there, and using fwrite. However, fputc may be cached behind
		   the scenes and this is likely to be fast enough for most platforms. */
		for (count = 0; count < nmemb; count++) {
			c = *source++;
			fputc(*source++, fp);
			fputc(c, fp);
		}
		if (ferror(fp)) {
			return 0;
		}
		return count;
	}
#endif
	return fwrite(ptr, size, nmemb, fp);
}

/* Multimedia_IsFileOpen simply returns true if any multimedia file is currently
   open and able to receive writes.

   Recording both video and audio at the same time is not allowed. This is not
   a common use case, and not worth the additional code and user interface changes
   necessary to support this.

   RETURNS: TRUE is file is open, FALSE if it is not */
int Multimedia_IsFileOpen(void)
{
	return sndoutput != NULL
#ifdef AVI_VIDEO_RECORDING
		|| avioutput != NULL
#endif
	;
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

	if (sndoutput != NULL) {
		bSuccess = WAV_CloseFile(sndoutput, byteswritten);
		sndoutput = NULL;
	}
#ifdef AVI_VIDEO_RECORDING
	if (avioutput != NULL) {
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

/* Multimedia_WriteAudio will dump PCM data to the WAV file. The best way to do
   this for Atari800 is probably to call it directly after
   POKEYSND_Process(buffer, size) with the same values (buffer, size).

   If using video, a call to Multimedia_WriteAudio must be called before calling
   Multimedia_WriteAudio again, but the audio and video functions may be called
   in either order.

   RETURNS: the number of bytes written to the file (should be equivalent to the
   input uiSize parm) */
int Multimedia_WriteAudio(const unsigned char *ucBuffer, unsigned int uiSize)
{
	if (ucBuffer && uiSize) {
		int result;

		if (sndoutput) {
			result = WAV_WriteSamples(ucBuffer, uiSize, sndoutput);
			if (result == 0) {
				Multimedia_CloseFile();
			}
			else {
				byteswritten += result;
			}

			return result;
		}
#ifdef AVI_VIDEO_RECORDING
		else if (avioutput) {
			result = AVI_AddAudioSamples(ucBuffer, uiSize, avioutput);
			if (result == 0) {
				Multimedia_CloseFile();
			}
		}
#endif
	}

	return 0;
}

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
	sample_size = POKEYSND_snd_flags & POKEYSND_BIT16? 2 : 1;

	fputs("RIFF", fp);
	fputl(0, fp); /* length to be filled in upon file close */
	fputs("WAVE", fp);

	fputs("fmt ", fp);
	fputl(16, fp);
	fputw(1, fp);
	fputw(POKEYSND_num_pokeys, fp);
	fputl(POKEYSND_playback_freq, fp);
	fputl(POKEYSND_playback_freq * sample_size, fp);
	fputw(POKEYSND_num_pokeys * sample_size, fp);
	fputw(sample_size * 8, fp);

	fputs("data", fp);
	fputl(0, fp); /* length to be filled in upon file close */

	if (ftell(fp) != 44) {
		fclose(fp);
		return NULL;
	}

	byteswritten = 0;
	return fp;
}

/* WAV_WriteSamples will dump PCM data to the WAV file. The best way
   to do this for Atari800 is probably to call it directly after
   POKEYSND_Process(buffer, size) with the same values (buffer, size)

   RETURNS: the number of bytes written to the file (should be equivalent to the
   input num_samples * sample size) */
int WAV_WriteSamples(const unsigned char *buf, unsigned int num_samples, FILE *fp)
{
	if (fp && buf && num_samples) {
		int result;

		result = fwritele(buf, sample_size, num_samples, fp);
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

#ifdef AVI_VIDEO_RECORDING

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

/* Multimedia_WriteVideo will dump the Atari screen to the AVI file. A call to
   Multimedia_WriteAudio must be called before calling Multimedia_WriteVideo
   again, but the audio and video functions may be called in either order.

   RETURNS: the number of bytes written to the file (should be equivalent to the
   input uiSize parm) */
int Multimedia_WriteVideo()
{
	if (avioutput) {
		int result;

		result = AVI_AddVideoFrame(avioutput);
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

/* AVI_WriteHeader creates and writes out the file header. Note that this
   function will have to be called again just prior to closing the file in order
   to re-write the header with updated size values that are only known after all
   data has been written.

   RETURNS: TRUE if header was written successfully, FALSE if not
   */
static int AVI_WriteHeader(FILE *fp) {
	int i;
	int list_size;

	fseek(fp, 0, SEEK_SET);

	/* RIFF AVI header */
	fputs("RIFF", fp);
	fputl(size_riff, fp); /* length of entire file minus 8 bytes */
	fputs("AVI ", fp);

	/* hdrl LIST. Payload size includes the 4 bytes of the 'hdrl' identifier. */
	fputs("LIST", fp);
	list_size = 4 + 8 + 56 /* hdrl identifier plus avih size */
		+ 12 + (8 + 56 + 8 + 40 + 256*4 + 8 + 16) /* video stream strl header LIST + (strh + strf + strn) */
		+ 12 + (8 + 56 + 8 + 18 + 8 + 12); /* audio stream strl header LIST + (strh + strf + strn) */
	fputl(list_size, fp); /* length of header payload */
	fputs("hdrl", fp);

	/* Main header is documented at https://docs.microsoft.com/en-us/previous-versions/windows/desktop/api/Aviriff/ns-aviriff-avimainheader */

	/* 8 bytes */
	fputs("avih", fp);
	fputl(56, fp); /* length of avih payload: 14 x 4 byte words */

	/* 56 bytes */
	fputl((ULONG)(1000000 / fps), fp); /* microseconds per frame */
	fputl(ATARI_VISIBLE_WIDTH * Screen_HEIGHT * 3, fp); /* approximate bytes per second of video + audio FIXME: should likely be (width * height * 3 + audio) * fps */
	fputl(0, fp); /* reserved */
	fputl(0x10, fp); /* flags; 0x10 indicates the index at the end of the file */
	fputl(frames_written, fp); /* number of frames in the video */
	fputl(0, fp); /* initial frames, always zero for us */
	fputl(2, fp); /* 2 streams, both video and audio */
	fputl(ATARI_VISIBLE_WIDTH * Screen_HEIGHT * 3, fp); /* suggested buffer size */
	fputl(ATARI_VISIBLE_WIDTH, fp); /* video width */
	fputl(Screen_HEIGHT, fp); /* video height */
	fputl(0, fp); /* reserved */
	fputl(0, fp);
	fputl(0, fp);
	fputl(0, fp);

	/* video stream format */

	/* 12 bytes for video stream strl LIST chuck header; LIST payload size includes the
	   4 bytes of the 'strl' identifier plus the strh + strf + strn sizes */
	fputs("LIST", fp);
	fputl(4 + 8 + 56 + 8 + 40 + 256*4 + 8 + 16, fp);
	fputs("strl", fp);

	/* Stream header format is document at https://docs.microsoft.com/en-us/previous-versions/windows/desktop/api/avifmt/ns-avifmt-avistreamheader */

	/* 8 bytes for stream header indicator */
	fputs("strh", fp);
	fputl(56, fp); /* length of strh payload: 14 x 4 byte words */

	/* 56 bytes for stream header data */
	fputs("vids", fp); /* video stream */
	fputs("mrle", fp); /* Microsoft Run-Length Encoding format */
	fputl(0, fp); /* flags */
	fputw(0, fp); /* priority */
	fputw(0, fp); /* language */
	fputl(0, fp); /* initial_frames */
	fputl(1000000, fp); /* scale */
	fputl((ULONG)(fps * 1000000), fp); /* rate = frames per second / scale */
	fputl(0, fp); /* start */
	fputl(frames_written, fp); /* length (for video is number of frames) */
	fputl(ATARI_VISIBLE_WIDTH * Screen_HEIGHT * 3, fp); /* suggested buffer size */
	fputl(0, fp); /* quality */
	fputl(0, fp); /* sample size (0 = variable sample size) */
	fputl(0, fp); /* rcRect, ignored */
	fputl(0, fp);

	/* 8 bytes for stream format indicator */
	fputs("strf", fp);
	fputl(40 + 256*4, fp); /* length of header + palette info */

	/* 40 bytes for stream format data */
	fputl(40, fp); /* header_size */
	fputl(ATARI_VISIBLE_WIDTH, fp); /* width */
	fputl(Screen_HEIGHT, fp); /* height */
	fputw(1, fp); /* number of bitplanes */
	fputw(8, fp); /* bits per pixel: 8 = paletted */
	fputl(1, fp); /* compression_type */
	fputl(ATARI_VISIBLE_WIDTH * Screen_HEIGHT * 3, fp); /* image_size */
	fputl(0, fp); /* x pixels per meter (!) */
	fputl(0, fp); /* y pikels per meter */
	fputl(256, fp); /* colors_used */
	fputl(0, fp); /* colors_important (0 = all are important) */

	/* 256 * 4 = 1024 bytes of palette in ARGB little-endian order */
	for (i = 0; i < 256; i++) {
		fputc(Colours_GetB(i), fp);
		fputc(Colours_GetG(i), fp);
		fputc(Colours_GetR(i), fp);
		fputc(0, fp);
	}

	/* 8 bytes for stream name indicator */
	fputs("strn", fp);
	fputl(16, fp); /* length of name */

	/* 16 bytes for name, zero terminated and padded with a zero */

	/* Note: everything in RIFF files must be word-aligned, so padding with a
	   zero is necessary if the length of the name plus the null terminator is
	   an odd value */
	fputs("atari800 video", fp);
	fputc(0, fp); /* null terminator */
	fputc(0, fp); /* padding to get to 16 bytes */

	/* audio stream format */

	/* 12 bytes for audio stream strl LIST chuck header; LIST payload size includes the
	   4 bytes of the 'strl' identifier plus the strh + strf + strn sizes */
	fputs("LIST", fp);
	fputl(4 + 8 + 56 + 8 + 18 + 8 + 12, fp);
	fputs("strl", fp);

	/* stream header format is same as video above even when used for audio */

	/* 8 bytes for stream header indicator */
	fputs("strh", fp);
	fputl(56, fp); /* length of strh payload: 14 x 4 byte words */

	/* 56 bytes for stream header data */
	fputs("auds", fp); /* video stream */
	fputl(1, fp); /* 1 = uncompressed audio */
	fputl(0, fp); /* flags */
	fputw(0, fp); /* priority */
	fputw(0, fp); /* language */
	fputl(0, fp); /* initial_frames */
	fputl(1, fp); /* scale */
	fputl(POKEYSND_playback_freq, fp); /* rate, i.e. samples per second */
	fputl(0, fp); /* start time; zero = no delay */
	fputl(samples_written, fp); /* length (for audio is number of samples) */
	fputl(POKEYSND_playback_freq * POKEYSND_num_pokeys * sample_size, fp); /* suggested buffer size */
	fputl(0, fp); /* quality (-1 = default quality?) */
	fputl(POKEYSND_num_pokeys * sample_size, fp); /* sample size */
	fputl(0, fp); /* rcRect, ignored */
	fputl(0, fp);

	/* 8 bytes for stream format indicator */
	fputs("strf", fp);
	fputl(18, fp); /* length of header */

	/* 18 bytes for stream format data */
	fputw(1, fp); /* format_type */
	fputw(POKEYSND_num_pokeys, fp); /* channels */
	fputl(POKEYSND_playback_freq, fp); /* sample_rate */
	fputl(POKEYSND_playback_freq * POKEYSND_num_pokeys * sample_size, fp); /* bytes_per_second */
	fputw(POKEYSND_num_pokeys * sample_size, fp); /* bytes per frame */
	fputw(sample_size * 8, fp); /* bits_per_sample */
	fputw(0, fp); /* size */

	/* 8 bytes for stream name indicator */
	fputs("strn", fp);
	fputl(12, fp); /* length of name */

	/* 12 bytes for name, zero terminated */
	fputs("POKEY audio", fp);
	fputc(0, fp); /* null terminator */

	/* audia/video data */

	/* 8 bytes for audio/video stream LIST chuck header; LIST payload is the
	  'movi' chunk which in turn contains 00dc and 01wb chunks representing a
	  frame of video and the corresponding audio. */
	fputs("LIST", fp);
	fputl(size_movi, fp); /* length of all video and audio chunks */
	size_movi = ftell(fp); /* start of movi payload, will finalize after all chunks written */
	fputs("movi", fp);

	return (ftell(fp) == 12 + 8 + list_size + 12);
}

/* AVI_OpenFile will start a new video file and write out an initial copy of the
   header. Note that the file will not be valid until the it is closed with
   AVI_CloseFile because the length information contained in the header must be
   updated with the number of samples in the file.

   RETURNS: file pointer if successful, NULL if failure during open
   */
FILE *AVI_OpenFile(const char *szFileName)
{
	FILE *fp;

	if (!(fp = fopen(szFileName, "wb")))
		return NULL;

	size_riff = 0;
	size_movi = 0;
	frames_written = 0;
	samples_written = 0;
	current_screen_size = 0;
	current_audio_samples = 0;

	fps = Atari800_tv_mode == Atari800_TV_PAL ? Atari800_FPS_PAL : Atari800_FPS_NTSC;
	sample_size = POKEYSND_snd_flags & POKEYSND_BIT16? 2 : 1;

	num_frames_allocated = FRAME_INDEX_ALLOC_SIZE;
	frame_indexes = (ULONG *)Util_malloc(num_frames_allocated * sizeof(ULONG));
	memset(frame_indexes, 0, num_frames_allocated * sizeof(ULONG));

	rle_buffer_size = ATARI_VISIBLE_WIDTH * Screen_HEIGHT;
	rle_buffer = (UBYTE *)Util_malloc(rle_buffer_size);

	audio_buffer_size = (int)(POKEYSND_playback_freq * POKEYSND_num_pokeys * sample_size / fps) + 1024;
	audio_buffer = (UBYTE *)Util_malloc(audio_buffer_size);

	if (!AVI_WriteHeader(fp)) {
		fclose(fp);
		return NULL;
	}

	/* set up video statistics */
	size_limit = ftell(fp) + 8; /* current size + index header */
	total_video_size = 0;
	smallest_video_frame = 0xffffffff;
	largest_video_frame = 0;

	return fp;
}

/* AVI_WriteFrame writes out a single frame of video and audio, and saves the
   index data for the end-of-file index chunk */
static int AVI_WriteFrame(FILE *fp) {
	int audio_size;
	int video_padding;
	int audio_padding;
	int frame_size;
	int result;

	frame_size = ftell(fp);

	/* AVI chunks must be word-aligned, i.e. lengths must be multiples of 2 bytes.
	   If the size is an odd number, the data is padded with a zero but the length
	   value still reports the actual length, not the padded length */
	video_padding = current_screen_size % 2;
	fputs("00dc", fp);
	fputl(current_screen_size, fp);
	fwrite(rle_buffer, 1, current_screen_size, fp);
	if (video_padding) {
		fputc(0, fp);
	}

	audio_size = current_audio_samples * sample_size;
	audio_padding = audio_size % 2;
	fputs("01wb", fp);
	fputl(audio_size, fp);
	fwritele(audio_buffer, sample_size, current_audio_samples, fp);
	if (audio_padding) {
		fputc(0, fp);
	}

	frame_indexes[frames_written] = current_screen_size * 0x10000 + audio_size;
	samples_written += current_audio_samples;
	frames_written++;
	if (frames_written >= num_frames_allocated) {
		num_frames_allocated += FRAME_INDEX_ALLOC_SIZE;
		frame_indexes = (ULONG *)Util_realloc(frame_indexes, num_frames_allocated * sizeof(ULONG));
	}

	/* check expected file data written equals the calculated size */
	frame_size = ftell(fp) - frame_size;
	result = (frame_size == 8 + current_screen_size + video_padding + 8 + audio_size + audio_padding);

	/* update size limit calculation including the 32 bytes needed for each index entry */
	size_limit += frame_size + 32;

	/* update statistics */
	total_video_size += current_screen_size;
	if (current_screen_size < smallest_video_frame) {
		smallest_video_frame = current_screen_size;
	}
	if (current_screen_size > largest_video_frame) {
		largest_video_frame = current_screen_size;
	}

	/* reset size indicators for next frame */
	current_screen_size = 0;
	current_audio_samples = 0;

	if (size_limit > MAX_RECORDING_SIZE) {
		/* force file close when at the limit */
		return 0;
	}

	return result;
}

/* Microsoft Run Length Encoding codec is the simplest codec that I could find
   that is supported on multiple platforms. It compresses *much* better than raw
   video, and is supported by ffmpeg and ffmpeg-based players (like vlc and
   mpv), as well as proprietary applications like Windows Media Player. See
   https://wiki.multimedia.cx/index.php?title=Microsoft_RLE for a description of
   the format. */
static int MRLE_CompressLine(UBYTE *buf, const UBYTE *ptr) {
	int x;
	int size;
	UBYTE last;
	UBYTE count;

	x = 0;
	size = 0;
	do {
		last = *ptr;
		count = 0;
		do {
			ptr++;
			count++;
			x++;
		} while (last == *ptr && x < ATARI_VISIBLE_WIDTH && count < 255);
		*buf++ = count;
		*buf++ = last;
		size += 2;
	} while (x < ATARI_VISIBLE_WIDTH);
	return size;
}

/* MRLE_CreateFrame fills the output buffer with the fourcc type 'mrle' run
   length encoding data using the paletted data of the Atari screen.

   RETURNS: number of encoded bytes, or -1 if buffer not large enough to hold
   encoded data */
int MRLE_CreateFrame(UBYTE *buf, int bufsize, const UBYTE *source) {
	UBYTE *buf_start;
	const UBYTE *ptr;
	int y;
	int size;

	buf_start = buf;

	/* MRLE codec requires image origin at bottom left, so start saving at last scan
	   line and work back to the zeroth scan line. */

	for (y = Screen_HEIGHT-1; y >= 0; y--) {
		ptr = source + (y * Screen_WIDTH) + ATARI_LEFT_MARGIN;

		/* worst case for RLE compression is where no pixel has the same color
		   as its neighbor,  resulting in twice the number of bytes as pixels.
		   Each line needs the end of line marker, plus possibly the end of
		   bitmap marker. If buffer size remaining is less than this, it's too
		   small. */
		if (bufsize < (Screen_WIDTH * 2 + 2 + 2)) {
			printf("AVI write error: video compression buffer size too small.\n");
			return -1;
		}
		size = MRLE_CompressLine(buf, ptr);
		buf += size;
		bufsize -= size + 2;

		/* mark end of line */
		*buf++ = 0;
		*buf++ = 0;
	}

	/* mark end of bitmap */
	*buf++ = 0;
	*buf++ = 1;

	return buf - buf_start;
}

/* AVI_AddVideoFrame adds a video frame to the stream. If an existing video
   frame & audio data exist, save it to the file before starting a new frame.
   Note that AVI_AddVideoFrame and AVI_AddAudioSamples may be called in either
   order, but you must call both video and audio functions before the same
   function again. */
int AVI_AddVideoFrame(FILE *fp) {
	if (current_screen_size > 0) {
		if (current_audio_samples > 0) {
			if (!AVI_WriteFrame(fp)) {
				return 0;
			}
		}
		else {
			printf("AVI write error: attempted to write video frame without audio data\n");
			return 0;
		}
	}
	else if (current_screen_size < 0 || current_audio_samples < 0) {
		/* error condition; force close of file */
		return 0;
	}

	current_screen_size = MRLE_CreateFrame(rle_buffer, rle_buffer_size, (const UBYTE *)Screen_atari);
	return current_screen_size > 0;
}

/* AVI_AddAudioSamples adds audio data to the stream for the current video
   frame. If an existing video frame & audio data exist, save it to the file
   before starting a new frame. Note that AVI_AddVideoFrame and
   AVI_AddAudioSamples may be called in either order, but you must call both
   video and audio functions before the same function again. */
int AVI_AddAudioSamples(const UBYTE *buf, int num_samples, FILE *fp) {
	int size;

	if (current_audio_samples > 0) {
		if (current_screen_size > 0) {
			if (!AVI_WriteFrame(fp)) {
				return 0;
			}
		}
		else {
			printf("AVI write error: attempted to write audio data without video frame\n");
			return 0;
		}
	}
	else if (current_screen_size < 0 || current_audio_samples < 0) {
		/* error condition; force close of file */
		return 0;
	}

	size = num_samples * sample_size;
	if (size > audio_buffer_size) {
		printf("AVI write error: audio buffer size too small to hold %d samples\n", num_samples);
		/* set error condition */
		current_audio_samples = -1;
		return 0;
	}
	current_audio_samples = num_samples;
	memcpy(audio_buffer, buf, size);

	return 1;
}

static int AVI_WriteIndex(FILE *fp) {
	int i;
	int offset;
	int size;
	int index_size;
	int bytes_written;

	if (frames_written == 0) return 0;

	bytes_written = ftell(fp);
	offset = 4;
	index_size = frames_written * 32;

	/* The index format used here is tag 'idx1" (index version 1.0) & documented at
	https://docs.microsoft.com/en-us/previous-versions/windows/desktop/api/Aviriff/ns-aviriff-avioldindex
	*/

	fputs("idx1", fp);
	fputl(index_size, fp);

	for (i = 0; i < frames_written; i++) {
		fputs("00dc", fp); /* stream 0, a compressed video frame */
		fputl(0x10, fp); /* flags: is a keyframe */
		fputl(offset, fp); /* offset in bytes from start of the 'movi' list */
		size = (frame_indexes[i] & 0xffff0000) / 0x10000;
		fputl(size, fp); /* size of video frame */
		offset += size + 8;

		fputs("01wb", fp); /* stream 1, audio data */
		fputl(0, fp); /* flags: audio is not a keyframe */
		fputl(offset, fp); /* offset in bytes from start of the 'movi' list */
		size = frame_indexes[i] & 0xffff;
		fputl(size, fp); /* size of audio data */
		offset += size + 8;
	}

	bytes_written = ftell(fp) - bytes_written;
	return (bytes_written == 8 + index_size);
}

/* AVI_CloseFile must be called to create a valid AVI file, because the header
   at the beginning of the file must be modified to indicate the number of
   samples in the file.

   RETURNS: TRUE if file closed with no problems, FALSE if failure during close
   */
int AVI_CloseFile(FILE *fp)
{
	int seconds;
	int result;

	/* write out final frame if one exists */
	if (current_screen_size > 0 && current_audio_samples > 0) {
		result = AVI_WriteFrame(fp);
	}
	else {
		result = 1;
	}

	if (frames_written > 0) {
		seconds = (int)(frames_written / fps);
		Log_print("AVI stats: %d:%02d:%02d, %dMB, %d frames; video codec avg frame size %.1fkB, min=%.1fkB, max=%.1fkB", seconds / 60 / 60, (seconds / 60) % 60, seconds % 60, size_limit / 1024 / 1024, frames_written, total_video_size / frames_written / 1024.0, smallest_video_frame / 1024.0, largest_video_frame / 1024.0);
	}

	if (result > 0) {
		size_movi = ftell(fp) - size_movi; /* movi payload ends here */
		result = AVI_WriteIndex(fp);
	}
	if (result > 0) {
		size_riff = ftell(fp) - 8;
		result = AVI_WriteHeader(fp);
	}
	fclose(fp);
	free(audio_buffer);
	audio_buffer = NULL;
	audio_buffer_size = 0;
	free(rle_buffer);
	rle_buffer = NULL;
	rle_buffer_size = 0;
	free(frame_indexes);
	frame_indexes = NULL;
	num_frames_allocated = 0;
	return result;
}
#endif /* AVI_VIDEO_RECORDING */

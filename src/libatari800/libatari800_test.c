#include <stdio.h>
#include <string.h>

#include "screen.h"
#include "colours.h"
#include "libatari800.h"
#include "gwavi.h"
#include "multimedia.h"


#define ATARI_VISIBLE_WIDTH 336
#define ATARI_LEFT_MARGIN 24

static void debug_screen()
{
	/* print out portion of screen, assuming graphics 0 display list */
	unsigned char *screen = libatari800_get_screen_ptr();
	int x, y;

	screen += (384 * 24) + ATARI_LEFT_MARGIN;
	for (y = 0; y < 32; y++) {
		for (x = 8; x < 88; x++) {
			unsigned char c = screen[x];
			if (c == 0)
				printf(" ");
			else if (c == 0x94)
				printf(".");
			else if (c == 0x9a)
				printf("X");
			else
				printf("?");
		}
		printf("\n");
		screen += 384;
	}
}


/* This wav writing code is from sndsave.c, repurposed here because the sndsave.c functions
   are tied into the normal emulator function of saving audio files. It uses the POKEY audio
   flags rather than the libatari800 flags, so I wanted to prove the libatari800 audio
   was actually being used correctly and had to copy the code here. As this is just a test
   program, it's not code duplication in the strictest sense, as it isn't included in the
   libatari800.a library. */

/* wavfile is just the file pointer for the current sound file */
static FILE *wavfile = NULL;

static ULONG byteswritten;


int wavCloseSoundFile(void)
{
	int bSuccess = TRUE;
	char aligned = 0;

	if (wavfile != NULL) {
		/* A RIFF file's chunks must be word-aligned. So let's align. */
		if (byteswritten & 1) {
			if (putc(0, wavfile) == EOF)
				bSuccess = FALSE;
			else
				aligned = 1;
		}

		if (bSuccess) {
			/* Sound file is finished, so modify header and close it. */
			if (fseek(wavfile, 4, SEEK_SET) != 0)	/* Seek past RIFF */
				bSuccess = FALSE;
			else {
				/* RIFF header's size field must equal the size of all chunks
				 * with alignment, so the alignment byte is added.
				 */
				fputl(byteswritten + 36 + aligned, wavfile);
				if (fseek(wavfile, 40, SEEK_SET) != 0)
					bSuccess = FALSE;
				else {
					/* But in the "data" chunk size field, the alignment byte
					 * should be ignored. */
					fputl(byteswritten, wavfile);
				}
			}
		}
		fclose(wavfile);
		wavfile = NULL;
	}

	return bSuccess;
}

int wavOpenSoundFile(const char *szFileName)
{
	wavCloseSoundFile();

	wavfile = fopen(szFileName, "wb");

	if (wavfile == NULL)
		return FALSE;

	if (fwrite("RIFF\0\0\0\0WAVEfmt \x10\0\0\0\1\0", 1, 22, wavfile) != 22) {
		fclose(wavfile);
		wavfile = NULL;
		return FALSE;
	}

	fputc(libatari800_get_num_sound_channels(), wavfile);
	fputc(0, wavfile);
	fputl(libatari800_get_sound_frequency(), wavfile);

	fputl(libatari800_get_sound_frequency() * libatari800_get_num_sound_channels() * libatari800_get_sound_sample_size(), wavfile);

	fputc(libatari800_get_num_sound_channels() * libatari800_get_sound_sample_size(), wavfile);
	fputc(0, wavfile);

	fputc(libatari800_get_sound_sample_size() * 8, wavfile);

	if (fwrite("\0data\0\0\0\0", 1, 9, wavfile) != 9) {
		fclose(wavfile);
		wavfile = NULL;
		return FALSE;
	}

	byteswritten = 0;
	return TRUE;
}

int wavWriteToSoundFile()
{
	/* XXX FIXME: doesn't work with big-endian architectures */
	if (wavfile) {
		int result;
		result = fwrite(libatari800_get_sound_ptr(), 1, libatari800_get_sound_buffer_size(), wavfile);
		byteswritten += result;
		if (result != libatari800_get_sound_buffer_size()) {
			wavCloseSoundFile();
		}

		return result;
	}

	return 0;
}

int rleCompressLine(UBYTE *buf, UBYTE *ptr) {
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
		//printf("%dx%d ", count, last);
	} while (x < ATARI_VISIBLE_WIDTH);
	return size;
}

int rleCreate(UBYTE *buf, int buf_size, UBYTE *source) {
	UBYTE *buf_start;
	UBYTE *ptr;
	int y;
	int size;

	buf_start = buf;

	/* MRLE codec requires image origin at bottom left, so start saving at last scan
	   line and work back to the zeroth scan line. */

	for (y = Screen_HEIGHT-1; y >= 0; y--) {
		//printf("y=%d: ", y);
		ptr = source + (y * Screen_WIDTH) + ATARI_LEFT_MARGIN;
		size = rleCompressLine(buf, ptr);
		//printf(", total=%d\n", size);
		buf += size;

		/* mark end of line */
		*buf++ = 0;
		*buf++ = 0;
	}

	/* mark end of bitmap */
	*buf++ = 0;
	*buf++ = 1;

	return buf - buf_start;
}

int main(int argc, char **argv) {
	int frame;
	input_template_t input;
	int i;
	int save_wav;
	int save_video;
	int save_avi;
	int video_count;
	char video_fname[256];
	int len;
	struct gwavi_t *avi;
	struct gwavi_audio_t avi_audio;
	UBYTE image_storage[336*240*2 + 1024];
	UBYTE palette[256*3];
	UBYTE *ptr;

	save_wav = 0;
	save_video = 0;
	save_avi = 0;
	avi = NULL;
	video_count = 0;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-wav") == 0) {
			save_wav = TRUE;
		}
		else if (strcmp(argv[i], "-video") == 0) {
			save_video = TRUE;
		}
		else if (strcmp(argv[i], "-avi") == 0) {
			save_avi = TRUE;
		}
	}

	/* force the 400/800 OS to get the Memo Pad */
	char *test_args[] = {
		"-atari",
	};
	libatari800_init(sizeof(test_args) / sizeof(test_args[0]), test_args);

	libatari800_clear_input_array(&input);

	emulator_state_t state;
	cpu_state_t *cpu;
	unsigned char *pc;

	printf("sound: freq=%d, frames=%d, bytes/sample=%d, channels=%d, buffer size=%d\n", libatari800_get_sound_frequency(), libatari800_get_num_sound_samples(), libatari800_get_sound_sample_size(), libatari800_get_num_sound_channels(), libatari800_get_sound_buffer_size());

	frame = 0;
	if (save_wav) {
		wavOpenSoundFile("libatari800_test.wav");
	}
	if (save_avi) {
		avi_audio.channels = libatari800_get_num_sound_channels();
		avi_audio.bits = libatari800_get_sound_sample_size() * 8;
		avi_audio.samples_per_second = libatari800_get_sound_frequency();
		ptr = palette;
		for (i = 0; i < 256; i++) {
			*ptr++ = Colours_GetR(i);
			*ptr++ = Colours_GetG(i);
			*ptr++ = Colours_GetB(i);
		}

		avi = gwavi_open("libatari800_test.avi", 336, 240, "MRLE", 60, &avi_audio, palette);
		if (!avi) {
			printf("failed opening avi\n");
		}

		Multimedia_OpenVideoFile("atari000.avi");
	}
	while (frame < 200) {
		libatari800_get_current_state(&state);
		cpu = (cpu_state_t *)&state.state[state.tags.cpu];  /* order: A,SR,SP,X,Y */
		pc = &state.state[state.tags.pc];
		printf("frame %d: A=%02x X=%02x Y=%02x SP=%02x SR=%02x PC=%04x\n", frame, cpu->A, cpu->X, cpu->Y, cpu->P, cpu->S, pc[0] + 256 * pc[1]);
		libatari800_next_frame(&input);
		if (save_video) {
			sprintf(video_fname, "libatari800_test_%05d.pcx", video_count);
			Screen_SaveScreenshot(video_fname, 0);
			video_count++;
		}
		if (frame > 100) {
			debug_screen();
			input.keychar = 'A';
		}
		if (save_wav) {
			wavWriteToSoundFile();
		}
		if (avi) {
			len = rleCreate(image_storage, sizeof(image_storage), libatari800_get_screen_ptr());
			if (gwavi_add_frame(avi, image_storage, len) == -1) {
                printf("Cannot add video to avi\n");
                avi = NULL;
            }
		}
		if (avi) {
			if (gwavi_add_audio(avi, libatari800_get_sound_ptr(), libatari800_get_sound_buffer_size()) == -1) {
                printf("Cannot add audio to avi\n");
                avi = NULL;
            }
		}
		frame++;
	}
	if (save_wav) {
		wavCloseSoundFile();
	}
	if (avi) {
		gwavi_close(avi);
		Multimedia_CloseFile();
	}

	libatari800_exit();

	if (save_video) {
		printf("\n\nCreate mp4 usisg ffmpeg command line:\n");
		if (save_wav) {
			printf("ffmpeg -framerate 60 -i libatari800_test_%%05d.pcx -i libatari800_test.wav -acodec aac output.mp4\n");
		}
		else {
			printf("ffmpeg -framerate 60 -i libatari800_test_%%05d.pcx output.mp4\n");
		}
	}
}

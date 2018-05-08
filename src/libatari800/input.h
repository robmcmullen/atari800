#ifndef LIBATARI800_H_
#define LIBATARI800_H_

#include <stdio.h>

        /* Sound sample rate - number of frames per second: 1000..65535. */
#define LIBATARI800_SOUND_FREQ 128

        /* Number of bytes per each sample, also determines sample format:
           1 = unsigned 8-bit format.
           2 = signed 16-bit system-endian format. */
#define LIBATARI800_SOUND_SAMPLE_SIZE 130

        /* Number of audio channels: 1 = mono, 2 = stereo. */
#define LIBATARI800_SOUND_CHANNELS 132

        /* Length of the hardware audio buffer in milliseconds. */
#define LIBATARI800_SOUND_BUFFER_MS 134

        /* Size of the hardware audio buffer in frames. Computed internally,
           equals freq * buffer_ms / 1000. */
#define LIBATARI800_SOUND_BUFFER_FRAMES 136


#define LIBATARI800_FLAG_DELTA_MOUSE 0
#define LIBATARI800_FLAG_DIRECT_MOUSE 1

extern unsigned char *input_array;

int LIBATARI800_Input_Initialise(int *argc, char *argv[]);

void LIBATARI800_Mouse(void);

#endif /* LIBATARI800_H_ */

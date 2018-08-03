#include <stdio.h>

#include "libatari800.h"

static void debug_screen()
{
    unsigned char *screen = libatari800_get_screen_ptr();
    int x, y;

    screen += 384 * 24 + 24;
    for (y = 0; y < 32; y++) {
       for (x = 8; x < 88; x++) {
           /*printf(" %02x", src[x]);*/
           /* print out text version of screen, assuming graphics 0 memo pad boot screen */
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

char *test_args[] = {
    "libatari800_test",
    "-atari",  /* force Memo Pad to see typing */
};

int main(int argc, char **argv) {
    int frame;
    input_template_t input;

    libatari800_init(2, test_args);
    libatari800_clear_input_array(&input);

    frame = 0;
    while (frame < 100) {
        printf("frame %d\n", frame);
        libatari800_next_frame(&input);
        frame++;
    }
    while (frame < 200) {
        printf("frame %d\n", frame);
        input.keychar = 'A';
        libatari800_next_frame(&input);
        frame++;
        debug_screen();
    }
}
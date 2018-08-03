#ifndef LIBATARI800_H_
#define LIBATARI800_H_

#define UBYTE unsigned char

typedef struct {
    UBYTE keychar;
    UBYTE keycode;
    UBYTE special;
    UBYTE shift;
    UBYTE control;
    UBYTE start;
    UBYTE select;
    UBYTE option;
    UBYTE joy0;
    UBYTE trig0;
    UBYTE joy1;
    UBYTE trig1;
    UBYTE joy2;
    UBYTE trig2;
    UBYTE joy3;
    UBYTE trig3;
    UBYTE mousex;
    UBYTE mousey;
    UBYTE mouse_buttons;
    UBYTE mouse_mode;
} input_template_t;

int libatari800_init(int argc, char **argv);

void libatari800_clear_input_array(input_template_t *input);

int libatari800_next_frame(input_template_t *input);

int libatari800_mount_disk_image(int diskno, const char *filename, int readonly);

int libatari800_reboot_with_file(const char *filename);

UBYTE *libatari800_get_main_memory_ptr();

UBYTE *libatari800_get_screen_ptr();

#endif /* LIBATARI800_H_ */

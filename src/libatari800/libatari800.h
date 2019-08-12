#ifndef LIBATARI800_H_
#define LIBATARI800_H_

#ifndef UBYTE
#define UBYTE unsigned char
#endif

#ifndef UWORD
#define UWORD unsigned short
#endif

#ifndef ULONG
#include <stdint.h>
#define ULONG uint32_t
#endif

#ifndef FALSE
#define FALSE  0
#endif
#ifndef TRUE
#define TRUE   1
#endif

#define LIBATARI800_STICK_UP 0x1
#define LIBATARI800_STICK_DOWN 0x2
#define LIBATARI800_STICK_LEFT 0x4
#define LIBATARI800_STICK_RIGHT 0x8

#define LIBATARI800_TRIG_0 0x1
#define LIBATARI800_TRIG_1 0x2
#define LIBATARI800_TRIG_2 0x4
#define LIBATARI800_TRIG_3 0x8
#define LIBATARI800_TRIG_4 0x10
#define LIBATARI800_TRIG_5 0x20
#define LIBATARI800_TRIG_6 0x40
#define LIBATARI800_TRIG_7 0x80

typedef struct {
    UBYTE keychar;  /* ascii key value, 0 = no key press */
    UBYTE keycode;  /* keyboard code, 0 = no key press */
    UBYTE special_key;  /* non-standard key (option, select, etc.), */
    UBYTE flags;  /* see LIBATARI800_INPUT_FLAG_* defines below */
    UBYTE joystick_triggers;  /* bit 0 = trig 0, bit 1 = trig 1, etc. */

    /* 2 joysticks per byte
       byte 0 = low nibble is stick 0, high nibble is stick 1
       byte 1 = low nibble is stick 2, high nibble is stick 3
       Nibble values as in LIBATARI800_STICK_[UP|DOWN|LEFT|RIGHT]
        */
    UBYTE joysticks[2];
    UBYTE paddle_triggers;  /* same as joystick triggers */
    UBYTE paddles[8];  /* one byte each, paddles 0 - 7 */
    UBYTE mouse_x;
    UBYTE mouse_y;
    UBYTE mouse_buttons;
} input_template_t;

/* the following constants act as bit masks on the flags member of the
   input_template_t structure, where a bit value of 1 is pressed and 0 is not
   pressed. (Note: this is opposite meaning of the atari800 source where 0
   means pressed, but the usage here in the input array is in the usual boolean
   sense where a bit value of 1 means true.)*/
#define LIBATARI800_INPUT_FLAG_START 0x1
#define LIBATARI800_INPUT_FLAG_SELECT 0x2
#define LIBATARI800_INPUT_FLAG_OPTION 0x4
#define LIBATARI800_INPUT_FLAG_SHIFT 0x10
#define LIBATARI800_INPUT_FLAG_CTRL 0x20

/* The mouse mode flag bit for the flags member of input_template_t controls how
   the mouse is interpreted. If the bit is not set, direct mouse mode is used
   where values are sent directly to POKEY potentiometer input; otherwise mouse
   movements are sent as deltas
*/
#define LIBATARI800_INPUT_FLAG_MOUSE_DELTA 0x80


#define STATESAV_MAX_SIZE 210000

/* byte offsets into output_template.state array of groups of data
   to prevent the need for a full parsing of the save state data to
   be able to find parts needed for the visualizer display
 */
typedef struct {
    ULONG size;
    ULONG cpu;
    ULONG pc;
    ULONG base_ram;
    ULONG base_ram_attrib;
    ULONG antic;
    ULONG gtia;
    ULONG pia;
    ULONG pokey;
} statesav_tags_t;

typedef struct {
    UBYTE selftest_enabled;
} statesav_flags_t;

typedef struct {
    union {
        statesav_tags_t tags;
        UBYTE tags_storage[128];
    };
    union {
        statesav_flags_t flags;
        UBYTE flags_storage[128];
    };
    UBYTE state[STATESAV_MAX_SIZE];
} emulator_state_t;

typedef struct {
    UBYTE A;
    UBYTE P;
    UBYTE S;
    UBYTE X;
    UBYTE Y;
    UBYTE IRQ;
} cpu_state_t;

typedef struct {
    UBYTE DMACTL;
    UBYTE CHACTL;
    UBYTE HSCROL;
    UBYTE VSCROL;
    UBYTE PMBASE;
    UBYTE CHBASE;
    UBYTE NMIEN;
    UBYTE NMIST;
    UBYTE IR;
    UBYTE anticmode;
    UBYTE dctr;
    UBYTE lastline;
    UBYTE need_dl;
    UBYTE vscrol_off;
    UWORD dlist;
    UWORD screenaddr;
    int xpos;
    int xpos_limit;
    int ypos;
} antic_state_t;

typedef struct {
    UBYTE HPOSP0;
    UBYTE HPOSP1;
    UBYTE HPOSP2;
    UBYTE HPOSP3;
    UBYTE HPOSM0;
    UBYTE HPOSM1;
    UBYTE HPOSM2;
    UBYTE HPOSM3;
    UBYTE PF0PM;
    UBYTE PF1PM;
    UBYTE PF2PM;
    UBYTE PF3PM;
    UBYTE M0PL;
    UBYTE M1PL;
    UBYTE M2PL;
    UBYTE M3PL;
    UBYTE P0PL;
    UBYTE P1PL;
    UBYTE P2PL;
    UBYTE P3PL;
    UBYTE SIZEP0;
    UBYTE SIZEP1;
    UBYTE SIZEP2;
    UBYTE SIZEP3;
    UBYTE SIZEM;
    UBYTE GRAFP0;
    UBYTE GRAFP1;
    UBYTE GRAFP2;
    UBYTE GRAFP3;
    UBYTE GRAFM;
    UBYTE COLPM0;
    UBYTE COLPM1;
    UBYTE COLPM2;
    UBYTE COLPM3;
    UBYTE COLPF0;
    UBYTE COLPF1;
    UBYTE COLPF2;
    UBYTE COLPF3;
    UBYTE COLBK;
    UBYTE PRIOR;
    UBYTE VDELAY;
    UBYTE GRACTL;
} gtia_state_t;

typedef struct {
    UBYTE KBCODE;
    UBYTE IRQST;
    UBYTE IRQEN;
    UBYTE SKCTL;

    int shift_key;
    int keypressed;
    int DELAYED_SERIN_IRQ;
    int DELAYED_SEROUT_IRQ;
    int DELAYED_XMTDONE_IRQ;

    UBYTE AUDF[4];
    UBYTE AUDC[4];
    UBYTE AUDCTL[4];

    int DivNIRQ[4];
    int DivNMax[4];
    int Base_mult[4];
} pokey_state_t;

extern int libatari800_error_code;
#define LIBATARI800_UNIDENTIFIED_CART_TYPE 1
#define LIBATARI800_CPU_CRASH 2
#define LIBATARI800_BRK_INSTRUCTION 3
#define LIBATARI800_DLIST_ERROR 4
#define LIBATARI800_SELF_TEST 5
#define LIBATARI800_MEMO_PAD 6
#define LIBATARI800_INVALID_ESCAPE_OPCODE 7

int libatari800_init(int argc, char **argv);

char *libatari800_error_message();

void libatari800_clear_input_array(input_template_t *input);

int libatari800_next_frame(input_template_t *input);

int libatari800_mount_disk_image(int diskno, const char *filename, int readonly);

int libatari800_reboot_with_file(const char *filename);

UBYTE *libatari800_get_main_memory_ptr();

UBYTE *libatari800_get_screen_ptr();

cpu_state_t *libatari800_get_cpu_ptr();

void libatari800_get_current_state(emulator_state_t *state);

void libatari800_restore_state(emulator_state_t *state);

#endif /* LIBATARI800_H_ */

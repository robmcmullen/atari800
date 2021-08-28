#ifndef HEADLESS_COMMANDS_H
#define HEADLESS_COMMANDS_H

#include <stdio.h>

#include "config.h"
#include "atari.h"

#define HEADLESS_MEDIA_TYPE_UNKNOWN 0
#define HEADLESS_MEDIA_TYPE_WAV 1
#define HEADLESS_MEDIA_TYPE_AVI 2

extern char HEADLESS_media_file[FILENAME_MAX];
extern int HEADLESS_media_type;
extern int HEADLESS_debug_screen;
extern int HEADLESS_keydown_time;
extern int HEADLESS_keyup_time;

#define COMMAND_ERROR 0
#define COMMAND_RECORD 1
#define COMMAND_STEP 2
#define COMMAND_TYPE 3
#define COMMAND_KEYSTROKE 4

typedef struct {
    ULONG command;
    SLONG number;
    SLONG index;
    UBYTE keychar;
    UBYTE keycode;
    UBYTE shift;
    UBYTE control;
    UBYTE modifier;
} COMMAND_t;

int Headless_SetOutputFile(char *filename);
int Headless_AddIntCommand(int cmd, int arg);
int Headless_AddStrCommand(int cmd, char *arg);
void Headless_ShowCommands(void);
int Headless_ProcessCommand(COMMAND_t *cmd);
void Headless_RunCommands(void);

#endif /* HEADLESS_COMMANDS_H */

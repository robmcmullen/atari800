#ifndef HEADLESS_GLOBALS_H_
#define HEADLESS_GLOBALS_H_

#include <stdio.h>

#include "config.h"
#include "atari.h"

#define OUTPUT_MEDIA_TYPE_UNKNOWN 0
#define OUTPUT_MEDIA_TYPE_WAV 1
#define OUTPUT_MEDIA_TYPE_AVI 2

extern char output_media_file[FILENAME_MAX];
extern int output_media_type;

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

int GLOBALS_SetOutputFile(char *filename);
int GLOBALS_AddIntCommand(int cmd, int arg);
int GLOBALS_AddStrCommand(int cmd, char *arg);
void GLOBALS_ShowCommands(void);
void GLOBALS_ProcessCommand(COMMAND_t *cmd);
void GLOBALS_RunCommands(void);

#endif /* HEADLESS_GLOBALS_H_ */

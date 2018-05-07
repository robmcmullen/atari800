#ifndef GENERIC_VIDEO_H_
#define GENERIC_VIDEO_H_

#include <stdio.h>

#include "config.h"

int GENERIC_Video_Initialise(int *argc, char *argv[]);
void GENERIC_Video_Exit(void);
void GENERIC_DebugVideo(unsigned char *);

#endif /* GENERIC_VIDEO_H_ */

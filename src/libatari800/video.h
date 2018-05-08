#ifndef LIBATARI800_VIDEO_H_
#define LIBATARI800_VIDEO_H_

#include <stdio.h>

#include "config.h"

int LIBATARI800_Video_Initialise(int *argc, char *argv[]);
void LIBATARI800_Video_Exit(void);
void LIBATARI800_DebugVideo(unsigned char *);

#endif /* LIBATARI800_VIDEO_H_ */

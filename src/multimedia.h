#ifndef MULTIMEDIA_H_
#define MULTIMEDIA_H_

#include "atari.h"

int Multimedia_IsFileOpen(void);
int Multimedia_CloseFile(void);
int Multimedia_OpenSoundFile(const char *szFileName);
int Multimedia_WriteToSoundFile(const UBYTE *ucBuffer, unsigned int uiSize);
#ifndef CURSES_BASIC
int Multimedia_OpenVideoFile(const char *szFileName);
#endif

FILE *WAV_OpenFile(const char *szFileName);
int WAV_WriteSamples(const unsigned char *buf, unsigned int num_samples, unsigned int sample_size, FILE *fp);
int WAV_CloseFile(FILE *fp, int num_bytes);

#ifndef CURSES_BASIC
FILE *AVI_OpenFile(const char *szFileName);
int AVI_CloseFile(FILE *fp);
#endif

void fputw(UWORD, FILE *fp);
void fputl(ULONG, FILE *fp);

#endif /* MULTIMEDIA_H_ */


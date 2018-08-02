#ifndef STATESAV_H_
#define STATESAV_H_

#include "config.h"
#include "atari.h"

#define STATESAV_MAX_SIZE 210000 /* max size of state save data */

int StateSav_SaveAtariState(const char *filename, const char *mode, UBYTE SaveVerbose);
int StateSav_ReadAtariState(const char *filename, const char *mode);

void StateSav_SaveUBYTE(const UBYTE *data, int num);
void StateSav_SaveUWORD(const UWORD *data, int num);
void StateSav_SaveINT(const int *data, int num);
void StateSav_SaveFNAME(const char *filename);

void StateSav_ReadUBYTE(UBYTE *data, int num);
void StateSav_ReadUWORD(UWORD *data, int num);
void StateSav_ReadINT(int *data, int num);
void StateSav_ReadFNAME(char *filename);

#ifdef LIBATARI800
ULONG StateSav_Tell(void);
#include "libatari800/statesav.h"
#define STATESAV_TAG(a) (LIBATARI800_StateSav_tags->a = StateSav_Tell())
#else
#define STATESAV_TAG(a)
#endif

#endif /* STATESAV_H_ */

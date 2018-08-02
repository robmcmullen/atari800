#ifndef LIBATARI800_STATESAV_H_
#define LIBATARI800_STATESAV_H_

#include <stdio.h>
#include <stdint.h>

#include "config.h"
#include "atari.h"
#include "../statesav.h"


/* byte offsets into output_template.state array of groups of data
   to prevent the need for a full parsing of the save state data to
   be able to find parts needed for the visualizer display
 */
typedef struct {
    ULONG cpu;
    ULONG pc;
    ULONG base_ram;
    ULONG base_ram_attrib;
    ULONG antic;
    ULONG gtia;
    ULONG pia;
    ULONG pokey;
} statesav_tags_t;

extern UBYTE *LIBATARI800_StateSav_buffer;
extern statesav_tags_t *LIBATARI800_StateSav_tags;

void LIBATARI800_StateSave(UBYTE *buffer, statesav_tags_t *tags);
void LIBATARI800_StateLoad(UBYTE *buffer);

#endif /* LIBATARI800_STATESAV_H_ */

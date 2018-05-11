#!/usr/bin/env python
import sys
import ctypes
import time

import numpy as np

import pyatari800
from pyatari800.save_state_parser import parse_state

if __name__ == "__main__":
    emu = pyatari800.Atari800()
    emu.begin_emulation()
    names = emu.names
    while emu.output['frame_number'] < 4000:
        emu.next_frame()
        print "run.py frame count =", emu.output['frame_number']
        emu.debug_video()
        if emu.output['frame_number'] > 10:
            raw = emu.raw_array
            ram_offset = names['ram_ram']
            ram = raw[ram_offset:ram_offset + 256*256]
            # print("\n".join(sorted([str((k, v)) for k, v in names.iteritems()])))
            pc = raw[names['PC'] + 1] * 256 + raw[names['PC']]
            current = " ".join(["%02x" % i for i in ram[pc: pc + 3]])
            print("A=%02x X=%02x Y=%02x SP=%02x FLAGS=%02x PC=%04x RAM=%s" % (raw[names['CPU_A']], raw[names['CPU_X']], raw[names['CPU_Y']], raw[names['CPU_S']], raw[names['CPU_P']], pc, current))
        if emu.output['frame_number'] > 100:
            emu.input['keychar'] = ord('A')

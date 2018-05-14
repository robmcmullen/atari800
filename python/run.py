#!/usr/bin/env python
import sys
import ctypes
import time

import numpy as np

import pyatari800 as a8
akey = a8.akey

from pyatari800.save_state_parser import parse_state


if __name__ == "__main__":
    emu = a8.Atari800()
    emu.begin_emulation()
    names = emu.names
    while emu.output['frame_number'] < 20:
        emu.next_frame()
        print "run.py frame count =", emu.output['frame_number']
        if emu.output['frame_number'] > 11:
            emu.enter_debugger()
        elif emu.output['frame_number'] > 10:
            emu.debug_video()
            a, x, y, s, sp, pc = emu.get_cpu()
            print("A=%02x X=%02x Y=%02x SP=%02x FLAGS=%02x PC=%04x" % (a, x, y, s, sp, pc))
        if emu.output['frame_number'] > 100:
            emu.input['keychar'] = ord('A')

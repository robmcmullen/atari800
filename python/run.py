#!/usr/bin/env python
from multiprocessing import Process, Array, RawArray
import ctypes
import time

import pyatari800

if __name__ == "__main__":
    emu = pyatari800.Atari800()
    while emu.output['frame_number'] < 4000:
        emu.next_frame()
        print "run.py frame count =", emu.output['frame_number']
        emu.debug_video()
        if emu.output['frame_number'] > 100:
            emu.input['keychar'] = ord('A')

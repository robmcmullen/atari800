import ctypes
import time
import numpy as np

from . import libatari800 as liba8
from . import generic_interface as g
from . import akey
from .ui import BitmapScreen, OpenGLScreen, GLSLScreen
from .save_state_parser import parse_state
from .colors import NTSC
from _metadata import __version__

debug_frames = False

def debug_video(mem):
    offset = 336*24 + 128
    for y in range(32):
        print "%x:" % offset,
        for x in range(8,60):
            c = mem[x + offset]
            if (c == 0 or c == '\x00'):
                print " ",
            elif (c == 0x94 or c == '\x94'):
                print ".",
            elif (c == 0x9a or c == '\x9a'):
                print "X",
            else:
                try:
                    print ord(c),
                except TypeError:
                    print repr(c),
        print
        offset += 336;

def clamp(val):
    if val < 0.0:
        return 0
    elif val > 255.0:
        return 255
    return int(val)

ntsc_iq_lookup = [
    [  0.000,  0.000 ],
    [  0.144, -0.189 ],
    [  0.231, -0.081 ],
    [  0.243,  0.032 ],
    [  0.217,  0.121 ],
    [  0.117,  0.216 ],
    [  0.021,  0.233 ],
    [ -0.066,  0.196 ],
    [ -0.139,  0.134 ],
    [ -0.182,  0.062 ],
    [ -0.175, -0.022 ],
    [ -0.136, -0.100 ],
    [ -0.069, -0.150 ],
    [  0.005, -0.159 ],
    [  0.071, -0.125 ],
    [  0.124, -0.089 ],
    ]

def gtia_ntsc_to_rgb_table(val):
    # This is a better representation of the NTSC colors using a lookup table
    # rather than the phase calculations. Also from the same thread:
    # http://atariage.com/forums/topic/107853-need-the-256-colors/page-2#entry1319398
    cr = (val >> 4) & 15;
    lm = val & 15;

    y = 255*(lm+1)/16;
    i = ntsc_iq_lookup[cr][0] * 255
    q = ntsc_iq_lookup[cr][1] * 255

    r = y + 0.956*i + 0.621*q;
    g = y - 0.272*i - 0.647*q;
    b = y - 1.107*i + 1.704*q;

    return clamp(r), clamp(g), clamp(b)

def ntsc_color_map():
    rmap = np.empty(256, dtype=np.uint8)
    gmap = np.empty(256, dtype=np.uint8)
    bmap = np.empty(256, dtype=np.uint8)

    for i in range(256):
        r, g, b = gtia_ntsc_to_rgb_table(i)
        rmap[i] = r
        gmap[i] = g
        bmap[i] = b

    return rmap, gmap, bmap


def start_monitor_event_loop(emu):
    print("Monitor event loop here!")
    liba8.get_current_state(emu.output)
    a, x, y, s, sp, pc = emu.get_cpu()
    print("A=%02x X=%02x Y=%02x SP=%02x FLAGS=%02x PC=%04x" % (a, x, y, s, sp, pc))
    liba8.monitor_step()
    #time.sleep(.5)


class Atari800(object):
    cpu = "6502"
    name = "atari800"
    pretty_name = "Atari 800"

    def __init__(self):
        self.input = np.zeros([1], dtype=g.INPUT_DTYPE)
        self.output = np.zeros([1], dtype=g.OUTPUT_DTYPE)

        self.width = g.VIDEO_WIDTH
        self.height = g.VIDEO_HEIGHT
        self.frame_count = 0
        self.rmap, self.gmap, self.bmap = ntsc_color_map()
        self.frame_event = []
        self.history = []
        self.set_alpha(False)
        self.offsets = None
        self.names = None
        self.segments = None
        self.active_event_loop = None

    @property
    def raw_array(self):
        return self.output.view(dtype=np.uint8)

    @property
    def video_array(self):
        return self.output['video'][0]

    @property
    def audio_array(self):
        return self.output['audio'][0]

    @property
    def state_array(self):
        return self.output['state'][0]

    @property
    def current_frame_number(self):
        return self.output['frame_number'][0]

    def begin_emulation(self, args=None, event_loop=None, event_loop_args=None):
        self.args = self.normalize_args(args)
        if event_loop is None:
            event_loop = start_monitor_event_loop
        if event_loop_args is None:
            event_loop_args = self
        liba8.start_emulator(self.args, event_loop, event_loop_args)
        liba8.prepare_arrays(self.input, self.output)
        self.parse_state()

    def normalize_args(self, args):
        if args is None:
            args = [
                "-basic",
                #"-shmem-debug-video",
                #"jumpman.atr"
            ]
        return args

    def parse_state(self):
        base = np.byte_bounds(self.output)[0]
        self.video_start_offset = np.byte_bounds(self.video_array)[0] - base
        self.audio_start_offset = np.byte_bounds(self.audio_array)[0] - base
        self.state_start_offset = np.byte_bounds(self.state_array)[0] - base
        self.offsets, self.names, self.segments = parse_state(self.output['state'], self.state_start_offset)
        self.segments[0:0] = [
            (self.video_start_offset, self.video_start_offset + len(self.video_array), 0, "Video Frame"),
            (self.audio_start_offset, self.audio_start_offset + len(self.audio_array), 0, "Audio Data"),
        ]

    def end_emulation(self):
        pass

    def debug_video(self):
        debug_video(self.output[0]['video'].view(dtype=np.uint8))

    def next_frame(self):
        self.frame_count += 1
        liba8.next_frame(self.input, self.output)
        self.process_frame_events()
        self.save_history()

    def process_frame_events(self):
        still_waiting = []
        for count, callback in self.frame_event:
            if self.frame_count >= count:
                print "processing %s", callback
                callback()
            else:
                still_waiting.append((count, callback))
        self.frame_event = still_waiting

    def get_cpu(self):
        raw = self.raw_array
        names = self.names
        try:
            pc = raw[names['PC'] + 1] * 256 + raw[names['PC']]
            return raw[names['CPU_A']], raw[names['CPU_X']], raw[names['CPU_Y']], raw[names['CPU_S']], raw[names['CPU_P']], pc
        except TypeError:
            # emulator hasn't generated a frame yet, so save state hasn't been
            # parsed. This initial data is from pagetable.com for the first few
            # operations after the processor is turned on.
            return (0xaa, 0, 0, 0, 0x2, 0xfffc)

    def get_ram(self):
        raw = self.output['state'][0]
        ram_offset = self.offsets['ram_ram']
        ram = raw[ram_offset:ram_offset + 256*256]
        return ram

    # Utility functions

    def load_disk(self, drive_num, pathname):
        liba8.load_disk(drive_num, pathname)

    def save_history(self):
        # History is saved in a big list, which will waste space for empty
        # entries but makes things extremely easy to manage. Simply delete
        # a history entry by setting it to NONE.
        frame_number = self.output['frame_number'][0]
        if self.frame_count % 10 == 0:
            d = self.output.copy()
        else:
            d = None
        if len(self.history) != frame_number:
            print("frame %d: history out of sync. has=%d expecting=%d" % (frame_number, len(self.history), frame_number))
        self.history.append(d)
        if d is not None:
            self.print_history(frame_number)

    def restore_history(self, frame_number):
        print("restoring state from frame %d" % frame_number)
        if frame_number < 0:
            return
        d = self.history[frame_number]
        liba8.restore_state(d)
        self.history[frame_number + 1:] = []  # remove frames newer than this
        print("  %d items remain in history" % len(self.history))
        self.frame_event = []

    def print_history(self, frame_number):
        d = self.history[frame_number]
        print "history[%d] of %d: %d %s" % (d['frame_number'], len(self.history), len(d), d['state'][0][0:8])

    def get_previous_history(self, frame_cursor):
        n = frame_cursor - 1
        while n > 0:
            if self.history[n] is not None:
                return n
            n -= 1
        raise IndexError("No previous frame")

    def get_next_history(self, frame_cursor):
        n = frame_cursor + 1
        while n < len(self.history):
            if self.history[n] is not None:
                return n
            n += 1
        raise IndexError("No next frame")

    def get_color_indexed_screen(self, frame_number=-1):
        if frame_number < 0:
            output = self.output
        else:
            output = self.history[frame_number]
        raw = output['video'].reshape((self.height, self.width))
        #print "get_raw_screen", frame_number, raw
        return raw

    def set_alpha(self, use_alpha):
        if use_alpha:
            self.get_frame = self.get_frame_4
            components = 4
        else:
            self.get_frame = self.get_frame_3
            components = 3
        self.screen = np.empty((self.height, self.width, components), np.uint8)

    def get_frame_3(self, frame_number=-1):
        raw = self.get_color_indexed_screen(frame_number)
        self.screen[:,:,0] = self.rmap[raw]
        self.screen[:,:,1] = self.gmap[raw]
        self.screen[:,:,2] = self.bmap[raw]
        return self.screen

    def get_frame_4(self, frame_number=-1):
        raw = self.get_color_indexed_screen(frame_number)
        self.screen[:,:,0] = self.rmap[raw]
        self.screen[:,:,1] = self.gmap[raw]
        self.screen[:,:,2] = self.bmap[raw]
        self.screen[:,:,3] = 255
        return self.screen

    get_frame = None

    def send_char(self, key_char):
        self.input['keychar'] = key_char
        self.input['keycode'] = 0
        self.input['special'] = 0

    def send_keycode(self, keycode):
        self.input['keychar'] = 0
        self.input['keycode'] = keycode
        self.input['special'] = 0

    def send_special_key(self, key_id):
        self.input['keychar'] = 0
        self.input['keycode'] = 0
        self.input['special'] = key_id
        if key_id in [2, 3]:
            self.frame_event.append((self.frame_count + 2, self.clear_keys))

    def clear_keys(self):
        self.input['keychar'] = 0
        self.input['keycode'] = 0
        self.input['special'] = 0

    def set_option(self, state):
        self.input['option'] = state

    def set_select(self, state):
        self.input['select'] = state

    def set_start(self, state):
        self.input['start'] = state

    ##### debugger convenience functions

    def enter_debugger(self):
        if self.active_event_loop is not None:
            print("Only one debugger may be active at a time!")
        else:
            print("Requesting debugger start via AKEY_UI")
            self.send_special_key(akey.AKEY_UI)

    def leave_debugger(self):
        liba8.monitor_clear()
        self.restart_cpu()

    def restart_cpu(self):
        if self.active_event_loop is not None:
            self.clear_keys()
            self.active_event_loop.Exit()
            print("alternate event loop is over.")
            self.active_event_loop = None
            liba8.monitor_summary()

    def get_current_state(self):
        liba8.get_current_state(self.output)

    def debugger_step(self):
        liba8.monitor_step()
        self.restart_cpu()

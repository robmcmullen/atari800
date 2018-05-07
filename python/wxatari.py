#!/usr/bin/env python

import os
import sys
import time
import wx
import wx.lib.newevent
try:
    import wx.glcanvas as glcanvas
    import OpenGL.GL as gl
    HAS_OPENGL = True
except ImportError:
    HAS_OPENGL = False

# FIXME: OpenGL on wx4 is segfaulting.
# update: Legacy OpenGL is segfaulting, GLSL seems to work.
#HAS_OPENGL = False

import numpy as np
from intscale import intscale

# Include pyatari directory so that modules can be imported normally
import sys
module_dir = os.path.realpath(os.path.abspath(".."))
if module_dir not in sys.path:
    sys.path.insert(0, module_dir)
import pyatari800
from pyatari800 import akey
from pyatari800.ui_wx import wxGLSLTextureCanvas, wxLegacyTextureCanvas

import logging
logging.basicConfig(level=logging.WARNING)
log = logging.getLogger(__name__)
#log.setLevel(logging.DEBUG)


class EmulatorControlBase(object):
    def __init__(self, parent, emulator, autostart=False):
        self.frame = parent
        self.emulator = emulator

        self.timer = wx.Timer(self)
        self.Bind(wx.EVT_TIMER, self.on_timer)
        self.Bind(wx.EVT_SIZE, self.on_size)
        self.Bind(wx.EVT_KEY_UP, self.on_key_up)
        self.Bind(wx.EVT_KEY_DOWN, self.on_key_down)
        self.Bind(wx.EVT_CHAR, self.on_char)

        self.firsttime=True
        self.refreshed=False
        self.repeat=True
        self.forceupdate=False
        self.framerate = 1/60.0
        self.tickrate = self.framerate
        self.delay = self.tickrate * 1000  # wxpython delays are in milliseconds
        self.last_update_time = 0.0
        self.screen_scale = 1
        emulator.set_alpha(False)

        self.key_down = False

        self.on_size(None)
        if self.IsDoubleBuffered():
            self.Bind(wx.EVT_PAINT, self.on_paint)
        else:
            self.Bind(wx.EVT_PAINT, self.on_paint_double_buffer)

        if autostart:
            wx.CallAfter(self.on_start, None)

    def cleanup(self):
        self.stop_timer()

    wx_to_akey = {
        wx.WXK_BACK: akey.AKEY_BACKSPACE,
        wx.WXK_DELETE: akey.AKEY_DELETE_CHAR,
        wx.WXK_INSERT: akey.AKEY_INSERT_CHAR,
        wx.WXK_ESCAPE: akey.AKEY_ESCAPE,
        wx.WXK_END: akey.AKEY_HELP,
        wx.WXK_HOME: akey.AKEY_CLEAR,
        wx.WXK_RETURN: akey.AKEY_RETURN,
        wx.WXK_SPACE: akey.AKEY_SPACE,
        wx.WXK_F7: akey.AKEY_BREAK,
        wx.WXK_PAUSE: akey.AKEY_BREAK,
        96: akey.AKEY_ATARI,  # back tick
    }

    wx_to_akey_ctrl = {
        wx.WXK_UP: akey.AKEY_UP,
        wx.WXK_DOWN: akey.AKEY_DOWN,
        wx.WXK_LEFT: akey.AKEY_LEFT,
        wx.WXK_RIGHT: akey.AKEY_RIGHT,
    }

    def on_key_down(self, evt):
        log.debug("key down! key=%s mod=%s" % (evt.GetKeyCode(), evt.GetModifiers()))
        key = evt.GetKeyCode()
        mod = evt.GetModifiers()
        if mod == wx.MOD_ALT or self.is_paused:
            self.frame.on_emulator_command_key(evt)
            return
        elif key == wx.WXK_F11:
            self.frame.on_emulator_command_key(evt)
            return
        elif mod == wx.MOD_CONTROL:
            akey = self.wx_to_akey_ctrl.get(key, None)
        else:
            akey = self.wx_to_akey.get(key, None)

        if akey is None:
            evt.Skip()
        else:
            self.emulator.send_keycode(akey)
    
    def on_key_up(self, evt):
        log.debug("key up before evt=%s" % evt.GetKeyCode())
        key=evt.GetKeyCode()
        self.emulator.clear_keys()

        evt.Skip()

    def on_char(self, evt):
        log.debug("on_char! char=%s, key=%s, raw=%s modifiers=%s" % (evt.GetUnicodeKey(), evt.GetKeyCode(), evt.GetRawKeyCode(), bin(evt.GetModifiers())))
        mods = evt.GetModifiers()
        char = evt.GetUnicodeKey()
        if char > 0:
            self.emulator.send_char(char)
        else:
            key = evt.GetKeyCode()

        evt.Skip()

    def process_key_state(self):
        up = 0b0001 if wx.GetKeyState(wx.WXK_UP) else 0
        down = 0b0010 if wx.GetKeyState(wx.WXK_DOWN) else 0
        left = 0b0100 if wx.GetKeyState(wx.WXK_LEFT) else 0
        right = 0b1000 if wx.GetKeyState(wx.WXK_RIGHT) else 0
        self.emulator.input['joy0'] = up | down | left | right
        trig = 1 if wx.GetKeyState(wx.WXK_CONTROL) else 0
        self.emulator.input['trig0'] = trig
        #print "joy", self.emulator.input['joy0'], "trig", trig

        # console keys will reflect being pressed if at any time between frames
        # the key has been pressed
        self.emulator.input['option'] = 1 if wx.GetKeyState(wx.WXK_F2) else 0
        self.emulator.input['select'] = 1 if wx.GetKeyState(wx.WXK_F3) else 0
        self.emulator.input['start'] = 1 if wx.GetKeyState(wx.WXK_F4) else 0

    def on_size(self,evt):
        if not self.IsDoubleBuffered():
            # make new background buffer
            size  = self.GetClientSize()
            self._buffer = wx.Bitmap(size)

    def show_frame(self, frame_number=-1):
        raise NotImplementedError

    def show_audio(self):
        #import binascii
        #a = binascii.hexlify(self.emulator.audio)
        #print np.where(self.emulator.audio > 0)
        pass

    if wx.Platform == "__WXGTK__":
        def on_timer(self, evt):
            if self.timer.IsRunning():
                self.process_key_state()
                now = time.time()
                delta = now - self.last_update_time
                print("now=%f delta=%f framerate=%f" % (now, delta, self.framerate))
                if delta >= self.framerate:
                    self.emulator.next_frame()
                    print("got frame %d" % self.emulator.output['frame_number'])
                    self.show_frame()
                    self.show_audio()
                    if delta > 2 * self.framerate:
                        self.emulator.next_frame()
                        print("got extra frame %d" % self.emulator.output['frame_number'])
                        self.show_frame()
                        self.show_audio()
                        self.last_update_time = now  # + (delta % self.framerate)
                    else:
                        self.last_update_time += self.framerate
                else:
                    print("pausing a tick after frame %d" % self.emulator.output['frame_number'])
                    #self.last_update_time += self.tickrate
            evt.Skip()
    elif wx.Platform == "__WXMSW__":
        # FIXME: settles on 120%
        def on_timer(self, evt):
            if self.timer.IsRunning():
                self.process_key_state()
                now = time.time()
                if now > self.next_update_time:
                    delta = now - self.next_update_time
                    self.emulator.next_frame()
                    print("got frame %d, delta=%f" % (self.emulator.output['frame_number'], delta))
                    self.show_frame()
                    self.show_audio()

                    # updating too slowly?
                    delta = now - self.next_update_time
                    if delta > self.framerate:
                        self.emulator.next_frame()
                        print("got extra frame %d" % self.emulator.output['frame_number'])
                        self.show_frame()
                        self.show_audio()
                        self.next_update_time += self.framerate
                    self.next_update_time += self.framerate
                else:
                    print("pausing a tick after frame %d" % self.emulator.output['frame_number'])
                    #self.last_update_time += self.tickrate
            evt.Skip()

    def start_timer(self,repeat=False,delay=None,forceupdate=True):
        if not self.timer.IsRunning():
            self.repeat=repeat
            if delay is not None:
                self.delay=delay
            self.forceupdate=forceupdate
            self.last_update_time = time.time()
            self.next_update_time = time.time() + self.framerate
            self.timer.Start(self.delay)

    def stop_timer(self):
        if self.timer.IsRunning():
            self.timer.Stop()

    def on_start(self, evt=None):
        self.start_timer(repeat=True)

    @property
    def is_paused(self):
        return not self.timer.IsRunning()

    def on_pause(self, evt=None):
        self.stop_timer()


class EmulatorControl(wx.Panel, EmulatorControlBase):
    def __init__(self, parent, emulator, autostart=False):
        wx.Panel.__init__(self, parent, -1, size=(emulator.width, emulator.height))
        EmulatorControlBase.__init__(self, parent, emulator, autostart)
        self.scaled_frame = None
        self.image = None
        self.set_scale(1)

    def get_bitmap_slow(self, frame):
        scaled = self.scale_frame(frame)
        h, w, _ = scaled.shape
        image = wx.ImageFromData(w, h, scaled.tostring())
        bmp = wx.BitmapFromImage(image)
        return bmp

    def get_bitmap_fast(self, frame):
        # Slightly improved speed over converting the array to a string
        # slow x 1000: 0.261524
        # fast x 1000: 0.206890
        self.scale_frame(frame)
        # the image data has already been updated because the unterlying
        # numpy array has been changed
        bmp = wx.Bitmap(self.image)
        return bmp

    get_bitmap = get_bitmap_fast

    def bitmap_benchmark(self):
        import time
        t0 = time.clock()
        for i in range(1000):
            self.get_bitmap_slow(frame)
        print "slow x 1000: %f" % (time.clock() - t0)
        t0 = time.clock()
        for i in range(1000):
            self.get_bitmap_fast(frame)
        print "fast x 1000: %f" % (time.clock() - t0)

    def set_scale(self, scale):
        """Scale a numpy array by an integer factor

        This turns out to be too slow to be used by the screen display. OpenGL
        displays don't use this at all because the display hardware scales
        automatically.
        """
        self.screen_scale = scale
        h = self.emulator.height
        w = self.emulator.width
        if scale > 1:
            self.scaled_frame = np.empty((h * scale, w * scale, 3), dtype=np.uint8)
            self.image = wx.ImageFromBuffer(w * scale, h * scale, self.scaled_frame)
        else:
            self.scaled_frame = None
            self.image = wx.ImageFromBuffer(w, h, self.emulator.screen)

        # self.delay = 5 * scale * scale
        # self.stop_timer()
        # self.start_timer(True)

    def scale_frame(self, frame):
        if self.screen_scale == 1:
            return frame
        scaled = intscale(frame, self.screen_scale, self.scaled_frame)
        log.debug("panel scale: %d, %s" % (self.screen_scale, scaled.shape))
        return scaled

    def show_frame(self, frame_number=-1):
        if self.forceupdate or frame_number >= 0:
            dc = wx.ClientDC(self)
            self.updateDrawing(dc, frame_number)
        else:
            #self.updateDrawing()
            self.Refresh()

    def updateDrawing(self, dc, frame_number=-1):
        #dc=wx.BufferedDC(wx.ClientDC(self), self._buffer)
        frame = self.emulator.get_frame(frame_number)
        bmp = self.get_bitmap(frame)
        dc.DrawBitmap(bmp, 0,0, True)

    def on_paint(self, evt):
        dc=wx.PaintDC(self)
        self.updateDrawing(dc)
        self.refreshed=True

    def on_paint_double_buffer(self, evt):
        dc=wx.BufferedPaintDC(self,self._buffer)
        self.updateDrawing(dc)
        self.refreshed=True


class OpenGLEmulatorMixin(object):
    def bind_events(self):
        pass

    def get_raw_texture_data(self, frame_number=-1):
        raw = np.flipud(self.emulator.get_frame(frame_number))
        return raw

    def show_frame(self, frame_number=-1):
        if not self.finished_init:
            return
        frame = self.get_rgba_texture_data(frame_number)
        try:
            self.update_texture(self.display_texture, frame)
        except Exception, e:
            import traceback

            print traceback.format_exc()
            sys.exit()
        self.on_draw()

    def on_paint(self, evt):
        if not self.finished_init:
            self.init_context()
        self.show_frame()

    on_paint_double_buffer = on_paint


class OpenGLEmulatorControl(OpenGLEmulatorMixin, wxLegacyTextureCanvas, EmulatorControlBase):
    def __init__(self, parent, emulator, autostart=False):
        wxLegacyTextureCanvas.__init__(self, parent, pyatari800.NTSC, -1, size=(3*emulator.width, 3*emulator.height))
        EmulatorControlBase.__init__(self, parent, emulator, autostart)
        emulator.set_alpha(True)

    def get_rgba_texture_data(self, frame_number=-1):
        raw = np.flipud(self.emulator.get_frame(frame_number))
        log.debug("raw data for legacy version: %s" % str(raw.shape))
        return raw


class GLSLEmulatorControl(OpenGLEmulatorMixin, wxGLSLTextureCanvas, EmulatorControlBase):
    def __init__(self, parent, emulator, autostart=False):
        wxGLSLTextureCanvas.__init__(self, parent, pyatari800.NTSC, -1, size=(3*emulator.width, 3*emulator.height))
        EmulatorControlBase.__init__(self, parent, emulator, autostart)
        emulator.set_alpha(True)

    def get_color_indexed_texture_data(self, frame_number=-1):
        raw = np.flipud(self.emulator.get_color_indexed_screen(frame_number))
        log.debug("raw data for GLSL version: %s" % str(raw.shape))
        return raw


# Not running inside the wxPython demo, so include the same basic
# framework.
class EmulatorFrame(wx.Frame):
    parsed_args = []
    options = {}

    def __init__(self):
        wx.Frame.__init__(self, None, -1, "wxPython atari800 test", pos=(50,50),
                         size=(200,100), style=wx.DEFAULT_FRAME_STYLE)
        self.CreateStatusBar()

        menuBar = wx.MenuBar()
        menu = wx.Menu()
        self.id_load = wx.NewId()
        item = menu.Append(self.id_load, "Load Image", "Load a disk image")
        self.Bind(wx.EVT_MENU, self.on_menu, item)
        menu.AppendSeparator()
        item = menu.Append(wx.ID_EXIT, "E&xit\tCtrl-Q", "Exit demo")
        self.Bind(wx.EVT_MENU, self.on_menu, item)
        menuBar.Append(menu, "&File")

        self.id_pause = wx.NewId()
        self.id_coldstart = wx.NewId()
        self.id_warmstart = wx.NewId()
        menu = wx.Menu()
        self.pause_item = menu.Append(self.id_pause, "Pause", "Pause or resume the emulation")
        self.Bind(wx.EVT_MENU, self.on_menu, self.pause_item)
        menu.AppendSeparator()
        item = menu.Append(self.id_coldstart, "Cold Start", "Cold start (power switch off then on)")
        self.Bind(wx.EVT_MENU, self.on_menu, item)
        item = menu.Append(self.id_coldstart, "Warm Start", "Warm start (reset switch)")
        self.Bind(wx.EVT_MENU, self.on_menu, item)
        menuBar.Append(menu, "&Machine")

        self.id_screen1x = wx.NewId()
        self.id_screen2x = wx.NewId()
        self.id_screen3x = wx.NewId()
        self.id_glsl = wx.NewId()
        self.id_opengl = wx.NewId()
        self.id_unaccelerated = wx.NewId()
        menu = wx.Menu()
        item = menu.AppendRadioItem(self.id_glsl, "GLSL", "Use GLSL for scalable display")
        self.Bind(wx.EVT_MENU, self.on_menu, item)
        item = menu.AppendRadioItem(self.id_opengl, "OpenGL", "Use OpenGL for scalable display")
        self.Bind(wx.EVT_MENU, self.on_menu, item)
        item = menu.AppendRadioItem(self.id_unaccelerated, "Unaccelerated", "No OpenGL acceleration")
        self.Bind(wx.EVT_MENU, self.on_menu, item)
        menu.AppendSeparator()
        item = menu.Append(self.id_screen1x, "Display 1x", "No magnification")
        self.Bind(wx.EVT_MENU, self.on_menu, item)
        item = menu.Append(self.id_screen2x, "Display 2x", "2x display")
        self.Bind(wx.EVT_MENU, self.on_menu, item)
        item = menu.Append(self.id_screen3x, "Display 3x", "3x display")
        self.Bind(wx.EVT_MENU, self.on_menu, item)
        menuBar.Append(menu, "&Screen")

        self.SetMenuBar(menuBar)
        self.Show(True)
        self.Bind(wx.EVT_CLOSE, self.on_close_frame)

        self.emulator = pyatari800.Atari800(self.parsed_args)
        if self.options.unaccelerated or wx.Platform == "__WXMSW__":
            control = EmulatorControl
        elif self.options.glsl and HAS_OPENGL:
            control = GLSLEmulatorControl
        elif HAS_OPENGL:
            control = OpenGLEmulatorControl
        else:
            control = EmulatorControl
        self.emulator_panel = control(self, self.emulator, autostart=True)
        self.SetSize((800, 600))
        self.emulator_panel.SetFocus()

        self.box = wx.BoxSizer(wx.VERTICAL)
        self.box.Add(self.emulator_panel, 1, wx.EXPAND)
        self.SetSizer(self.box)

        self.frame_cursor = -1

    def set_glsl(self):
        self.set_display(GLSLEmulatorControl)

    def set_opengl(self):
        self.set_display(OpenGLEmulatorControl)

    def set_unaccelerated(self):
        self.set_display(EmulatorControl)

    def set_display(self, panel_cls):
        paused = self.emulator_panel.is_paused
        self.emulator_panel.cleanup()
        old = self.emulator_panel

        # Mac can occasionally fail to get an OpenGL context, so creation of
        # the panel can fail. Attempting to work around by giving it more
        # chances to work.
        attempts = 3
        while attempts > 0:
            attempts -= 1
            try:
                self.emulator_panel = panel_cls(self, self.emulator)
                attempts = 0
            except wx.wxAssertionError:
                log.error("Failed initializing OpenGL context. Trying %d more times" % attempts)
                time.sleep(1)

        self.box.Replace(old, self.emulator_panel)
        old.Destroy()
        print self.emulator_panel
        self.Layout()
        self.emulator_panel.SetFocus()
        if not paused:
            self.emulator_panel.on_start()

    def on_menu(self, evt):
        id = evt.GetId()
        if id == wx.ID_EXIT:
            self.emulator.end_emulation()
            self.emulator_panel.cleanup()
            self.Close(True)
        elif id == self.id_load:
            dlg = wx.FileDialog(self, "Choose a disk image", defaultDir = "", defaultFile = "", wildcard = "*.atr")
            if dlg.ShowModal() == wx.ID_OK:
                print("Opening %s" % dlg.GetPath())
                filename = dlg.GetPath()
            else:
                filename = None
            dlg.Destroy()
            if filename:
                self.emulator.load_disk(1, filename)
                self.emulator.send_special_key(akey.AKEY_COLDSTART)
        elif id == self.id_coldstart:
            self.emulator.send_special_key(akey.AKEY_COLDSTART)
        elif id == self.id_warmstart:
            self.emulator.send_special_key(akey.AKEY_WARMSTART)
        elif id == self.id_glsl:
            self.set_glsl()
        elif id == self.id_opengl:
            self.set_opengl()
        elif id == self.id_unaccelerated:
            self.set_unaccelerated()
        elif id == self.id_screen1x:
            self.emulator_panel.set_scale(1)
        elif id == self.id_screen2x:
            self.emulator_panel.set_scale(2)
        elif id == self.id_screen3x:
            self.emulator_panel.set_scale(3)
        elif id == self.id_pause:
            if self.emulator_panel.is_paused:
                self.restart()
            else:
                self.pause()

    def on_emulator_command_key(self, evt):
        key = evt.GetKeyCode()
        mod = evt.GetModifiers()
        print("emu key: %s %s" % (key, mod))
        if key == wx.WXK_LEFT:
            if not self.emulator_panel.is_paused:
                self.pause()
            else:
                self.history_previous()
        elif key == wx.WXK_RIGHT:
            if not self.emulator_panel.is_paused:
                self.pause()
            else:
                self.history_next()
        elif key == wx.WXK_SPACE or key == wx.WXK_F11:
            if self.emulator_panel.is_paused:
                self.restart()
            else:
                self.pause()

    def restart(self):
        self.emulator_panel.on_start()
        self.pause_item.SetItemLabel("Pause")
        if self.frame_cursor >= 0:
            self.emulator.restore_history(self.frame_cursor)
        self.frame_cursor = -1
        self.SetStatusText("")

    def pause(self):
        self.emulator_panel.on_pause()
        self.pause_item.SetItemLabel("Resume")
        self.frame_cursor = self.emulator.frame_count
        self.update_ui()
    
    def history_previous(self):
        if self.frame_cursor < 0:
            self.frame_cursor = self.emulator.frame_count
        try:
            self.frame_cursor = self.emulator.get_previous_history(self.frame_cursor)
            #self.emulator.restore_history(frame_number)
        except IndexError:
            return
        self.emulator_panel.show_frame(self.frame_cursor)
        self.update_ui()
    
    def history_next(self):
        if self.frame_cursor < 0:
            return
        try:
            self.frame_cursor = self.emulator.get_next_history(self.frame_cursor)
            #self.emulator.restore_history(frame_number)
        except IndexError:
            self.frame_cursor = -1
        self.emulator_panel.show_frame(self.frame_cursor)
        self.update_ui()

    def show_frame_number(self):
        text = "Paused: %d frames, showing %d" % (self.emulator.frame_count, self.frame_cursor if self.frame_cursor > 0 else self.emulator.frame_count)
        print(text)
        self.SetStatusText(text)

    def update_ui(self):
        self.show_frame_number()

    def on_close_frame(self, evt):
        self.emulator.end_emulation()
        evt.Skip()



if __name__ == '__main__':
    # use argparse rather than sys.argv to handle the difference in being
    # called as "python script.py" and "./script.py"
    import argparse

    parser = argparse.ArgumentParser(description='Atari800 WX Demo')
    parser.add_argument("--unaccelerated", "--bitmap", "--slow", action="store_true", default=False, help="Use bitmap scaling instead of OpenGL")
    parser.add_argument("--opengl", action="store_true", default=False, help="Use OpenGL scaling")
    parser.add_argument("--glsl", action="store_true", default=False, help="Use GLSL scaling")
    EmulatorFrame.options, EmulatorFrame.parsed_args = parser.parse_known_args()
    app = wx.App(False)
    frame = EmulatorFrame()
    frame.Show()
    app.MainLoop()

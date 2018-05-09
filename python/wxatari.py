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

# Include pyatari directory so that modules can be imported normally
import sys
module_dir = os.path.realpath(os.path.abspath(".."))
if module_dir not in sys.path:
    sys.path.insert(0, module_dir)
import pyatari800
from pyatari800 import akey, ui

import logging
logging.basicConfig(level=logging.WARNING)
log = logging.getLogger(__name__)
#log.setLevel(logging.DEBUG)


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
        self.pause_item = menu.Append(self.id_pause, "Pause\tCtrl-P", "Pause or resume the emulation")
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
            control = ui.EmulatorControl
        elif self.options.glsl and HAS_OPENGL:
            control = ui.GLSLEmulatorControl
        elif HAS_OPENGL:
            control = ui.OpenGLEmulatorControl
        else:
            control = ui.EmulatorControl
        self.emulator_panel = control(self, self.emulator, autostart=True)
        self.SetSize((800, 600))
        self.emulator_panel.SetFocus()

        self.cpu_status = wx.StaticText(self, -1)

        self.box = wx.BoxSizer(wx.VERTICAL)
        self.box.Add(self.emulator_panel, 1, wx.EXPAND)
        self.box.Add(self.cpu_status, 0, wx.EXPAND)
        self.SetSizer(self.box)

        self.frame_cursor = -1

    def set_glsl(self):
        self.set_display(ui.GLSLEmulatorControl)

    def set_opengl(self):
        self.set_display(ui.OpenGLEmulatorControl)

    def set_unaccelerated(self):
        self.set_display(ui.EmulatorControl)

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
        self.update_internals()

    def update_internals(self):
        a, x, y, sp, p, pc = self.emulator.get_cpu()
        #print(offsets)
        text = "A=%02x X=%02x Y=%02x SP=%02x FLAGS=%02x PC=%04x" % (a, x, y, sp, p, pc)
        self.cpu_status.SetLabel(text)

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

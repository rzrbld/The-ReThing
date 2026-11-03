#!/usr/bin/env python3
"""Send Escape only to the isolated Xvfb reference display, to skip FMV intros."""
import ctypes
import os
import re
import subprocess
import time

display_name = os.environ.get('DISPLAY', '')
if not display_name.startswith(':') or not os.environ.get('THING_REFERENCE_XVFB'):
    raise SystemExit('Requires the isolated capture_original.sh Xvfb environment')
x11 = ctypes.CDLL('libX11.so.6')
xtest = ctypes.CDLL('libXtst.so.6')
x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
x11.XOpenDisplay.restype = ctypes.c_void_p
x11.XKeysymToKeycode.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
x11.XKeysymToKeycode.restype = ctypes.c_uint
x11.XFlush.argtypes = [ctypes.c_void_p]
x11.XCloseDisplay.argtypes = [ctypes.c_void_p]
xtest.XTestFakeKeyEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_int, ctypes.c_ulong]
x11.XSetInputFocus.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int, ctypes.c_ulong]
x11.XRaiseWindow.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
display = x11.XOpenDisplay(display_name.encode())
if not display:
    raise SystemExit('Cannot open reference display')
symbols = [None] * 4 if os.environ.get('THING_REFERENCE_FOCUS_ONLY') == '1' else [0xff1b, 0xff1b, 0xff1b, 0xff0d]
for symbol in symbols:
    time.sleep(4)
    tree = subprocess.check_output(['xwininfo', '-root', '-tree'], text=True)
    match = re.search(r'(0x[0-9a-f]+) "The Thing"', tree)
    if match:
        window = int(match.group(1), 16)
        x11.XRaiseWindow(display, window)
        x11.XSetInputFocus(display, window, 2, 0)
        x11.XFlush(display)
    if symbol is None:
        continue
    key = x11.XKeysymToKeycode(display, symbol)
    xtest.XTestFakeKeyEvent(display, key, 1, 0)
    x11.XFlush(display)
    time.sleep(0.12)
    xtest.XTestFakeKeyEvent(display, key, 0, 0)
    x11.XFlush(display)
x11.XCloseDisplay(display)

# SPDX-License-Identifier: GPL-2.0-or-later
"""Embedded Windows mouse input. Hosted by Bloodborne.exe's existing --run role."""
import ctypes as C
import os
import sys
import threading
import time
from pathlib import Path
from bbport_integrated_logic import Controls
PORT = Path(sys.executable).resolve().parent
U32, I32, UPTR, IPTR = C.c_uint32, C.c_int32, C.c_size_t, C.c_ssize_t
user32 = C.WinDLL('user32', use_last_error=True)
kernel32 = C.WinDLL('kernel32', use_last_error=True)


class POINT(C.Structure):
    _fields_ = [('x', I32), ('y', I32)]


class MOUSEINPUT(C.Structure):
    _fields_ = [('dx', I32), ('dy', I32), ('mouseData', U32), ('dwFlags', U32),
                ('time', U32), ('dwExtraInfo', UPTR)]


class KEYBDINPUT(C.Structure):
    _fields_ = [('wVk', C.c_uint16), ('wScan', C.c_uint16), ('dwFlags', U32),
                ('time', U32), ('dwExtraInfo', UPTR)]


class HARDWAREINPUT(C.Structure):
    _fields_ = [('uMsg', U32), ('wParamL', C.c_uint16), ('wParamH', C.c_uint16)]


class INPUTUNION(C.Union):
    _fields_ = [('mi', MOUSEINPUT), ('ki', KEYBDINPUT), ('hi', HARDWAREINPUT)]


class INPUT(C.Structure):
    _fields_ = [('type', U32), ('data', INPUTUNION)]


class CURSORINFO(C.Structure):
    _fields_ = [('cbSize', U32), ('flags', U32), ('hCursor', C.c_void_p),
                ('ptScreenPos', POINT)]


class MSLLHOOKSTRUCT(C.Structure):
    _fields_ = [('pt', POINT), ('mouseData', U32), ('flags', U32),
                ('time', U32), ('dwExtraInfo', UPTR)]


HOOKPROC = C.WINFUNCTYPE(IPTR, C.c_int, UPTR, IPTR)
user32.SetWindowsHookExW.argtypes = [C.c_int, HOOKPROC, C.c_void_p, U32]
user32.SetWindowsHookExW.restype = C.c_void_p
user32.CallNextHookEx.argtypes = [C.c_void_p, C.c_int, UPTR, IPTR]
user32.CallNextHookEx.restype = IPTR
user32.UnhookWindowsHookEx.argtypes = [C.c_void_p]
user32.UnhookWindowsHookEx.restype = C.c_int
user32.GetForegroundWindow.restype = C.c_void_p
user32.GetWindowThreadProcessId.argtypes = [C.c_void_p, C.POINTER(U32)]
user32.GetWindowThreadProcessId.restype = U32
user32.GetCursorInfo.argtypes = [C.POINTER(CURSORINFO)]
user32.GetAsyncKeyState.argtypes = [C.c_int]
user32.GetAsyncKeyState.restype = C.c_short
user32.SendInput.argtypes = [U32, C.POINTER(INPUT), C.c_int]
user32.SendInput.restype = U32
kernel32.OpenProcess.argtypes = [U32, C.c_int, U32]
kernel32.OpenProcess.restype = C.c_void_p
kernel32.QueryFullProcessImageNameW.argtypes = [C.c_void_p, U32, C.c_wchar_p, C.POINTER(U32)]
kernel32.QueryFullProcessImageNameW.restype = C.c_int
kernel32.CloseHandle.argtypes = [C.c_void_p]
kernel32.GetModuleHandleW.argtypes = [C.c_wchar_p]
kernel32.GetModuleHandleW.restype = C.c_void_p
kernel32.CreateMutexW.argtypes = [C.c_void_p, C.c_int, C.c_wchar_p]
kernel32.CreateMutexW.restype = C.c_void_p
user32.MessageBoxW.argtypes = [C.c_void_p, C.c_wchar_p, C.c_wchar_p, U32]

SCAN = {'light': (0x44, False), 'heavy': (0x57, False), 'firearm': (0x58, False),
        'up': (0x48, True), 'down': (0x50, True),
        'left': (0x4B, True), 'right': (0x4D, True)}


def send_key(key, down):
    scan, extended = SCAN[key]
    flags = 0x0008 | (0x0001 if extended else 0) | (0 if down else 0x0002)
    event = INPUT(type=1, data=INPUTUNION(ki=KEYBDINPUT(0, scan, flags, 0, 0)))
    if user32.SendInput(1, C.byref(event), C.sizeof(INPUT)) != 1:
        raise OSError('Windows no pudo enviar la tecla. Abre juego y complemento con el mismo nivel de permisos.')


class Bridge:
    def __init__(self, root):
        self.root = root
        self.controls = Controls(send_key, bindings=root.bindings)
        self.enabled = True
        self.stopping = False
        self.last_window = None
        self.is_game = False
        self.error = None
        self.wheel_rest = 0
        self.callback = HOOKPROC(self.mouse_hook)
        self.hook = user32.SetWindowsHookExW(14, self.callback, kernel32.GetModuleHandleW(None), 0)
        if not self.hook:
            raise C.WinError(C.get_last_error())
        self.poll()

    def active(self):
        if not self.enabled or self.stopping:
            return False
        hwnd = user32.GetForegroundWindow()
        if hwnd != self.last_window:
            self.last_window = hwnd
            self.is_game = False
            pid = U32()
            user32.GetWindowThreadProcessId(hwnd, C.byref(pid))
            handle = kernel32.OpenProcess(0x1000, 0, pid.value)
            if handle:
                try:
                    text = C.create_unicode_buffer(32768)
                    length = U32(len(text))
                    if kernel32.QueryFullProcessImageNameW(handle, 0, text, C.byref(length)):
                        self.is_game = os.path.normcase(os.path.abspath(text.value)) in {os.path.normcase(
                            str((PORT / 'bin' / name).resolve())) for name in ('bb-probe.exe', 'bb-probe-ui.exe')}
                finally:
                    kernel32.CloseHandle(handle)
        if not self.is_game:
            return False
        cursor = CURSORINFO(cbSize=C.sizeof(CURSORINFO))
        # A visible cursor means launcher, overlay, text entry, or mouse look off.
        return bool(user32.GetCursorInfo(C.byref(cursor))) and not (cursor.flags & 1)

    def mouse_hook(self, code, message, address):
        try:
            if code >= 0 and message in (0x0201, 0x0202, 0x0204, 0x0205, 0x020B, 0x020C, 0x020A):
                event = C.cast(address, C.POINTER(MSLLHOOKSTRUCT)).contents
                if not (event.flags & 1):
                    if self.active():
                        shift = bool(user32.GetAsyncKeyState(0x10) & 0x8000)
                        if message == 0x0201:
                            self.controls.press('left', shift)
                        elif message == 0x0202:
                            self.controls.release('left')
                        elif message == 0x0204:
                            self.controls.press('right', shift)
                        elif message == 0x0205:
                            self.controls.release('right')
                        elif message in (0x020B, 0x020C):
                            button = 'side_back' if (event.mouseData >> 16) == 1 else 'side_forward'
                            if message == 0x020B:
                                self.controls.press(button, shift)
                            else:
                                self.controls.release(button)
                        elif message == 0x020A:
                            self.wheel_rest += C.c_int16(event.mouseData >> 16).value
                            # Avoid runaway pulse queues from free-spinning wheels.
                            steps = max(-4, min(4, int(self.wheel_rest / 120)))
                            if steps:
                                self.wheel_rest = 0
                                self.controls.wheel(steps > 0, shift)
                    else:
                        self.controls.cancel()
                        self.wheel_rest = 0
        except Exception as exc:
            self.error = str(exc)
            self.enabled = False
        # Preserve native mouse motion and all clicks for overlays/other programs.
        return user32.CallNextHookEx(self.hook, code, message, address)

    def poll(self):
        if self.stopping:
            return
        try:
            if self.active():
                self.controls.tick()
            else:
                self.controls.cancel()
                self.wheel_rest = 0
        except Exception as exc:
            self.error = str(exc)
            self.enabled = False
        self.root.after(10, self.poll)

    def close(self):
        self.stopping = True
        try:
            self.controls.cancel()
        finally:
            user32.UnhookWindowsHookEx(self.hook)



class MSG(C.Structure):
    _fields_ = [('hwnd', C.c_void_p), ('message', U32), ('wParam', UPTR),
                ('lParam', IPTR), ('time', U32), ('pt', POINT), ('lPrivate', U32)]

user32.SetTimer.argtypes = [C.c_void_p, UPTR, U32, C.c_void_p]
user32.SetTimer.restype = UPTR
user32.KillTimer.argtypes = [C.c_void_p, UPTR]
user32.GetMessageW.argtypes = [C.POINTER(MSG), C.c_void_p, U32, U32]
user32.GetMessageW.restype = C.c_int
user32.TranslateMessage.argtypes = [C.POINTER(MSG)]
user32.DispatchMessageW.argtypes = [C.POINTER(MSG)]
user32.DispatchMessageW.restype = IPTR
user32.PostThreadMessageW.argtypes = [U32, U32, UPTR, IPTR]
kernel32.GetCurrentThreadId.restype = U32


class InputService:
    def __init__(self, bindings):
        self.bindings = bindings
        self.callback = None
        self.ready = threading.Event()
        self.error = None
        self.thread_id = 0
        self.thread = threading.Thread(target=self.run, daemon=True, name='Bloodborne mouse controls')
        self.thread.start()
        if not self.ready.wait(5):
            raise RuntimeError('No se pudo iniciar el control de ratón integrado.')
        if self.error:
            raise RuntimeError(self.error)

    def after(self, _delay, callback):
        self.callback = callback

    def run(self):
        timer = 0
        bridge = None
        try:
            self.thread_id = kernel32.GetCurrentThreadId()
            bridge = Bridge(self)
            timer = user32.SetTimer(None, 0, 10, None)
            if not timer:
                raise C.WinError(C.get_last_error())
            self.ready.set()
            print('Controles PC integrados: activos (perfil DS3).', flush=True)
            msg = MSG()
            while True:
                result = user32.GetMessageW(C.byref(msg), None, 0, 0)
                if result == 0:
                    break
                if result < 0:
                    raise C.WinError(C.get_last_error())
                if msg.message == 0x0113 and self.callback:
                    self.callback()
                    if bridge.error:
                        raise RuntimeError(bridge.error)
                user32.TranslateMessage(C.byref(msg))
                user32.DispatchMessageW(C.byref(msg))
        except Exception as exc:
            self.error = str(exc)
            print('Controles PC integrados: ERROR: ' + self.error, file=sys.stderr, flush=True)
        finally:
            self.ready.set()
            if timer:
                user32.KillTimer(None, timer)
            if bridge:
                bridge.close()

    def stop(self):
        if self.thread.is_alive():
            user32.PostThreadMessageW(self.thread_id, 0x0012, 0, 0)
            self.thread.join(2)

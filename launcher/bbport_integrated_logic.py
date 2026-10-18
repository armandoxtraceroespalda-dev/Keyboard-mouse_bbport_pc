# SPDX-License-Identifier: GPL-2.0-or-later
"""Stateful mouse actions for the launcher-integrated PC control profile."""
import time


class ChordCapture:
    """Accumulate unique tokens until every physical input has been released."""
    def __init__(self):
        self.down = set()
        self.tokens = []

    def press(self, source, token):
        self.down.add(source)
        if token not in self.tokens:
            self.tokens.append(token)

    def release(self, source):
        self.down.discard(source)
        return bool(self.tokens) and not self.down

    def text(self):
        return ' + '.join(self.tokens)

DEFAULT_BINDINGS = {'light': ('left', False), 'heavy': ('left', True), 'firearm': ('right', False)}
VALID_BUTTONS = {'left', 'right', 'middle', 'side_back', 'side_forward'}


def decode_binding(text):
    if text == 'none':return None,False
    parts = text.split('+')
    shifted = len(parts) == 2 and parts[0] == 'shift'
    button = parts[-1]
    if button not in VALID_BUTTONS or (len(parts) != 1 and not shifted):
        raise ValueError('Combinación de ratón no válida: ' + text)
    return button, shifted


class Controls:
    def __init__(self, emit, clock=time.monotonic, bindings=None):
        self.emit = emit
        self.clock = clock
        self.bindings = dict(DEFAULT_BINDINGS if bindings is None else bindings)
        self.held = {}
        self.output = set()
        self.serial = 0

    def _sync(self):
        wanted = {entry[0] for entry in self.held.values()}
        for key in sorted(self.output - wanted):
            self.emit(key, False)
        for key in sorted(wanted - self.output):
            self.emit(key, True)
        self.output = wanted

    def press(self, button, shift=False):
        if button in self.held and self.held[button][2] is None:
            return
        # Exact Shift bindings win over unmodified bindings on the same button.
        matches = [(modified, action) for action, (source, modified) in self.bindings.items()
                   if source == button and (not modified or shift)]
        if not matches:
            return
        key = max(matches)[1]
        self.held[button] = (key, self.clock(), None)
        self._sync()

    def release(self, button):
        entry = self.held.get(button)
        if entry is not None:
            key, start, _ = entry
            self.held[button] = (key, start, max(self.clock(), start + .050))
            self.tick()

    def wheel(self, up, shift=False):
        key = ('right' if up else 'left') if shift else ('up' if up else 'down')
        self.serial += 1
        now = self.clock()
        self.held['wheel_' + str(self.serial)] = (key, now, now + .050)
        self._sync()

    def tick(self):
        now = self.clock()
        self.held = {k: v for k, v in self.held.items() if v[2] is None or v[2] > now}
        self._sync()

    def cancel(self):
        self.held.clear()
        self._sync()

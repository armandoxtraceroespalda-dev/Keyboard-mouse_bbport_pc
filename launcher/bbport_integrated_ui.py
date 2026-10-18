# SPDX-License-Identifier: GPL-2.0-or-later
"""Controls and GPU settings embedded in the original Bloodborne launcher."""
import os
import re
import shutil
import subprocess
import threading
from pathlib import Path
from bbport_integrated_logic import DEFAULT_BINDINGS, decode_binding, ChordCapture
try:
    import bbport_integrated_localization as localization
except ImportError:
    import integrated_localization as localization

VERSION = '1.5.27'
ENGLISH = {
 'Mostrar teclas de teclado y ratón en el juego':'Show keyboard and mouse prompts in-game',
 'Mostrar FPS':'Show FPS',
 'Menús de ajustes dentro del juego':'In-game tweak menus',
 'Añade Key Bindings, PC Controls, PC Enhancements y PC Camera al menú Sistema. Los cambios se aplican al iniciar el juego.':'Adds Key Bindings, PC Controls, PC Enhancements and PC Camera to the System menu. Changes apply when starting the game.',
 'El espejo del Sueño del Cazador abre el editor de apariencia':"Hunter's dream mirror opens the appearance editor",
 'El Altar de la Desesperación restablece el nivel y los atributos con la Piedra de Yharnam':'The Altar of Despair resets level and attributes with the Yharnam Stone',
 'Con la Piedra de Yharnam, el Altar de la Desesperación restablece el nivel y los atributos. Activa también los menús de ajustes dentro del juego.':'With the Yharnam Stone, the Altar of Despair resets level and attributes. Also enables the in-game tweak menus.',
 'Ajustes de jugabilidad':'Gameplay Tweaks',
 'Control':'Controller', 'Usar perfil Dark Souls 3 PC':'Use Dark Souls 3 PC profile',
 'Ataque ligero':'Light attack','Ataque fuerte / cargado':'Strong / charged attack','Arma de fuego / parry':'Firearm / parry',
 'Asignar…':'Set…','Borrar':'Clear','Objeto siguiente':'Next item','Objeto anterior':'Previous item',
 'Capturar…':'Capture…','Sin asignar':'Unassigned','Clic izquierdo':'Left click','Clic derecho':'Right click','Clic central':'Middle click',
 'Shift + clic izquierdo':'Shift + left click','Shift + clic derecho':'Shift + right click','Shift + clic central':'Shift + middle click',
 'Botón lateral atrás':'Back side button','Botón lateral adelante':'Forward side button','Shift + lateral atrás':'Shift + back side button','Shift + lateral adelante':'Shift + forward side button',
 'Carrera y cambios de dirección':'Sprint and direction changes','Velocidad al cambiar dirección':'Turning speed','Margen entre teclas WASD':'WASD key release delay',
 'Mantener intensidad completa al moverse en diagonal':'Keep full movement intensity on diagonals',
 'Recomendado: 900 °/s, 60 ms y diagonales a intensidad completa. El margen mantiene la última dirección brevemente después de soltar WASD.':'Recommended: 900 °/s, 60 ms, and full diagonal movement. The delay briefly retains the last direction after releasing WASD.',
 'Sensibilidad de cámara directa':'Direct camera sensitivity','Guardar ajustes de controles':'Save control settings',
 'Opciones de PC dentro del juego':'In-game PC options','Usar ratón en los menús del juego':'Use mouse in game menus',
 'Ajustes guardados. Se aplican al iniciar el juego de nuevo.':'Settings saved. They apply the next time the game starts.',
 'Ratón activo: clic izquierdo selecciona, derecho vuelve. WASD navega en el título; en partida mueve sin navegar por el inventario.':'With menu mouse enabled: left click selects and right click goes back. WASD navigates the title screen; in gameplay it moves the character without navigating the inventory.',
 'Sin menús de ratón: E/Enter confirma y Espacio/Retroceso vuelve. Rueda arriba crea balas de sangre y consume vida. Los cambios se aplican al iniciar el juego.':'Without menu mouse: E/Enter confirms and Space/Backspace goes back. Wheel up creates Blood Bullets and consumes HP. Changes apply when the game starts.',
 'Control · asignaciones del mando':'Controller · gamepad bindings','Keyboard and mouse · perfil Dark Souls 3 PC':'Keyboard and mouse · Dark Souls 3 PC profile',
 'Keyboard and mouse · combinaciones de ataque desactivadas':'Keyboard and mouse · attack combinations disabled',
 'Elige Control o Keyboard and mouse. Los ajustes se guardan para el siguiente inicio.':'Choose Controller or Keyboard and mouse. Settings are saved for the next launch.',
 'Controles de PC':'PC controls','Cierra el juego antes de cambiar o restaurar el perfil.':'Close the game before changing or restoring the profile.',
 'Asigna una combinación diferente a cada ataque.':'Assign a different combination to each attack.',
 'Pulsa la combinación y suelta todo…':'Press the combination, then release everything…',
 'Perfil DS3 aplicado. Inicia con PLAY o Play Bloodborne.exe.\n\nLa copia de tus controles anteriores está en ':'DS3 profile applied. Start with PLAY or Play Bloodborne.exe.\n\nYour previous controls are backed up in ',
 'Espejo del Sueño: cambiar apariencia':"Dream mirror: change appearance",
 'Automática':'Automatic','Tarjeta gráfica':'Graphics card','La selección se guarda para PLAY y Play Bloodborne.exe.':'The selection is saved for PLAY and Play Bloodborne.exe.',
 '✓ Tarjeta gráfica: ':'✓ Graphics card: ','• GPU automática':'• Automatic GPU','• Comprobando GPU seleccionada…':'• Checking selected GPU…',
}
from bbport_integrated_profile import PROFILE_TEXT
BINDING_LABELS = {'Sin asignar': 'none',
    'Clic central': 'middle', 'Shift + clic central': 'shift+middle',
    'Clic izquierdo': 'left', 'Shift + clic izquierdo': 'shift+left',
    'Clic derecho': 'right', 'Shift + clic derecho': 'shift+right',
    'Botón lateral atrás': 'side_back', 'Shift + lateral atrás': 'shift+side_back',
    'Botón lateral adelante': 'side_forward', 'Shift + lateral adelante': 'shift+side_forward',
}
REVERSE_LABELS = {value: key for key, value in BINDING_LABELS.items()}
ACTION_OUTPUTS = {'r1': 'light', 'r2': 'heavy', 'l2': 'firearm'}
NATIVE_ACTIONS = dict(zip(
    'axis_left_y_minus axis_left_y_plus axis_left_x_minus axis_left_x_plus leftjoystick_halfmode circle r3 r1 r2 l1 l2 triangle square cross pad_down pad_right pad_left pad_up touchpad_left touchpad_right axis_right_y_minus axis_right_y_plus axis_right_x_minus axis_right_x_plus options l3'.split(),
    'move_forward move_back move_left move_right walk roll lock_on attack strong_attack transform firearm blood_vial use_item interact switch_item switch_right switch_left dpad_up gestures personal_effects look_up look_down look_left look_right menu l3'.split()))
NATIVE_ACTIONS['previous_item']='previous_item'
DEFAULTS = {'pc_controls_mode': 'standard', 'pc_mouse_light': 'left',
            'pc_mouse_heavy': 'shift+left', 'pc_mouse_firearm': 'right',
            'pc_gpu_id': os.environ.get('BB_GPU_ID', '-1')}


def read_ini(path):
    result = {}
    try:
        lines = Path(path).read_text(encoding='utf-8').splitlines()
    except OSError:
        return result
    for line in lines:
        if '=' in line and not line.lstrip().startswith(('#', ';')):
            key, value = line.split('=', 1)
            result[key.strip()] = value.strip()
    return result


def update_ini(path, values, remove_prefixes=()):
    path = Path(path)
    try:
        lines = path.read_text(encoding='utf-8').splitlines()
    except FileNotFoundError:
        lines = []
    out, seen = [], set()
    for line in lines:
        key = line.split('=', 1)[0].strip() if '=' in line and not line.lstrip().startswith(('#', ';')) else None
        if key and key.startswith(remove_prefixes):
            continue
        if key in values:
            if key not in seen:
                out.append(key + '=' + str(values[key]))
                seen.add(key)
        else:
            out.append(line)
    out.extend(key + '=' + str(value) for key, value in values.items() if key not in seen)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + '.pc-controls.tmp')
    temporary.write_text('\n'.join(out) + '\n', encoding='utf-8')
    os.replace(temporary, path)


def read_config(path):
    config = {**DEFAULTS, **read_ini(path)}
    for action, fallback in DEFAULT_BINDINGS.items():
        key = 'pc_mouse_' + action
        try:
            decode_binding(config[key])
        except (ValueError, AttributeError):
            config[key] = ('shift+' if fallback[1] else '') + fallback[0]
    return config


def bindings_from_config(config):
    return {key: decode_binding(config['pc_mouse_' + key]) for key in DEFAULT_BINDINGS}


def scan_gpus(port):
    devices = []
    executable = next((Path(port)/folder/'bb-gpu-capabilities.exe' for folder in ('bin','out') if (Path(port)/folder/'bb-gpu-capabilities.exe').is_file()), Path(port)/'bin'/'bb-gpu-capabilities.exe')
    if not executable.is_file():
        return devices
    for index in range(8):
        env = dict(os.environ, BB_GPU_ID=str(index))
        try:
            result = subprocess.run([str(executable)], cwd=str(port), env=env,
                                    capture_output=True, timeout=8,
                                    creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
        except (OSError, subprocess.TimeoutExpired):
            break
        text = (result.stdout + result.stderr).decode('utf-8', errors='replace')
        if 'outside the device list' in text:
            break
        match = re.search(r'GPU scene scaling: (.+?)(?: supports live presets| lacks blit support)', text)
        if not match:
            break
        devices.append((index, match.group(1)))
    return devices


def install(ns):
    def tr(text):
        if ns.get('LANG','es').lower().startswith('es'):return text
        english=ENGLISH.get(text,text)
        index=localization.language_index(ns.get('LANG','en'))
        if index is not None and english in localization.TRANSLATIONS:return localization.TRANSLATIONS[english][index]
        return ns.get('_',lambda value:value)(english)
    # Keep this navigation/page name identical in every language.
    original_translate = ns.get('_')
    if original_translate is not None:
        def translate(text, *args, **kwargs):
            return 'Tweaks & Cheats' if text == 'Cheats' else original_translate(text, *args, **kwargs)
        ns['_'] = translate
    Launcher = ns['Launcher']
    original_style = getattr(Launcher, 'style', None)
    original_init = Launcher.__init__
    original_controls = Launcher.build_controls
    original_cheats = Launcher.build_cheats
    original_graphics = Launcher.build_graphics
    original_display = Launcher.build_display
    original_collect = Launcher.collect
    original_refresh = Launcher.refresh_remap_row
    original_capture = Launcher.start_remap_capture
    original_clear = Launcher.clear_remap
    original_play = Launcher.play
    original_drain = getattr(Launcher,'drain_output',None)
    original_environment = ns['game_environment']
    original_run = ns['run_role']

    def style(self):
        original_style(self)
        # Same TButton geometry and palette, with an explicit selected mode.
        styles = self.ttk.Style(self.root)
        styles.configure('DeviceSelected.TButton', background=ns['BLOOD'], foreground=ns['TEXT'],
                         bordercolor=ns['GOLD'])
        styles.map('DeviceSelected.TButton',
                   background=[('disabled', ns['PANEL']), ('pressed', ns['BLOOD']), ('active', ns['BLOOD_HI'])],
                   foreground=[('disabled', ns['MUTED']), ('!disabled', ns['TEXT'])],
                   bordercolor=[('!disabled', ns['GOLD'])])

    def input_path(self):
        return Path(ns['input_ini_path'](ns['ini_path']()))

    def reload_inputs(self):
        self.remap_bindings, self.remap_lines, self.remap_toggle_key, self.remap_reload_key, self.mouse_settings = ns['load_input_ini'](input_path(self))
        self.remap_edits = {}
        self.mouse_edited = False
        for output in getattr(self, 'remap_rows', {}):
            self.refresh_remap_row(output)

    def rebuild_controls(self):
        for var, token in getattr(self, '_pc_slider_traces', []):
            var.trace_remove('write', token)
        self._pc_slider_traces = []
        self.pages['controls'].destroy()
        self.build_controls()
        self.show('controls')

    def ensure_vars(self):
        if hasattr(self, '_pc_vars'):
            return
        settings = read_ini(input_path(self))
        tk = self.tk
        self._pc_vars = {}
        for action in DEFAULT_BINDINGS:
            value = self._pc_config['pc_mouse_' + action]
            self._pc_vars[action] = tk.StringVar(value=REVERSE_LABELS[value])
        self._pc_binding_display={a:tk.StringVar(value=tr(self._pc_vars[a].get())) for a in DEFAULT_BINDINGS}
        for action in DEFAULT_BINDINGS:
            self._pc_vars[action].trace_add('write',lambda *_args,a=action:self._pc_binding_display[a].set(tr(self._pc_vars[a].get())))
        self._pc_vars['turn'] = tk.DoubleVar(value=float(settings.get('stick_turn_rate', 900)))
        self._pc_vars['gap'] = tk.DoubleVar(value=float(settings.get('stick_release_delay_ms', 60)))
        self._pc_vars['square'] = tk.BooleanVar(value=settings.get('stick_circular', 'off') != 'on')
        self._pc_vars['sensitivity'] = tk.DoubleVar(value=float(settings.get('mouse_camera_sensitivity', 1)))
        self._pc_vars['options'] = tk.BooleanVar(value=self._pc_config.get('pc_game_options','1') == '1')
        self._pc_vars['mirror'] = tk.BooleanVar(value=self._pc_config.get('pc_game_mirror','0') == '1')
        self._pc_vars['prompts'] = tk.BooleanVar(value=self._pc_config.get('pc_key_prompts','1')=='1' and self._pc_config.get('pc_keyboard_mouse','1')=='1')
        self._pc_vars['rebirth'] = tk.BooleanVar(value=self._pc_config.get('pc_game_rebirth','0') == '1')
        self._pc_vars['menus'] = tk.BooleanVar(value=True)
        self._pc_device = tk.StringVar(value='controller' if self._pc_config.get('pc_keyboard_mouse','1')=='0' else 'kbm')
        active=self._pc_config.get('pc_kbm_profile_ready',self._pc_config.get('pc_ds3_profile','1'))=='1'
        self._pc_ds3 = tk.BooleanVar(value=active)
        self._pc_status = tk.StringVar()
        self._pc_gpu_choices = {tr('Automática'): -1}
        gpu_id = int(self._pc_config.get('pc_gpu_id', '-1'))
        gpu_label = tr('Automática') if gpu_id < 0 else 'GPU ' + str(gpu_id)
        self._pc_gpu_choices[gpu_label] = gpu_id
        self._pc_gpu = tk.StringVar(value=gpu_label)
        self._pc_devices = {}

    def update_status(self):
        mode=self._pc_device.get()=='kbm'
        self._pc_status.set(tr('Control · asignaciones del mando') if not mode else tr('Keyboard and mouse · perfil Dark Souls 3 PC') if self._pc_ds3.get() else tr('Keyboard and mouse · combinaciones de ataque desactivadas'))

    def validate(self, show=True):
        if self._pc_device.get()!='kbm' or not self._pc_ds3.get():return True
        values = [self._pc_vars[action].get() for action in DEFAULT_BINDINGS]
        active=[v for v in values if v != 'Sin asignar']
        if len(set(active)) != len(active):
            if show:
                self.messagebox.showerror(tr('Controles de PC'), tr('Asigna una combinación diferente a cada ataque.'))
            return False
        return True

    def blocked(self):
        if self.process:
            self.messagebox.showinfo(tr('Controles de PC'), tr('Cierra el juego antes de cambiar o restaurar el perfil.'))
            return True
        return False

    def apply_profile(self, notify=True, rebuild=True):
        if blocked(self):
            return
        # Applying the preset must recover from invalid current assignments.
        # Do not validate stale/custom mouse combinations before resetting them.
        ensure_vars(self)
        self._pc_device.set('kbm');self._pc_ds3.set(True)
        self._pc_vars['options'].set(True)
        for action, (button, shifted) in DEFAULT_BINDINGS.items():
            value = ('shift+' if shifted else '') + button
            self._pc_vars[action].set(REVERSE_LABELS[value])
        original_collect(self)
        target = input_path(self)
        backup = target.with_name(target.stem + '.before-integrated' + target.suffix)
        if self._pc_config.get('pc_controls_mode') != 'ds3' and not backup.exists():
            if target.exists():
                shutil.copy2(target, backup)
            else:
                from bbport_input_config import DEFAULT_INPUT_INI
                backup.write_text(DEFAULT_INPUT_INI, encoding='utf-8')
        target.write_text(PROFILE_TEXT, encoding='utf-8')
        # Explicit profile application replaces in-game overrides too. Ordinary
        # startup/Save must leave the player's later in-game rebinds intact.
        native_defaults = {'pc_bind.strong_modifier': 'Left Shift',
                           'pc_bind.switch_item':'Down, WheelDown','pc_bind.previous_item':'WheelUp',
                           'pc_bind.confirm': 'Return', 'pc_bind.back': 'Backspace',
                           'pc_keyboard_mouse': '1', 'pc_kbm_profile_ready': '1',
                           'pc_ds3_profile':'1','pc_mouse_camera':'1'}
        update_ini(ns['ini_path'](), native_defaults, remove_prefixes=('pc_bind.',))
        self._pc_config = {key: value for key, value in self._pc_config.items()
                           if not key.startswith('pc_bind.')}
        self._pc_config.update(native_defaults)
        self._pc_config['pc_controls_mode'] = 'ds3'
        # Reset the sprint controls to the corrected preset on explicit application.
        self._pc_vars['turn'].set(900)
        self._pc_vars['gap'].set(60)
        self._pc_vars['square'].set(True)
        self._pc_vars['sensitivity'].set(1)
        reload_inputs(self)
        save_values(self)
        update_status(self)
        if rebuild:rebuild_controls(self)
        if notify:self.messagebox.showinfo(tr('Controles de PC'), tr('Perfil DS3 aplicado. Inicia con PLAY o Play Bloodborne.exe.\n\nLa copia de tus controles anteriores está en ') + backup.name + '.')

    def restore_profile(self, rebuild=True):
        if blocked(self):
            return
        target = input_path(self)
        backup = target.with_name(target.stem + '.before-integrated' + target.suffix)
        if self._pc_config.get('pc_controls_mode') == 'ds3' and backup.is_file():
            shutil.copy2(backup, target)
        self._pc_config['pc_controls_mode'] = 'standard'
        self._pc_config={k:v for k,v in self._pc_config.items() if not k.startswith('pc_bind.')}
        update_ini(ns['ini_path'](), {'pc_controls_mode': 'standard','pc_keyboard_mouse':'1','pc_kbm_profile_ready':'0'},remove_prefixes=('pc_bind.',))
        reload_inputs(self)
        settings = read_ini(target)
        self._pc_vars['turn'].set(float(settings.get('stick_turn_rate', 900)))
        self._pc_vars['gap'].set(float(settings.get('stick_release_delay_ms', 60)))
        self._pc_vars['square'].set(settings.get('stick_circular', 'off') != 'on')
        update_status(self)
        if rebuild:rebuild_controls(self)

    def save_values(self):
        ensure_vars(self)
        if not validate(self):
            return False
        if self._pc_vars['rebirth'].get() or self._pc_vars['prompts'].get():self._pc_vars['options'].set(True)
        mouse_look(self,self._pc_device.get()=='kbm')
        fields = {'pc_controls_mode': self._pc_config.get('pc_controls_mode', 'standard'),
                  'pc_gpu_id': self._pc_gpu_choices.get(self._pc_gpu.get(), -1),
                  'pc_game_mouse_menu': '1',
                  'pc_key_prompts': '1' if self._pc_vars['prompts'].get() else '0',
                  'pc_game_rebirth': '1' if self._pc_vars['rebirth'].get() else '0',
                  'pc_game_mirror': '1' if self._pc_vars['mirror'].get() else '0',
                  'pc_game_options': '1' if self._pc_vars['options'].get() else '0'}
        fields.update(pc_keyboard_mouse='1' if self._pc_device.get()=='kbm' else '0',
                      pc_ds3_profile='1' if self._pc_ds3.get() else '0',
                      pc_kbm_profile_ready='1' if self._pc_ds3.get() else '0')
        fields['pc_controls_mode']='ds3' if self._pc_device.get()=='kbm' and self._pc_ds3.get() else 'standard'
        for action in DEFAULT_BINDINGS:
            fields['pc_mouse_' + action] = BINDING_LABELS[self._pc_vars[action].get()]
        update_ini(ns['ini_path'](), fields)
        self._pc_config.update({key: str(value) for key, value in fields.items()})
        # Native controls implement sprint shaping. The embedded mouse bridge only
        # translates attacks/wheel, and does not emulate or re-press Space/WASD.
        target = input_path(self)
        if not target.is_file():
            from bbport_input_config import DEFAULT_INPUT_INI
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(DEFAULT_INPUT_INI, encoding='utf-8')
        update_ini(target, {
            'stick_socd': 'last',
            'stick_turn_rate': max(0, min(5000, round(self._pc_vars['turn'].get()))),
            'stick_release_delay_ms': max(0, min(200, round(self._pc_vars['gap'].get()))),
            'stick_circular': 'off' if self._pc_vars['square'].get() else 'on',
            'mouse_camera_sensitivity': max(.05, min(20, self._pc_vars['sensitivity'].get())),
        })
        self.ini, self.ini_lines = ns['load_ini']()
        for output in ACTION_OUTPUTS:
            if output in getattr(self, 'remap_rows', {}):
                self.refresh_remap_row(output)
        return True

    def mouse_look(self, enabled):
        self.mouse_settings['enabled']=enabled;self.mouse_settings['stick']='right'
        update_ini(input_path(self),{'mouse_to_joystick':'right' if enabled else 'none','mouse_camera':'direct'})
        self._pc_config['pc_mouse_camera']='1' if enabled else '0'
        update_ini(ns['ini_path'](),{'pc_mouse_camera':self._pc_config['pc_mouse_camera']})
        self.mouse_edited=False

    def change_device(self):
        previous='controller' if self._pc_config.get('pc_keyboard_mouse','1')=='0' else 'kbm'
        if blocked(self):self._pc_device.set(previous);return
        if self._pc_device.get()==previous:return
        self._pc_vars['options'].set(True)
        self._pc_vars['prompts'].set(self._pc_device.get()=='kbm')
        original_collect(self)
        self._pc_config['pc_keyboard_mouse']='1' if self._pc_device.get()=='kbm' else '0'
        mouse_look(self,self._pc_device.get()=='kbm')
        save_values(self)
        reload_inputs(self);update_status(self);rebuild_controls(self)

    def toggle_ds3(self):
        if blocked(self):self._pc_ds3.set(self._pc_config.get('pc_ds3_profile','1')=='1');return
        if self._pc_ds3.get():apply_profile(self,notify=False,rebuild=False)
        else:
            restore_profile(self,rebuild=False)
            for a,value in [('light','left'),('heavy','none'),('firearm','right')]:self._pc_vars[a].set(REVERSE_LABELS[value])
            update_ini(ns['ini_path'](),{'pc_bind.strong_modifier':'none'})
            self._pc_config['pc_bind.strong_modifier']='none'
            mouse_look(self,True);save_values(self)
        reload_inputs(self);update_status(self);rebuild_controls(self)

    def refresh_mode_widgets(self):
        kbm=self._pc_device.get()=='kbm';active=kbm and self._pc_ds3.get()
        for widget in getattr(self,'_pc_attack_widgets',[]):widget.configure(state='readonly' if active and widget in self._pc_attack_combos else 'normal' if active else 'disabled')
        for output,label in getattr(self,'remap_rows',{}).items():
            if hasattr(label,'grid_info'):
                info=label.grid_info()
                for column in (2,3):
                    for button in label.master.grid_slaves(row=info['row'],column=column):button.configure(state='normal' if kbm and (output not in ACTION_OUTPUTS or active) else 'disabled')
        for output,label in getattr(self,'_pc_attack_labels',{}).items():
            chord=self._pc_config.get('pc_bind.'+NATIVE_ACTIONS[output],'')
            label.configure(text=chord if chord and chord!='none' else tr('Sin asignar') if chord=='none' else tr(self._pc_vars[ACTION_OUTPUTS[output]].get()) if output in ACTION_OUTPUTS else 'WheelUp' if output=='previous_item' else 'Down, WheelDown')

    def mode_header(self,parent):
        ensure_vars(self)
        header=self.ttk.Frame(parent);header.grid(row=self.next_row(parent),column=0,columnspan=4,sticky='ew',pady=(4,12))
        toggle=self.ttk.Frame(header);toggle.grid(row=0,column=0,sticky='w')
        for text,value in [('Control','controller'),('Keyboard and mouse','kbm')]:
            def select_device(value=value):
                if self._pc_device.get() == value:return
                self._pc_device.set(value)
                change_device(self)
            self.ttk.Button(toggle,text=tr(text),
                            style='DeviceSelected.TButton' if self._pc_device.get()==value else 'TButton',
                            command=select_device).pack(side='left',padx=(0,8))
        self._pc_attack_widgets=[];self._pc_attack_combos=[];self._pc_attack_labels={}
        if self._pc_device.get()=='kbm':
            block=self.ttk.Frame(header);block.grid(row=1,column=0,sticky='ew',pady=(8,0))
            self.ttk.Checkbutton(block,text=tr('Usar perfil Dark Souls 3 PC'),variable=self._pc_ds3,command=lambda:toggle_ds3(self)).grid(row=0,column=0,columnspan=4,sticky='w',pady=(0,6))
            if self._pc_ds3.get():
                # Match build_controls.add_row: caption, muted assignment, Set, Clear.
                for row,(output,title) in enumerate([('r1','Ataque ligero'),('r2','Ataque fuerte / cargado'),('l2','Arma de fuego / parry'),('pad_down','Objeto siguiente'),('previous_item','Objeto anterior')],1):
                    self.ttk.Label(block,text=tr(title)).grid(row=row,column=0,sticky='w',padx=(0,18),pady=4)
                    label=self.ttk.Label(block,width=14,style='Muted.TLabel')
                    label.grid(row=row,column=1,sticky='w',pady=4)
                    self._pc_attack_labels[output]=label
                    button=self.ttk.Button(block,text=tr('Asignar…'))
                    button.configure(command=lambda o=output,l=label,b=button:self.start_remap_capture(o,l,b))
                    button.grid(row=row,column=2,sticky='w',padx=(10,4),pady=4)
                    clear_button=self.ttk.Button(block,text=tr('Borrar'),command=lambda o=output,l=label:self.clear_remap(o,l))
                    clear_button.grid(row=row,column=3,sticky='w',pady=4)
                    self._pc_attack_widgets.extend((button,clear_button))
        self.ttk.Label(header,textvariable=self._pc_status,style='Muted.TLabel').grid(row=2,column=0,sticky='w',pady=(8,0))
        update_status(self)

    def controls_section(self, parent):
        ensure_vars(self)
        ttk = self.ttk
        self.section(parent, tr('Carrera y cambios de dirección'))

        def slider(key, title, low, high, unit, decimals=0):
            holder = ttk.Frame(parent)
            var = self._pc_vars[key]
            ttk.Scale(holder, from_=low, to=high, variable=var, length=self.px(220)).pack(side='left')
            label = ttk.Label(holder, width=11)
            label.pack(side='left', padx=8)
            def refresh(*_args):
                label.configure(text=f'{var.get():.{decimals}f}{unit}')
            token = var.trace_add('write', refresh)
            self._pc_slider_traces.append((var, token))
            refresh()
            self.row(parent, tr(title), holder)

        slider('turn', 'Velocidad al cambiar dirección', 300, 1800, ' °/s')
        slider('gap', 'Margen entre teclas WASD', 0, 120, ' ms')
        ttk.Checkbutton(parent, text=tr('Mantener intensidad completa al moverse en diagonal'),
                        variable=self._pc_vars['square']).grid(row=self.next_row(parent), column=0, columnspan=4, sticky='w', pady=4)
        self.note(parent, tr('Recomendado: 900 °/s, 60 ms y diagonales a intensidad completa. '
                  'El margen mantiene la última dirección brevemente después de soltar WASD.'), top=5)
        slider('sensitivity', 'Sensibilidad de cámara directa', .1, 5, '', 2)

        def save():
            if validate(self):
                self.collect()
                self._pc_status.set(tr('Ajustes guardados. Se aplican al iniciar el juego de nuevo.'))
        ttk.Button(parent, text=tr('Guardar ajustes de controles'), command=save).grid(
            row=self.next_row(parent), column=0, columnspan=2, sticky='w', pady=(8, 0))
        self.note(parent, tr('Ratón activo: clic izquierdo selecciona, derecho vuelve. WASD navega en el título; en partida mueve sin navegar por el inventario.'), top=4)
        update_status(self)

    def controls(self):
        old = self.scrolled_page
        parents = []
        def make_page(*args, **kwargs):
            if len(args)>=3:args=(*args[:2],tr('Elige Control o Keyboard and mouse. Los ajustes se guardan para el siguiente inicio.'),*args[3:])
            parent = old(*args, **kwargs)
            parents.append(parent)
            mode_header(self,parent)
            return parent
        self.scrolled_page = make_page
        try:
            original_controls(self)
        finally:
            self.scrolled_page = old
        # The base page ends with Mouse look. Append our controls after its
        # complete widgets/notes, instead of inserting ahead of the bindings.
        if parents:
            controls_section(self, parents[-1])
            refresh_mode_widgets(self)

    def cheats(self):
        old_page, old_section = self.scrolled_page, self.section
        def make_page(*args, **kwargs):
            args = (*args[:1], 'Tweaks & Cheats', *args[2:])
            return old_page(*args, **kwargs)
        def add_tweaks(parent):
            ensure_vars(self)
            def options_changed():
                if not self._pc_vars['options'].get():
                    self._pc_vars['rebirth'].set(False);self._pc_vars['prompts'].set(False)
            def rebirth_changed():
                if self._pc_vars['rebirth'].get() or self._pc_vars['prompts'].get():self._pc_vars['options'].set(True)
            self.ttk.Checkbutton(parent,text=tr('Menús de ajustes dentro del juego'),
                variable=self._pc_vars['options'],command=options_changed).grid(
                    row=self.next_row(parent),column=0,columnspan=4,sticky='w',pady=5)
            def prompts_changed():
                if self._pc_vars['prompts'].get():self._pc_vars['options'].set(True)
            self.ttk.Checkbutton(parent,text=tr('Mostrar teclas de teclado y ratón en el juego'),
                variable=self._pc_vars['prompts'],command=prompts_changed).grid(row=self.next_row(parent),column=0,columnspan=4,sticky='w',pady=5)
            self.ttk.Checkbutton(parent,text=tr('El espejo del Sueño del Cazador abre el editor de apariencia'),
                variable=self._pc_vars['mirror']).grid(row=self.next_row(parent),column=0,columnspan=4,sticky='w',pady=5)
            self.ttk.Checkbutton(parent,text=tr('El Altar de la Desesperación restablece el nivel y los atributos con la Piedra de Yharnam'),variable=self._pc_vars['rebirth'],
                command=rebirth_changed).grid(row=self.next_row(parent),column=0,columnspan=4,sticky='w',pady=5)
        def section(parent, text, **kwargs):
            # The base section is translated before reaching us.
            gameplay=ns.get('_',lambda text,*a:text)('Gameplay tweaks','Изменения игрового процесса')
            result=old_section(parent,tr('Ajustes de jugabilidad') if text==gameplay else 'Cheats' if text=='Tweaks & Cheats' else text,**kwargs)
            if text==gameplay:add_tweaks(parent)
            return result
        self.scrolled_page,self.section=make_page,section
        try:original_cheats(self)
        finally:self.scrolled_page,self.section=old_page,old_section

    def graphics(self):
        old = self.scrolled_page
        old_check = self.check
        def check(parent,key,*args,**kwargs):
            if key=='show_fps':return None
            return old_check(parent,key,*args,**kwargs)
        self.check = check
        def make_page(*args, **kwargs):
            parent = old(*args, **kwargs)
            ensure_vars(self)
            self.section(parent, tr('Tarjeta gráfica'), top=4)
            self._pc_gpu_box = self.ttk.Combobox(parent, textvariable=self._pc_gpu, state='readonly',
                                                values=list(self._pc_gpu_choices), width=48)
            self.row(parent, tr('GPU Vulkan'), self._pc_gpu_box)
            self.note(parent, tr('La selección se guarda para PLAY y Play Bloodborne.exe.'), top=3)
            return parent
        self.scrolled_page = make_page
        try:
            original_graphics(self)
        finally:
            self.scrolled_page = old
            self.check = old_check

    def display(self):
        old_section = self.section
        def section(parent,text,**kwargs):
            frame_rate=ns.get('_',lambda text,*a:text)('Frame rate','Частота кадров')
            result=old_section(parent,text,**kwargs)
            if text==frame_rate:self.check(parent,'show_fps','ini',tr('Mostrar FPS'))
            return result
        self.section=section
        try:original_display(self)
        finally:self.section=old_section

    def init(self, *args, **kwargs):
        self._pc_config = read_config(ns['ini_path']())
        if self._pc_config.get('pc_key_prompts_native')!='1':
            migration={'pc_key_prompts_native':'1','pc_key_prompts':'1' if self._pc_config.get('pc_keyboard_mouse','1')=='1' else '0'}
            update_ini(ns['ini_path'](),migration);self._pc_config.update(migration)
        first_profile=not any(k in self._pc_config for k in ('pc_ds3_profile','pc_kbm_profile_ready')) and self._pc_config.get('pc_controls_mode')!='ds3' and self._pc_config.get('pc_keyboard_mouse','1')!='0'
        self._pc_slider_traces = []
        self._pc_auto_gpu = 'pc_gpu_id' not in read_ini(ns['ini_path']())
        original_init(self, *args, **kwargs)
        if first_profile:apply_profile(self,notify=False)
        self._pc_gpu.trace_add('write', lambda *_args: self.detect_gpu())
        def detect():
            devices = scan_gpus(ns['PORT_DIR'])
            def done():
                selected_id = int(self._pc_config.get('pc_gpu_id', '-1'))
                labels = {tr('Automática'): -1}
                chosen = None
                for index, name in devices:
                    label = f'{index} · {name}'
                    labels[label] = index
                    if index == selected_id:
                        chosen = label
                    elif selected_id == -1 and self._pc_auto_gpu and '4090' in name:
                        chosen = label
                if not devices:
                    labels.update({f'GPU {index}': index for index in range(3)})
                    chosen = tr('Automática') if selected_id < 0 else f'GPU {selected_id}'
                self._pc_gpu_choices = labels
                self._pc_devices = dict(devices)
                self._pc_gpu_box.configure(values=list(labels))
                if chosen:
                    self._pc_gpu.set(chosen)
                else:
                    self.detect_gpu()
            self.ui_calls.put(done)
        threading.Thread(target=detect, daemon=True).start()

    def collect(self):
        # Keep fields written by an in-game PC menu while this launcher was
        # open; the base writer otherwise preserves a stale copy of its lines.
        if hasattr(self,'ini_lines'):
            _fresh,self.ini_lines=ns['load_ini']()
        original_collect(self)
        if hasattr(self, '_pc_vars'):
            save_values(self)

    def detect_gpu(self):
        ensure_vars(self)
        selected = self._pc_gpu_choices.get(self._pc_gpu.get(), -1)
        known = self._pc_devices.get(selected)
        if known:
            self.gpu_text = tr('✓ Tarjeta gráfica: ') + known
            self.ui_calls.put(self.refresh_status)
            return
        self.gpu_text = tr('• Comprobando GPU seleccionada…')
        self.ui_calls.put(self.refresh_status)
        def query():
            env = dict(os.environ)
            if selected >= 0:
                env['BB_GPU_ID'] = str(selected)
            else:
                env.pop('BB_GPU_ID', None)
            executable = next((Path(ns['PORT_DIR'])/folder/'bb-gpu-capabilities.exe' for folder in ('bin','out') if (Path(ns['PORT_DIR'])/folder/'bb-gpu-capabilities.exe').is_file()), Path(ns['PORT_DIR'])/'bin'/'bb-gpu-capabilities.exe')
            text = '• GPU ' + str(selected) if selected >= 0 else tr('• GPU automática')
            try:
                result = subprocess.run([str(executable)], capture_output=True, env=env,
                    creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0), timeout=15)
                raw = (result.stdout + result.stderr).decode('utf-8', errors='replace')
                match = re.search(r'GPU scene scaling: (.+?)(?: supports live presets| lacks blit support)', raw)
                if match:
                    text = tr('✓ Tarjeta gráfica: ') + match.group(1)
            except (OSError, subprocess.TimeoutExpired):
                pass
            def publish():
                # A late response for a previous selection must not overwrite it.
                if self._pc_gpu_choices.get(self._pc_gpu.get(), -1) == selected:
                    self.gpu_text = text
                    self.refresh_status()
            self.ui_calls.put(publish)
        threading.Thread(target=query, daemon=True).start()

    def refresh(self, output):
        if output in getattr(self,'_pc_attack_labels',{}):refresh_mode_widgets(self)
        if output not in self.remap_rows:return
        if self._pc_device.get()=='controller':
            names={'cross':'Cross (X)','circle':'Circle','square':'Square','triangle':'Triangle','l1':'L1','r1':'R1','l2':'L2','r2':'R2','l3':'L3','r3':'R3','options':'OPTIONS','pad_up':'D-pad up','pad_down':'D-pad down','pad_left':'D-pad left','pad_right':'D-pad right','touchpad_left':'Touchpad left','touchpad_right':'Touchpad right'}
            text=names.get(output,output.replace('axis_left','Left stick').replace('axis_right','Right stick').replace('_',' '))
            self.remap_rows[output].configure(text=text)
            return
        chord=self._pc_config.get('pc_bind.'+NATIVE_ACTIONS.get(output,''),'')
        if chord and chord!='none' and output in self.remap_rows:
            self.remap_rows[output].configure(text=chord)
            return
        if self._pc_config.get('pc_controls_mode') == 'ds3' and output in ACTION_OUTPUTS:
            self.remap_rows[output].configure(text=tr(self._pc_vars[ACTION_OUTPUTS[output]].get()))
        else:
            original_refresh(self, output)

    def capture(self, output, key_label, set_button):
        if self._pc_device.get()!='kbm' or (output in ACTION_OUTPUTS and not self._pc_ds3.get()):return
        if output not in NATIVE_ACTIONS:
            return original_capture(self,output,key_label,set_button)
        if blocked(self):return
        state=ChordCapture();legacy={};closed=False;ready=False
        aliases={'lshift':'Left Shift','rshift':'Right Shift','lctrl':'Left Ctrl','rctrl':'Right Ctrl',
                 'lalt':'Left Alt','ralt':'Right Alt','enter':'Return','space':'Space','esc':'Escape',
                 'backspace':'Backspace','leftbutton':'Mouse1','rightbutton':'Mouse2','middlebutton':'Mouse3',
                 'sidebuttonback':'Mouse4','sidebuttonforward':'Mouse5','mousewheelup':'WheelUp','mousewheeldown':'WheelDown',
                 'pgup':'PageUp','pgdown':'PageDown','kpenter':'Keypad Enter'}
        aliases.update({'lmeta':'Left GUI','rmeta':'Right GUI','grave':'`','minus':'-','equals':'=',
                        'lbracket':'[','rbracket':']','backslash':'\\','semicolon':';','apostrophe':"'",'comma':',','period':'.','slash':'/'})
        aliases.update({'kp'+str(i):'Keypad '+str(i) for i in range(10)})
        aliases.update({'pausebreak':'Pause','kpperiod':'Keypad .','kpcomma':'Keypad Comma','kpslash':'Keypad /',
                        'kpasterisk':'Keypad *','kpminus':'Keypad -','kpplus':'Keypad Plus','kpequals':'Keypad =',
                        'comma':'Comma','lwin':'Left GUI','rwin':'Right GUI'})
        key_label.configure(text=tr('Pulsa la combinación y suelta todo…'))
        set_button.configure(state='disabled');self.root.grab_set()
        events=('<KeyPress>','<KeyRelease>','<ButtonPress>','<ButtonRelease>','<MouseWheel>','<Shift-MouseWheel>','<FocusOut>')
        def finish(commit):
            nonlocal closed
            if closed:return
            closed=True
            for event in events:self.root.unbind_all(event)
            self.root.bind_all('<MouseWheel>',self.wheel);self.root.grab_release();set_button.configure(state='normal')
            if commit and state.tokens:
                value=state.text();field='pc_bind.'+NATIVE_ACTIONS[output]
                self._pc_config[field]=value
                if output!='previous_item':self.remap_edits[output]=legacy.get(state.tokens[0]) if len(state.tokens)==1 else None
                self._pc_vars['options'].set(True)
                if output in ACTION_OUTPUTS:
                    action=ACTION_OUTPUTS[output]
                    selected={'Mouse1':'left','Mouse2':'right','Mouse3':'middle','Mouse4':'side_back','Mouse5':'side_forward'}.get(value,'none')
                    self._pc_vars[action].set(REVERSE_LABELS[selected])
                update_ini(ns['ini_path'](),{field:value,'pc_game_options':'1','pc_keyboard_mouse':'1','pc_kbm_profile_ready':'1' if self._pc_ds3.get() else '0'})
            self.refresh_remap_row(output)
            refresh_mode_widgets(self)
        def add(source,name):
            if not ready or not name:return
            if name in ns['reserved_keys'](self.remap_toggle_key,self.remap_reload_key):return
            token=aliases.get(name,name.upper() if len(name)==1 else name)
            legacy[token]=name;state.press(source,token);key_label.configure(text=state.text()+' …')
        def release(source):
            if state.release(source):self.root.after(35,lambda: finish(True) if not state.down else None)
            return 'break'
        def key_down(event):
            if event.keysym=='Escape':finish(False);return 'break'
            name=ns['keysym_to_name'](event.keysym)
            # Windows VKs retain the physical letter/digit when Shift changes
            # Tk's keysym (for example 1 -> exclam).
            if name is None and (48<=event.keycode<=57 or 65<=event.keycode<=90):name=chr(event.keycode).lower()
            add(('key',event.keycode),name);return 'break'
        def mouse_down(event):add(('mouse',event.num),ns['button_num_to_name'](event.num));return 'break'
        def wheel_down(event):
            name=ns['wheel_to_name'](event.delta,False);add(('wheel',0),name);return release(('wheel',0))
        def arm():
            nonlocal ready
            ready=True
        self.root.bind_all('<KeyPress>',key_down)
        self.root.bind_all('<KeyRelease>',lambda e:release(('key',e.keycode)))
        self.root.bind_all('<ButtonPress>',mouse_down)
        self.root.bind_all('<ButtonRelease>',lambda e:release(('mouse',e.num)))
        self.root.bind_all('<MouseWheel>',wheel_down);self.root.bind_all('<Shift-MouseWheel>',wheel_down)
        self.root.bind_all('<FocusOut>',lambda _e:finish(False))
        self.root.after(250,arm)

    def clear(self, output, key_label):
        if self._pc_device.get()!='kbm' or (output in ACTION_OUTPUTS and not self._pc_ds3.get()):return
        if blocked(self):return
        if output in NATIVE_ACTIONS:
            field='pc_bind.'+NATIVE_ACTIONS[output];self._pc_config[field]='none'
            update_ini(ns['ini_path'](),{field:'none'})
            if output in ACTION_OUTPUTS:self._pc_vars[ACTION_OUTPUTS[output]].set('Sin asignar')
            if output in self.remap_rows:original_clear(self,output,key_label)
            self.refresh_remap_row(output);refresh_mode_widgets(self)
        else:original_clear(self,output,key_label)

    def play(self):
        if validate(self):
            original_play(self)

    def drain(self):
        previous=self.process
        original_drain(self)
        if previous is not None and self.process is None and not getattr(self,'hidden',False):
            self.ini,self.ini_lines=ns['load_ini']()
            self._pc_config=read_config(ns['ini_path']())
            self._pc_device.set('controller' if self._pc_config.get('pc_keyboard_mouse','1')=='0' else 'kbm')
            self._pc_ds3.set(self._pc_config.get('pc_kbm_profile_ready',self._pc_config.get('pc_ds3_profile','1'))=='1')
            for key,var in getattr(self,'vars',{}).items():
                if getattr(var,'store',None)=='ini' and key in self.ini:
                    value=self.ini[key]
                    if key in ns.get('INI_FLAGS',()):value=value=='1'
                    elif key=='sharpness':value=float(value)
                    var.set(value)
            for action in DEFAULT_BINDINGS:
                self._pc_vars[action].set(REVERSE_LABELS[self._pc_config['pc_mouse_'+action]])
            for key,field in [('prompts','pc_key_prompts'),('menus','pc_game_mouse_menu'),('mirror','pc_game_mirror'),('rebirth','pc_game_rebirth'),('options','pc_game_options')]:
                self._pc_vars[key].set(self._pc_config.get(field,'0' if key in ('mirror','rebirth') else '1')=='1')
            reload_inputs(self)
            current_input=read_ini(input_path(self))
            if 'mouse_camera_sensitivity' in current_input:
                self._pc_vars['sensitivity'].set(float(current_input['mouse_camera_sensitivity']))
            rebuild_controls(self)

    def environment(settings):
        env = original_environment(settings)
        config = read_config(ns['ini_path']())
        gpu = int(config.get('pc_gpu_id', '-1'))
        if gpu >= 0:
            env['BB_GPU_ID'] = str(gpu)
        else:
            env.pop('BB_GPU_ID', None)
        return env

    def run_role(argv):
        service = None
        if argv and argv[0] == '--run':
            ns['attach_stdio']()
            main_config_path = os.environ.get('BB_PC_MAIN_CONFIG') or ns['ini_path']()
            config = read_config(main_config_path)
            # Repair environment inherited from the withdrawn optional-engine build.
            if os.environ.get('BB_PC_MAIN_CONFIG'):
                os.environ['BB_CONFIG'] = str(main_config_path)
                os.environ.pop('BB_PC_MAIN_CONFIG', None)
            owned_probe = Path(ns['PORT_DIR'])/'pc-engine'/'bin'/'bb-probe.exe'
            if os.path.normcase(os.environ.get('BB_PROBE','')) == os.path.normcase(str(owned_probe)):
                os.environ.pop('BB_PROBE',None)
            if os.environ.get('BB_GPU_USER_DIR','').endswith('pc-engine-cache'):
                os.environ.pop('BB_GPU_USER_DIR',None)
            selected_gpu = int(config.get('pc_gpu_id','-1'))
            if selected_gpu >= 0:os.environ['BB_GPU_ID']=str(selected_gpu)
            else:os.environ.pop('BB_GPU_ID',None)
            from bbport_integrated_menus import prepare
            try:
                prepare(ns, config)
            except Exception as exc:
                print('Menús nativos: no se pudo preparar el motor original: ' + str(exc), file=ns['sys'].stderr, flush=True)
                return 1
            if config['pc_controls_mode'] == 'ds3' and config.get('pc_game_options','1') != '1':
                from bbport_integrated_runtime import InputService
                try:
                    service = InputService(bindings_from_config(config))
                except Exception as exc:
                    print('No se pudieron iniciar los controles PC: ' + str(exc), file=ns['sys'].stderr, flush=True)
                    return 1
        try:
            if argv and argv[0] == '--run' and (read_config(ns['ini_path']()).get('pc_game_mirror') == '1' or read_config(ns['ini_path']()).get('pc_game_options','1') == '1'):
                from bbport_integrated_menus import run_with_mirror
                return run_with_mirror(ns, argv, original_run)
            return original_run(argv)
        finally:
            if service:
                service.stop()

    if original_style is not None:Launcher.style = style
    Launcher.__init__ = init
    Launcher.build_controls = controls
    Launcher.build_cheats = cheats
    Launcher.build_graphics = graphics
    Launcher.build_display = display
    Launcher.collect = collect
    Launcher.refresh_remap_row = refresh
    Launcher.start_remap_capture = capture
    Launcher.clear_remap = clear
    Launcher.play = play
    Launcher.detect_gpu = detect_gpu
    if original_drain is not None:Launcher.drain_output=drain
    ns['game_environment'] = environment
    ns['run_role'] = run_role

# SPDX-License-Identifier: GPL-2.0-or-later
"""Headless integration checks for native launcher configuration and lifecycle."""
import importlib.util
import pathlib
import queue
import sys
import tempfile
import types
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2] / 'launcher'
sys.path.insert(0, str(ROOT))
import bbport_integrated_logic as integrated_logic
sys.modules['bbport_integrated_logic'] = integrated_logic
import bbport_integrated_ui as integrated_ui
sys.modules['bbport_integrated_ui'] = integrated_ui


class Var:
    def __init__(self, value=None):
        self.value, self.callbacks, self.serial = value, {}, 0
    def get(self):
        return self.value
    def set(self, value):
        self.value = value
        for callback in list(self.callbacks.values()):
            callback()
    def trace_add(self, _mode, callback):
        self.serial += 1
        self.callbacks[str(self.serial)] = callback
        return str(self.serial)
    def trace_remove(self, _mode, token):
        del self.callbacks[token]


class Widget:
    registry = []
    def __init__(self, *args, **kwargs):
        self.options, self.dead = kwargs, False
        self.children = []
        if args and isinstance(args[0], Widget):
            args[0].children.append(self)
        self.registry.append(self)
    def grid(self, **_kw):
        return self
    pack = grid
    def configure(self, **kw):
        if self.dead:
            raise RuntimeError('Callback touched a destroyed widget')
        self.options.update(kw)
    def bind(self,event,callback):
        self.options[event]=callback
    def destroy(self):
        self.dead = True
        for child in self.children:
            child.destroy()


class Messages:
    def __init__(self):
        self.errors, self.infos = [], []
    def showerror(self, *args):
        self.errors.append(args)
    def showinfo(self, *args):
        self.infos.append(args)


class IntegrationTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.port = pathlib.Path(self.temp.name)
        self.config = self.port / 'bbport.ini'
        self.config.write_text('pc_game_options=0\n')
        self.input = self.port / 'input.ini'
        self.original = '# custom controls\ncross = e\nr1 = m\ncircle = space\n'
        self.input.write_text(self.original)
        self.calls = []
        Widget.registry = []
        toolkit = types.SimpleNamespace(Frame=Widget, Label=Widget, Button=Widget,
                     Combobox=Widget, Checkbutton=Widget, Radiobutton=Widget, Scale=Widget)
        tk = types.SimpleNamespace(StringVar=Var, DoubleVar=Var, BooleanVar=Var)
        tests = self
        def load_input(path):
            values = integrated_ui.read_ini(path)
            return {k: [v] for k,v in values.items()}, [], 'f7', 'f8', {}
        class Launcher:
            def __init__(self):
                self.tk, self.ttk, self.process = tk, toolkit, None
                self.ui_calls, self.pages = queue.Queue(), {}
                self.messagebox, self.app = Messages(), {}
                self.ini, self.ini_lines = {}, []
                self.remap_bindings, self.remap_lines, self.remap_toggle_key, self.remap_reload_key, self.mouse_settings = load_input(tests.input)
                self.remap_edits, self.mouse_edited = {}, False
                self.build_graphics()
                self.build_display()
                self.build_cheats()
                self.build_controls()
            def build_graphics(self):
                parent=self.scrolled_page('graphics', '', '')
                self.check(parent,'show_fps','ini','Show the FPS counter')
            def build_display(self):
                parent=self.scrolled_page('display','','')
                self.section(parent,'Frame rate')
            def check(self,parent,key,store,text,**kwargs):
                return Widget(parent,text=text,key=key,store=store)
            def build_cheats(self):
                parent=self.scrolled_page('cheats', '', '')
                self.section(parent,tests.ns['_']('Cheats'))
                self.section(parent,'Gameplay tweaks')
            def build_controls(self):
                parent = self.scrolled_page('controls', '', '')
                self.remap_rows = {o: Widget(parent) for o in ['r1','r2','l2','cross']}
                for output in self.remap_rows:
                    self.refresh_remap_row(output)
                self.section(parent, 'Mouse look')
                self.note(parent, 'Last mouse setting')
            def scrolled_page(self, name, *_args):
                w = Widget()
                self.pages[name] = w
                return w
            def next_row(self, _parent):
                return 0
            def row(self, _parent, title, *_args, **_kwargs):
                self.row_titles=getattr(self,'row_titles',[])+[title]
            def section(self, parent, text, **kwargs):
                Widget(parent, text=text)
            def note(self, parent, text, **kwargs):
                Widget(parent, text=text)
            def show(self, name):
                self.page = name
            def px(self, value):
                return value
            def refresh_status(self):
                tests.calls.append(('gpu_status',getattr(self,'gpu_text','')))
            def collect(self):
                tests.calls.append('collect')
            def refresh_remap_row(self, output):
                self.remap_rows[output].configure(text=output)
            def start_remap_capture(self, *_args):
                tests.calls.append('original_capture')
            def clear_remap(self, *_args):
                tests.calls.append('original_clear')
            def play(self):
                self.collect()
                tests.calls.append(('play', tests.ns['game_environment']({})))
        self.ns = {'_': lambda text,*args:text, 'Launcher': Launcher, 'ini_path': lambda: self.config,
                   'input_ini_path': lambda _path: self.input, 'load_input_ini': load_input,
                   'load_ini': lambda: (integrated_ui.read_ini(self.config), []),
                   'PORT_DIR': self.port, 'game_environment': lambda _: {'BB_GPU_ID': 'old'},
                   'run_role': lambda args: self.calls.append(('role', args)) or 0,
                   'attach_stdio': lambda: None, 'sys': sys}
        from bbport_integrated_profile import PROFILE_TEXT
        integrated_ui.PROFILE_TEXT = PROFILE_TEXT
        integrated_ui.install(self.ns)
        self.launcher = self.ns['Launcher']()

    def button(self, text):
        return next(w.options['command'] for w in reversed(Widget.registry)
                    if not w.dead and w.options.get('text') == text)

    def apply(self):
        self.launcher._pc_ds3.set(True)
        self.button('Usar perfil Dark Souls 3 PC')()

    def test_apply_saves_preset_and_backup_then_restores_exact_input(self):
        self.apply()
        self.assertEqual((self.port/'input.before-integrated.ini').read_text(), self.original)
        values = integrated_ui.read_ini(self.input)
        self.assertEqual(values['r1'], 'r1')  # controller line is preserved after f10 line
        self.assertIn('r1 = f10', self.input.read_text())
        self.assertEqual(values['stick_turn_rate'], '900')
        self.assertEqual(values['stick_release_delay_ms'], '60')
        self.assertEqual(values['stick_circular'], 'off')
        self.assertEqual(integrated_ui.read_ini(self.config)['pc_controls_mode'], 'ds3')
        self.launcher._pc_ds3.set(False)
        self.button('Usar perfil Dark Souls 3 PC')()
        self.assertIn('cross = e', self.input.read_text())
        self.assertEqual(integrated_ui.read_ini(self.config)['pc_controls_mode'], 'standard')

    def test_save_custom_mouse_and_sprint_settings(self):
        self.apply()
        obj = self.launcher
        obj._pc_vars['light'].set('Botón lateral atrás')
        obj._pc_vars['heavy'].set('Shift + lateral atrás')
        obj._pc_vars['turn'].set(1200)
        obj._pc_vars['gap'].set(40)
        obj.collect()
        cfg = integrated_ui.read_config(self.config)
        self.assertEqual(integrated_ui.bindings_from_config(cfg)['heavy'], ('side_back', True))
        self.assertEqual(integrated_ui.read_ini(self.input)['stick_turn_rate'], '1200')
        self.assertEqual(integrated_ui.read_ini(self.input)['stick_release_delay_ms'], '40')

    def test_apply_profile_clears_stale_game_bindings_and_preserves_other_settings(self):
        integrated_ui.update_ini(self.config, {'pc_bind.interact': 'Z',
            'pc_bind.roll': 'Backspace', 'pc_bind.strong_modifier': 'none',
            'pc_bind.confirm': 'Space', 'pc_bind.back': 'E', 'pc_camera_fov': '1.25'})
        self.apply()
        values = integrated_ui.read_ini(self.config)
        self.assertNotIn('pc_bind.interact', values)
        self.assertNotIn('pc_bind.roll', values)
        self.assertEqual(values['pc_bind.strong_modifier'], 'Left Shift')
        self.assertEqual(values['pc_bind.confirm'], 'Return')
        self.assertEqual(values['pc_bind.back'], 'Backspace')
        self.assertEqual(values['pc_camera_fov'], '1.25')
        # Saving normally does not reapply defaults over later in-game changes.
        integrated_ui.update_ini(self.config, {'pc_bind.confirm': 'E'})
        self.launcher.collect()
        self.assertEqual(integrated_ui.read_ini(self.config)['pc_bind.confirm'], 'E')

    def test_gpu_selection_reaches_game_environment(self):
        self.launcher._pc_gpu_choices['RTX 4090'] = 1
        self.launcher._pc_gpu.set('RTX 4090')
        self.launcher.play()
        self.assertEqual(self.calls[-1][1]['BB_GPU_ID'], '1')

    def test_duplicate_combinations_block_play(self):
        self.launcher._pc_vars['heavy'].set('Clic izquierdo')
        self.launcher.play()
        self.assertTrue(self.launcher.messagebox.errors)
        self.assertFalse(any(isinstance(c, tuple) and c[0]=='play' for c in self.calls))

    def test_apply_profile_recovers_from_duplicate_attacks_then_allows_play(self):
        self.launcher._pc_vars['light'].set('Clic izquierdo')
        self.launcher._pc_vars['heavy'].set('Clic izquierdo')
        self.launcher._pc_vars['firearm'].set('Clic izquierdo')
        self.apply()
        cfg = integrated_ui.read_ini(self.config)
        self.assertEqual(cfg['pc_mouse_light'], 'left')
        self.assertEqual(cfg['pc_mouse_heavy'], 'shift+left')
        self.assertEqual(cfg['pc_mouse_firearm'], 'right')
        self.assertFalse(self.launcher.messagebox.errors)
        self.launcher.play()
        self.assertTrue(any(isinstance(c, tuple) and c[0]=='play' for c in self.calls))

    def test_launcher_capture_accumulates_until_all_released_and_persists_chord(self):
        class Root:
            def __init__(self):self.bindings={};self.pending=[]
            def bind_all(self,e,f):self.bindings[e]=f
            def unbind_all(self,e):self.bindings.pop(e,None)
            def grab_set(self):pass
            def grab_release(self):pass
            def after(self,_ms,f):self.pending.append(f)
        root=Root();self.launcher.root=root;self.launcher.wheel=lambda *_:None
        self.ns.update(reserved_keys=lambda *_:set(),keysym_to_name=lambda k:{'Shift_L':'lshift'}.get(k,k.lower()),button_num_to_name=lambda n:{3:'rightbutton'}[n],wheel_to_name=lambda *_:'mousewheelup')
        self.launcher.start_remap_capture('r2',self.launcher.remap_rows['r2'],Widget())
        root.pending.pop(0)() # arm after the opening click
        root.bindings['<KeyPress>'](types.SimpleNamespace(keysym='Shift_L',keycode=16))
        root.bindings['<ButtonPress>'](types.SimpleNamespace(num=3))
        root.bindings['<KeyRelease>'](types.SimpleNamespace(keycode=16))
        self.assertFalse(root.pending) # right mouse still held
        root.bindings['<ButtonRelease>'](types.SimpleNamespace(num=3))
        root.pending.pop(0)()
        values=integrated_ui.read_ini(self.config)
        self.assertEqual(values['pc_bind.strong_attack'],'Left Shift + Mouse2')
        self.assertEqual(values['pc_game_options'],'1')
        self.assertEqual(self.launcher.remap_rows['r2'].options['text'],'Left Shift + Mouse2')
        self.launcher.collect()
        self.assertEqual(integrated_ui.read_ini(self.config)['pc_bind.strong_attack'],'Left Shift + Mouse2')

    def test_native_run_starts_and_stops_embedded_service_only_for_run_role(self):
        self.apply()
        integrated_ui.update_ini(self.config,{'pc_game_options':'0','pc_key_prompts':'0'})
        tests = self
        class Service:
            def __init__(self, bindings):
                tests.calls.append(('service', bindings))
            def stop(self):
                tests.calls.append('stop')
        sys.modules['bbport_integrated_runtime'] = types.SimpleNamespace(InputService=Service)
        sys.modules['bbport_integrated_menus'] = types.SimpleNamespace(prepare=lambda ns,cfg: tests.calls.append('prepare_menus'))
        self.assertEqual(self.ns['run_role'](['--run']), 0)
        self.assertEqual(self.calls[-1], 'stop')
        self.assertIn('prepare_menus', self.calls)
        self.assertTrue(any(isinstance(c, tuple) and c[0]=='service' for c in self.calls))
        self.calls.clear()
        self.ns['run_role'](['--script', 'prepare.py'])
        self.assertEqual(self.calls, [('role', ['--script', 'prepare.py'])])

    def test_custom_controls_follow_mouse_look_and_mirror_lives_in_cheats(self):
        controls = self.launcher.pages['controls']
        texts = [w.options.get('text','') for w in controls.children]
        self.assertNotIn('Controles de PC integrados', texts)
        active=[w.options.get('text') for w in Widget.registry if not w.dead]
        self.assertLess(active.index('Keyboard and mouse'),active.index('Mouse look'))
        self.assertIn('Usar perfil Dark Souls 3 PC',active)
        self.assertNotIn('Usar ratón en los menús del juego', texts)
        self.assertFalse(any('motor original' in text or '1.4' in text or 'Espejo' in text for text in texts))
        cheats = self.launcher.pages['cheats']
        mirror = next(w for w in cheats.children if w.options.get('text') == 'El espejo del Sueño del Cazador abre el editor de apariencia')
        self.assertIs(mirror.options['variable'], self.launcher._pc_vars['mirror'])
        self.launcher._pc_vars['mirror'].set(True)
        self.launcher._pc_vars['menus'].set(False)
        self.launcher.collect()
        saved = integrated_ui.read_ini(self.config)
        self.assertEqual(saved['pc_game_mirror'], '1')
        self.assertEqual(saved['pc_game_mouse_menu'], '1')
        self.apply()
        self.assertFalse(mirror.dead)
        self.assertIs(mirror.options['variable'], self.launcher._pc_vars['mirror'])

    def test_pc_pages_use_native_input_without_starting_legacy_bridge(self):
        self.apply()
        integrated_ui.update_ini(self.config, {'pc_game_options':'1'})
        tests=self
        sys.modules['bbport_integrated_menus']=types.SimpleNamespace(
            prepare=lambda ns,cfg: tests.calls.append('native_prepare'),
            run_with_mirror=lambda ns,args,original: tests.calls.append('native_pages') or original(args))
        class ForbiddenBridge:
            def __init__(self,*args):raise AssertionError('duplicate input bridge')
        sys.modules['bbport_integrated_runtime']=types.SimpleNamespace(InputService=ForbiddenBridge)
        self.assertEqual(self.ns['run_role'](['--run']),0)
        self.assertIn('native_pages',self.calls)

    def test_unbound_mouse_actions_from_native_page_survive_launcher_save(self):
        for action in ('light','heavy','firearm'):
            self.launcher._pc_vars[action].set('Sin asignar')
        self.launcher.collect()
        saved=integrated_ui.read_config(self.config)
        self.assertEqual([saved['pc_mouse_'+a] for a in ('light','heavy','firearm')],['none']*3)

    def test_tweaks_page_translation_order_and_rebirth_persistence(self):
        self.assertEqual(self.ns['_']('Cheats','Читы'),'Tweaks & Cheats')
        for lang,options,mirror,rebirth in [
            ('es','Menús de ajustes dentro del juego','El espejo del Sueño del Cazador abre el editor de apariencia','El Altar de la Desesperación restablece el nivel y los atributos con la Piedra de Yharnam'),
            ('en','In-game tweak menus',"Hunter's dream mirror opens the appearance editor",'The Altar of Despair resets level and attributes with the Yharnam Stone')]:
            self.ns['LANG']=lang
            launcher=self.ns['Launcher']()
            fps=[w for w in launcher.pages['display'].children if w.options.get('key')=='show_fps']
            self.assertEqual(len(fps),1)
            self.assertEqual(fps[0].options['text'],'Mostrar FPS' if lang=='es' else 'Show FPS')
            self.assertFalse(any(w.options.get('key')=='show_fps' for w in launcher.pages['graphics'].children))
            page=launcher.pages['cheats']
            texts=[w.options.get('text','') for w in page.children]
            prompt_text='Mostrar teclas de teclado y ratón en el juego' if lang=='es' else 'Show keyboard and mouse prompts in-game'
            self.assertIn(prompt_text,texts)
            self.assertIn('Cheats',texts)
            self.assertNotIn('Tweaks & Cheats',texts)
            self.assertEqual(texts.index(mirror),texts.index(options)+2)
            self.assertFalse(any('Adds Key Bindings' in t or 'Añade Key Bindings' in t or 'Also enables' in t for t in texts))
            self.assertEqual(texts.index(rebirth),texts.index(mirror)+1)
            self.assertNotIn(options,[w.options.get('text') for w in launcher.pages['controls'].children])
            launcher._pc_vars['options'].set(False)
            launcher._pc_vars['rebirth'].set(True)
            next(w.options['command'] for w in page.children if w.options.get('text')==rebirth)()
            self.assertTrue(launcher._pc_vars['options'].get())
            launcher.collect()
            self.assertEqual(integrated_ui.read_ini(self.config)['pc_game_rebirth'],'1')
            reopened=self.ns['Launcher']()
            self.assertTrue(reopened._pc_vars['rebirth'].get())
            reopened._pc_vars['options'].set(False)
            next(w.options['command'] for w in reopened.pages['cheats'].children if w.options.get('text')==options)()
            self.assertFalse(reopened._pc_vars['rebirth'].get())
            reopened.collect()
            self.assertEqual(integrated_ui.read_ini(self.config)['pc_game_rebirth'],'0')
            self.assertEqual(integrated_ui.read_ini(self.config)['pc_key_prompts'],'0')

    def test_config_writer_preserves_comments_and_unrelated_keys(self):
        self.config.write_text('# original\nresolution=4k\npc_gpu_id=0\npc_gpu_id=2\n')
        integrated_ui.update_ini(self.config, {'pc_gpu_id': 1})
        text = self.config.read_text()
        self.assertIn('# original\nresolution=4k\n', text)
        self.assertEqual(text.count('pc_gpu_id='), 1)

    def test_first_launch_defaults_to_ds3_keyboard_mouse_and_mouse_look(self):
        cfg=integrated_ui.read_ini(self.config)
        self.assertEqual(cfg['pc_keyboard_mouse'],'1')
        self.assertEqual(cfg['pc_key_prompts'],'1')
        self.assertEqual(cfg['pc_ds3_profile'],'1')
        self.assertEqual(cfg['pc_mouse_camera'],'1')
        self.assertIn('cross = e',self.input.read_text())
        self.assertEqual(integrated_ui.read_ini(self.input)['mouse_to_joystick'],'right')
        self.assertTrue(self.launcher._pc_ds3.get())
        self.assertEqual(cfg['pc_key_prompts'],'1')

    def test_device_toggle_hides_ds3_block_and_remaps_labels_then_restores_kbm(self):
        self.button('Control')()
        cfg=integrated_ui.read_ini(self.config)
        self.assertEqual(cfg['pc_keyboard_mouse'],'0')
        self.assertEqual(cfg['pc_key_prompts'],'0')
        self.assertEqual(cfg['pc_mouse_camera'],'0')
        self.assertEqual(cfg['pc_game_mouse_menu'],'1')
        self.assertFalse(any(w.options.get('text')=='Usar perfil Dark Souls 3 PC' for w in Widget.registry if not w.dead))
        self.assertEqual(self.launcher.remap_rows['r1'].options['text'],'R1')
        self.button('Keyboard and mouse')()
        cfg=integrated_ui.read_ini(self.config)
        self.assertEqual(cfg['pc_keyboard_mouse'],'1')
        self.assertEqual(cfg['pc_key_prompts'],'1')
        self.assertEqual(cfg['pc_ds3_profile'],'1')
        self.assertEqual(cfg['pc_mouse_camera'],'1')
        self.assertEqual(integrated_ui.read_ini(self.input)['mouse_to_joystick'],'right')
        self.assertEqual((self.port/'input.before-integrated.ini').read_text(),self.original)

    def test_device_switch_preserves_custom_keyboard_and_controller_bindings(self):
        integrated_ui.update_ini(self.config,{'pc_bind.interact':'T','pc_bind.attack':'Left Ctrl + Mouse1'})
        self.launcher._pc_config.update({'pc_bind.interact':'T','pc_bind.attack':'Left Ctrl + Mouse1'})
        pad_before=[line for line in self.input.read_text().splitlines() if ' = cross' in line or ' = r1' in line]
        self.button('Control')()
        self.assertEqual(integrated_ui.read_ini(self.config)['pc_bind.interact'],'T')
        self.button('Keyboard and mouse')()
        cfg=integrated_ui.read_ini(self.config)
        self.assertEqual(cfg['pc_bind.interact'],'T')
        self.assertEqual(cfg['pc_bind.attack'],'Left Ctrl + Mouse1')
        self.assertTrue(all(line in self.input.read_text().splitlines() for line in pad_before))

    def test_ds3_checkbox_disables_attack_capture_and_is_persistent(self):
        self.launcher._pc_ds3.set(False);self.button('Usar perfil Dark Souls 3 PC')()
        self.assertFalse(self.launcher._pc_attack_widgets)
        self.assertFalse(any(w.options.get('text')=='Ataque ligero' for w in Widget.registry if not w.dead))
        self.launcher.start_remap_capture('r1',self.launcher.remap_rows['r1'],Widget()) # must return before touching root
        cfg=integrated_ui.read_ini(self.config)
        self.assertEqual(cfg['pc_ds3_profile'],'0')
        self.assertEqual(cfg['pc_bind.strong_modifier'],'none')
        self.assertEqual(cfg['pc_mouse_heavy'],'none')
        self.assertEqual(cfg['pc_mouse_camera'],'1')
        reopened=self.ns['Launcher']()
        self.assertFalse(reopened._pc_ds3.get())
        self.assertFalse(reopened._pc_attack_widgets)

    def test_toggle_during_game_restores_selection_without_changing_files(self):
        before=self.config.read_bytes();self.launcher.process=object()
        self.button('Control')()
        self.assertEqual(self.launcher._pc_device.get(),'kbm')
        self.assertEqual(self.config.read_bytes(),before)

    def test_profile_rows_match_standard_assignment_layout_and_hide(self):
        self.ns['LANG']='en';start=len(Widget.registry)
        launcher=self.ns['Launcher']()
        texts=[w.options.get('text') for w in Widget.registry[start:] if not w.dead]
        for text in ('Controller','Use Dark Souls 3 PC profile','Light attack','Strong / charged attack','Firearm / parry','Set…','Clear','Next item','Previous item','Sprint and direction changes','In-game tweak menus'):
            self.assertIn(text,texts)
        self.assertIn('Turning speed',launcher.row_titles)
        self.assertIn('WASD key release delay',launcher.row_titles)
        self.assertNotIn('Carrera y cambios de dirección',texts)
        self.assertFalse(launcher._pc_attack_combos)
        self.assertEqual(len(launcher._pc_attack_widgets),10)
        for label in launcher._pc_attack_labels.values():
            self.assertEqual(label.options['width'],14)
            self.assertEqual(label.options['style'],'Muted.TLabel')
        self.assertEqual(integrated_ui.read_ini(self.config)['pc_bind.previous_item'],'WheelUp')
        self.assertEqual(integrated_ui.read_ini(self.config)['pc_bind.switch_item'],'Down, WheelDown')
        launcher._pc_ds3.set(False);self.button('Use Dark Souls 3 PC profile')()
        self.assertFalse(launcher._pc_attack_widgets)
        self.assertFalse(any(w.options.get('text')=='Light attack' for w in Widget.registry if not w.dead))
        launcher._pc_ds3.set(True);self.button('Use Dark Souls 3 PC profile')()
        self.assertEqual(len(launcher._pc_attack_widgets),10)
        self.assertTrue(any(w.options.get('text')=='Light attack' for w in Widget.registry if not w.dead))

    def test_previous_item_clear_is_native_only_and_profile_restores_both_directions(self):
        self.launcher.clear_remap('previous_item',self.launcher._pc_attack_labels['previous_item'])
        self.assertEqual(integrated_ui.read_ini(self.config)['pc_bind.previous_item'],'none')
        self.assertNotIn('previous_item',self.launcher.remap_edits)
        self.launcher._pc_ds3.set(True);self.button('Usar perfil Dark Souls 3 PC')()
        config=integrated_ui.read_ini(self.config)
        self.assertEqual(config['pc_bind.previous_item'],'WheelUp')
        self.assertEqual(config['pc_bind.switch_item'],'Down, WheelDown')

    def test_selected_gpu_updates_main_check_without_using_default(self):
        self.launcher._pc_devices={0:'NVIDIA RTX 3090',1:'NVIDIA RTX 4090'}
        self.launcher._pc_gpu_choices.update({'3090':0,'4090':1})
        self.launcher._pc_gpu.set('4090')
        self.assertIn('4090',self.launcher.gpu_text)
        self.launcher._pc_gpu.set('3090')
        self.assertIn('3090',self.launcher.gpu_text)


    def test_customized_shift_action_is_exclusive_and_latched(self):
        output = []
        time = [0]
        ctrl = integrated_logic.Controls(lambda *a: output.append(a), lambda: time[0],
                    {'light': ('side_back', False), 'heavy': ('side_back', True), 'firearm': ('right', False)})
        ctrl.press('side_back', True)
        ctrl.press('side_back', False)
        self.assertEqual(output, [('heavy', True)])
        time[0] = 2
        ctrl.release('side_back')
        self.assertEqual(output[-1], ('heavy', False))
        ctrl.press('left')
        self.assertEqual(ctrl.output, set())
        ctrl.press('side_back')
        ctrl.cancel()
        self.assertEqual(ctrl.output, set())


if __name__ == '__main__':
    unittest.main(verbosity=2)

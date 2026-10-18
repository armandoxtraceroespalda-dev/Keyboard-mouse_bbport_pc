# SPDX-License-Identifier: GPL-3.0-or-later
import importlib
import json
import os
import sys
import tempfile
import types
import unittest
from pathlib import Path
from unittest.mock import patch
import bbport_integrated_menus as integrated_menus
import bbport_integrated_ui as integrated_ui
class PrepareTests(unittest.TestCase):
 def test_source_engine_is_never_patched_and_physical_input_view_is_prepared(self):
  with tempfile.TemporaryDirectory() as tmp,patch.dict(os.environ,{},clear=True):
   port=Path(tmp);(port/'bin').mkdir()
   (port/'bin/bb-probe.exe').write_bytes(b'unchanged source-built engine')
   for name in ('bb-native-ui.dll','bb-native-assets.exe'):(port/'bin'/name).write_bytes(b'component')
   (port/'input.ini').write_text('r1 = f10\nr1 = r1\naxis_left_y_minus = w\naxis_left_x = axis_left_x\n')
   ns={'PORT_DIR':port,'ini_path':lambda:port/'bbport.ini','input_ini_path':lambda _:port/'input.ini'}
   integrated_menus.prepare(ns,{})
   self.assertEqual((port/'bin/bb-probe.exe').read_bytes(),b'unchanged source-built engine')
   self.assertFalse((port/'bin/bb-probe-ui.exe').exists())
   self.assertNotIn('BB_PROBE',os.environ)
   self.assertEqual(os.environ['BB_NATIVE_MENUS'],'1')
   physical=Path(os.environ['BB_INPUT_CONFIG']).read_text()
   self.assertIn('r1 = r1',physical);self.assertNotIn('r1 = f10',physical)
 def test_developer_out_directory_and_always_enabled_menu_mouse(self):
  with tempfile.TemporaryDirectory() as tmp,patch.dict(os.environ,{},clear=True):
   port=Path(tmp);(port/'out').mkdir();(port/'out/bb-native-ui.dll').write_bytes(b'component')
   integrated_menus.prepare({'PORT_DIR':port},{'pc_game_mouse_menu':'0','pc_game_mirror':'1','pc_game_options':'0'})
   self.assertEqual(os.environ['BB_NATIVE_MENUS'],'1');self.assertEqual(os.environ['BB_NATIVE_MIRROR'],'1')
 def test_missing_native_component_stops_with_actionable_error(self):
  with tempfile.TemporaryDirectory() as tmp:
   with self.assertRaisesRegex(RuntimeError,'bb-native-ui.dll'):
    integrated_menus.prepare({'PORT_DIR':tmp},{})
 def test_mirror_prepares_after_linking_and_backs_up_before_game(self):
  with tempfile.TemporaryDirectory() as tmp,patch.dict(os.environ,{},clear=True):
   port=Path(tmp);(port/'user/savedata').mkdir(parents=True)
   (port/'user/savedata/save.bin').write_bytes(b'original save')
   events=[]
   fake=types.SimpleNamespace(verify_executable=lambda elf:events.append('verify') or 'identity',
                             prepare_mirror=lambda *a:events.append('layout') or port/'layout.dcx',
                             prepare_mirror_messages=lambda *a:port/'mirror-messages',
                             prepare_menu_assets=lambda *a,**kw:events.append('menus') or port/'menus')
   sys.modules['bbport_integrated_mirror_assets']=fake
   context={'events':events,'os':os,'port':port}
   exec("def run_script(name,*args,**kwargs): events.append(name)\ndef main():\n run_script('link_modules.py',port/'game','--out',port/'out')\n assert os.environ['BB_ENGINE_IMAGE_SHA256']=='identity'\n events.append('game')\n",context)
   def original_path(*a,**kw):return context
   rp=types.SimpleNamespace(run_path=original_path)
   def run(argv):rp.run_path(port/'run.py',run_name='__main__');return 0
   (port/'bbport.ini').write_text('pc_game_mirror=1\npc_game_options=0\n')
   self.assertEqual(integrated_menus.run_with_mirror({'PORT_DIR':port,'runpy':rp,'ini_path':lambda:port/'bbport.ini'},['--run'],run),0)
   self.assertIs(rp.run_path,original_path)
   self.assertEqual(events,['link_modules.py','verify','layout','game'])
   self.assertEqual((port/'native-ui-assets/save-backup-before-appearance/save.bin').read_bytes(),b'original save')
   self.assertEqual((port/'user/savedata/save.bin').read_bytes(),b'original save')
 def test_pc_pages_prepare_messages_movie_and_rebirth_from_actual_app_view(self):
  with tempfile.TemporaryDirectory() as tmp,patch.dict(os.environ,{},clear=True):
   port=Path(tmp);(port/'bin').mkdir();(port/'bin/bb-native-assets.exe').write_bytes(b'converter');(port/'bbport.ini').write_text('pc_game_options=1\npc_game_mirror=0\npc_game_rebirth=1\n')
   (port/'user/savedata').mkdir(parents=True);(port/'user/savedata/save.bin').write_bytes(b'keep')
   events=[]
   def prepare(game,elf,cache,tool,rebirth=False):
    self.assertEqual(game,port/'mod-view');self.assertTrue(rebirth)
    self.assertEqual(tool,port/'bin/bb-native-assets.exe');events.append('movie/messages/talk');return port/'menus'
   sys.modules['bbport_integrated_mirror_assets']=types.SimpleNamespace(verify_executable=lambda _:events.append('verified') or 'identity',prepare_mirror=lambda *a:self.fail('mirror disabled'),prepare_menu_assets=prepare)
   context={'events':events,'port':port,'os':os}
   exec("def run_script(name,*args,**kwargs): events.append(name)\ndef main():\n run_script('link_modules.py',port/'mod-view','--out',port/'out')\n assert os.environ['BB_PC_MENU_ASSETS']==str(port/'menus')\n assert os.environ['BB_UI_GAME_VIEW']==str(port/'mod-view')\n assert 'BB_REBIRTH_ASSET' in os.environ\n assert 'BB_DREAM_MIRROR_ASSET' not in os.environ\n events.append('game')\n",context)
   rp=types.SimpleNamespace(run_path=lambda *a,**kw:context)
   def run(argv):rp.run_path(port/'run.py',run_name='__main__');return 0
   self.assertEqual(integrated_menus.run_with_mirror({'PORT_DIR':port,'runpy':rp,'ini_path':lambda:port/'bbport.ini'},['--run'],run),0)
   self.assertEqual(events,['link_modules.py','verified','movie/messages/talk','game'])
   self.assertEqual((port/'native-ui-assets/save-backup-before-appearance/save.bin').read_bytes(),b'keep')

 def test_legacy_fallback_preserves_keyboard_rebinds_and_restores_mouse_chords(self):
  with tempfile.TemporaryDirectory() as tmp,patch.dict(os.environ,{},clear=True):
   port=Path(tmp);original=port/'input.ini';original.write_text('r1 = z\nr1 = leftbutton\nr1 = r1\nr2 = r2\nl2 = rightbutton\n')
   ns={'PORT_DIR':port,'ini_path':lambda:port/'bbport.ini','input_ini_path':lambda _:original}
   integrated_menus.prepare_legacy_input(ns,{'pc_controls_mode':'ds3'})
   text=Path(os.environ['BB_INPUT_CONFIG']).read_text()
   self.assertIn('r1 = z',text);self.assertIn('r1 = r1',text);self.assertIn('r1 = f10',text)
   self.assertNotIn('leftbutton',text);self.assertNotIn('rightbutton',text)
   self.assertIn('leftbutton',original.read_text())

if __name__=='__main__':unittest.main(verbosity=2)

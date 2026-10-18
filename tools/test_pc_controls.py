#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Source integration checks. Python is sufficient; --native-tests also needs Linux x64/C++."""
from pathlib import Path
import argparse,os,subprocess,sys,tempfile,shutil
root=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--native-tests',action='store_true')
parser.add_argument('--cxx',default='g++')
parser.add_argument('--game-eboot',type=Path)
parser.add_argument('--game-menu',type=Path)
args=parser.parse_args()
env=dict(os.environ);env['PYTHONPATH']=str(root/'launcher')+os.pathsep+env.get('PYTHONPATH','')
subprocess.run([sys.executable,'-m','unittest','discover','-s',str(root/'native/tests'),'-p','test_*.py'],env=env,check=True)
if shutil.which('bash'):
 subprocess.run(['bash','-n',str(root/'build.sh'),str(root/'packaging/windows/package.sh')],check=True)
else:print('SKIP: shell syntax check requires MSYS2/bash')
if args.native_tests:
 if not sys.platform.startswith('linux'):
  raise SystemExit('The native unit harness uses Linux x64. Run Python tests without --native-tests on Windows.')
 ui=root/'native/pc_ui';pages=ui/'pages';assets=root/'native/pc_assets'
 cases=[('items',[pages/'test_item_cycle.cpp']),('input',[pages/'test_native_input.cpp']),
        ('settings',[pages/'test_settings.cpp',pages/'bindings.cpp']),
        ('controls',[pages/'test_vanilla_controls.cpp']),('language',[pages/'test_language_text.cpp']),
        ('physical',[ui/'test_physical_input.cpp']),('menu-keys',[ui/'test_menu_keys.cpp']),
        ('pointer',[ui/'test_menu_pointer.cpp']),('camera',[pages/'test_camera_rotation.cpp']),
        ('prompts',[pages/'test_prompt_text.cpp'])]
 with tempfile.TemporaryDirectory(prefix='bb-pc-source-tests-') as folder:
  temp=Path(folder)
  for name,sources in cases:
   exe=temp/name
   subprocess.run([args.cxx,'-std=c++20','-O2',*(['-DBBUI_TEST'] if name=='settings' else []),*map(str,sources),'-o',str(exe)],check=True)
   subprocess.run([str(exe)],check=True)
  if args.game_menu:
   subprocess.run([sys.executable,str(assets/'generate_messages.py'),str(assets/'pc_option_messages.tsv'),str(temp/'pc_option_messages.inc')],check=True)
   exe=temp/'movie'
   subprocess.run([args.cxx,'-std=c++20','-O2','-I'+str(temp),*[str(assets/n) for n in ('test_movie.cpp','rebirth_script.cpp','esd.cpp','sha256.cpp')],'-lz','-o',str(exe)],check=True)
   subprocess.run([str(exe),str(args.game_menu.resolve())],check=True)
 if args.game_eboot:
  subprocess.run([sys.executable,str(ui/'test_vanilla_game_layout.py'),str(args.game_eboot.resolve())],check=True)
  subprocess.run([sys.executable,str(ui/'test_item_cycle_original.py'),str(args.game_eboot.resolve())],check=True)

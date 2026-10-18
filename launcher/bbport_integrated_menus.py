# SPDX-License-Identifier: GPL-3.0-or-later
"""Prepare local game assets for the source-integrated native UI module."""
import hashlib
import json
import os
from pathlib import Path

def native_executable(port,name):
    for folder in ('bin','out'):
        path=Path(port)/folder/name
        if path.is_file():return path
    raise RuntimeError(f'Falta {name}. Compila con bash build.sh o extrae el paquete completo.')

def prepare(ns, config):
    port=Path(ns['PORT_DIR'])
    menus=True
    mirror=config.get('pc_game_mirror','0')=='1'
    options=config.get('pc_game_options','1')=='1'
    native_executable(port,'bb-native-ui.dll')
    if options:native_executable(port,'bb-native-assets.exe')
    # Remove only our obsolete binary-patched entry point from older packages.
    if os.path.normcase(os.environ.get('BB_PROBE',''))==os.path.normcase(str(port/'bin'/'bb-probe-ui.exe')):
        os.environ.pop('BB_PROBE',None)
    if menus:os.environ['BB_NATIVE_MENUS']='1'
    else:os.environ.pop('BB_NATIVE_MENUS',None)
    if options:
        os.environ['BB_NATIVE_OPTIONS']='1'
        original_input=Path(os.environ.get('BB_UI_INPUT_ORIGINAL') or ns['input_ini_path'](ns['ini_path']())).resolve()
        os.environ['BB_UI_INPUT_ORIGINAL']=str(original_input)
        if not original_input.is_file():
            from bbport_input_config import DEFAULT_INPUT_INI
            original_input.parent.mkdir(parents=True,exist_ok=True)
            original_input.write_text(DEFAULT_INPUT_INI,encoding='utf-8')
        physical=Path(os.environ.get('BB_DATA_DIR',port))/'native-ui-assets'/'input-controller.ini'
        physical.parent.mkdir(parents=True,exist_ok=True)
        physical.write_text(controller_input(original_input.read_text(encoding='utf-8')),encoding='utf-8')
        os.environ['BB_INPUT_CONFIG']=str(physical)
    else:
        os.environ.pop('BB_NATIVE_OPTIONS',None)
        original_input=os.environ.pop('BB_UI_INPUT_ORIGINAL',None)
        if original_input:os.environ['BB_INPUT_CONFIG']=original_input
        prepare_legacy_input(ns,config)
    if mirror:os.environ['BB_NATIVE_MIRROR']='1'
    else:os.environ.pop('BB_NATIVE_MIRROR',None)
    print('Menús nativos: motor original preparado (renderer conservado).',flush=True)


def run_with_mirror(ns, argv, original_run):
    """Retain the original run.py, preparing only a local copy of the Dream layout."""
    import shutil
    from bbport_integrated_mirror_assets import prepare_mirror, verify_executable, prepare_menu_assets
    port=Path(ns['PORT_DIR'])
    original_path=ns['runpy'].run_path
    def run_path(path,*args,**kwargs):
        if Path(path).resolve()!=(port/'run.py').resolve() or kwargs.get('run_name')!='__main__':
            return original_path(path,*args,**kwargs)
        namespace=original_path(path,run_name='bbport.original.mirror.prepare')
        main=namespace['main'];globals_=main.__globals__
        original_script=globals_['run_script']
        def script(name,*args,**kwargs):
            result=original_script(name,*args,**kwargs)
            if name=='link_modules.py':
                data=Path(os.environ.get('BB_DATA_DIR',port))
                game=Path(args[0]);out=Path(args[args.index('--out')+1]) if '--out' in args else data/'out'
                elf=out/'eboot.elf'
                identity=verify_executable(elf)
                from bbport_integrated_ui import read_config
                config=read_config(ns['ini_path']())
                layout=prepare_mirror(game,elf,data/'native-ui-assets') if config.get('pc_game_mirror')=='1' else None
                os.environ['BB_UI_GAME_VIEW']=str(game.resolve())
                if config.get('pc_game_options','1')=='1':
                    tool=native_executable(port,'bb-native-assets.exe')
                    menus=prepare_menu_assets(game,elf,data/'native-ui-assets',tool,rebirth=config.get('pc_game_rebirth')=='1')
                    os.environ['BB_PC_MENU_ASSETS']=str(menus)
                    if config.get('pc_game_rebirth')=='1':os.environ['BB_REBIRTH_ASSET']=str(menus/'dvdroot_ps4/script/talk/m24_02_00_00.talkesdbnd.dcx')
                    else:os.environ.pop('BB_REBIRTH_ASSET',None)
                if layout and config.get('pc_game_options','1')!='1':
                    from bbport_integrated_mirror_assets import prepare_mirror_messages
                    os.environ['BB_MIRROR_MESSAGES']=str(prepare_mirror_messages(game,data/'native-ui-assets'))
                else:os.environ.pop('BB_MIRROR_MESSAGES',None)
                source=Path(os.environ.get('BB_USER_DIR',data/'user'))/'savedata'
                backup=data/'native-ui-assets'/'save-backup-before-appearance'
                if (layout or config.get('pc_game_rebirth')=='1') and source.is_dir() and not backup.exists():
                    temporary=backup.with_name(backup.name+'.tmp')
                    shutil.copytree(source,temporary,dirs_exist_ok=True)
                    os.replace(temporary,backup)
                    print('Espejo: copia de partidas guardada en '+str(backup),flush=True)
                os.environ['BB_ENGINE_IMAGE_SHA256']=identity
                if layout:os.environ['BB_DREAM_MIRROR_ASSET']=str(layout)
                else:os.environ.pop('BB_DREAM_MIRROR_ASSET',None)
                print('Opciones PC: recursos locales preparados; motor original conservado.',flush=True)
            return result
        globals_['run_script']=script
        main()
        return namespace
    ns['runpy'].run_path=run_path
    try:return original_run(argv)
    except Exception as error:
        print('Espejo: no se pudo preparar: '+str(error),flush=True)
        return 1
    finally:ns['runpy'].run_path=original_path


_ACTION_OUTPUTS=set("cross circle square triangle l1 r1 l2 r2 l3 r3 options pad_up pad_down pad_left pad_right touchpad_left touchpad_center touchpad_right axis_left_x_minus axis_left_x_plus axis_left_y_minus axis_left_y_plus axis_right_x_minus axis_right_x_plus axis_right_y_minus axis_right_y_plus axis_left_x axis_left_y axis_right_x axis_right_y leftjoystick_halfmode rightjoystick_halfmode".split())
_CONTROLLER_INPUTS=set("cross circle square triangle l1 r1 l2 r2 l3 r3 options back share pad_up pad_down pad_left pad_right lpaddle_high lpaddle_low rpaddle_high rpaddle_low l4 l5 r4 r5 qam axis_left_x axis_left_y axis_right_x axis_right_y axis_left_x_minus axis_left_x_plus axis_left_y_minus axis_left_y_plus axis_right_x_minus axis_right_x_plus axis_right_y_minus axis_right_y_plus".split())
def controller_input(text):
    out=[]
    for line in text.splitlines():
        if '=' not in line or line.lstrip().startswith(('#',';')):
            out.append(line);continue
        key,value=(p.strip().lower() for p in line.split('=',1))
        if key not in _ACTION_OUTPUTS or value in _CONTROLLER_INPUTS:out.append(line)
    return '\n'.join(out)+'\n'


def prepare_legacy_input(ns,config):
    if config.get('pc_controls_mode')!='ds3':return
    original=Path(os.environ.get('BB_UI_INPUT_ORIGINAL') or ns['input_ini_path'](ns['ini_path']())).resolve()
    if not original.is_file():return
    out=[]
    for line in original.read_text(encoding='utf-8').splitlines():
        if '=' in line and not line.lstrip().startswith(('#',';')):
            key,value=(p.strip().lower() for p in line.split('=',1))
            if key in ('r1','r2','l2') and value not in _CONTROLLER_INPUTS:
                if value in ('f10','f11','f12','leftbutton','middlebutton','rightbutton','sidebuttonback','sidebuttonforward','mousewheelup','mousewheeldown'):continue
        out.append(line)
    out+=['r1 = f10','r2 = f11','l2 = f12']
    target=Path(os.environ.get('BB_DATA_DIR',ns['PORT_DIR']))/'native-ui-assets'/'input-legacy.ini'
    target.parent.mkdir(parents=True,exist_ok=True)
    target.write_text('\n'.join(out)+'\n',encoding='utf-8')
    os.environ['BB_UI_INPUT_ORIGINAL']=str(original)
    os.environ['BB_INPUT_CONFIG']=str(target)

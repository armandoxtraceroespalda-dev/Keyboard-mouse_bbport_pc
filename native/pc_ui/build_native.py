#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build only the native UI/input/camera DLL, without renderer objects.
Usage: python build_native.py OUTPUT_DLL [PATH_TO_CLANG++]
"""
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parent
sources=[root/n for n in ('menu_bridge.cpp','menu_pointer.cpp','native_mirror.cpp','engine_hooks.cpp','menu_memory.cpp')]
sources += [root/'pages'/n for n in ('option_menu.cpp','engine_state.cpp','camera.cpp','rebirth.cpp','bindings.cpp','settings.cpp','input_runtime.cpp','camera_rotation.cpp','key_prompts.cpp','item_cycle.cpp')]
if len(sys.argv)<2:raise SystemExit(__doc__)
output=Path(sys.argv[1]).resolve();output.parent.mkdir(parents=True,exist_ok=True)
compiler=sys.argv[2] if len(sys.argv)>2 else 'x86_64-w64-mingw32-clang++'
subprocess.run([compiler,'-std=c++20','-O2','-Wno-deprecated-declarations','-shared','-static',*[str(p) for p in sources],'-o',str(output),'-luser32','-lkernel32','-Wl,--no-insert-timestamp'],check=True)

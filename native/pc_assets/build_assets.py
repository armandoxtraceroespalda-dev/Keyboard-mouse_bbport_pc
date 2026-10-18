#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build serialization-only assets tool. Usage: OUTPUT_EXE CLANG++ ZLIB_SOURCE_DIR.
The zlib source tarball and license are included; extract it locally first.
"""
from pathlib import Path
import subprocess,sys,tempfile,json
root=Path(__file__).resolve().parent
if len(sys.argv)!=4:raise SystemExit(__doc__)
out=Path(sys.argv[1]).resolve();cxx=Path(sys.argv[2]).absolute();zlib=Path(sys.argv[3]).resolve();cc=cxx.with_name(cxx.name.replace('clang++','clang'))
with tempfile.TemporaryDirectory(prefix='bb-assets-build-') as tmp:
 (Path(tmp)/'pc_option_messages.inc').write_text(json.dumps((root/'pc_option_messages.tsv').read_text(encoding='utf-8'),ensure_ascii=False),encoding='utf-8')
 objects=[]
 for name in ('adler32','compress','crc32','deflate','infback','inffast','inflate','inftrees','trees','uncompr','zutil'):
  obj=Path(tmp)/(name+'.o');objects.append(str(obj));subprocess.run([str(cc),'-O2','-I'+str(zlib),'-c',str(zlib/(name+'.c')),'-o',str(obj)],check=True)
 out.parent.mkdir(parents=True,exist_ok=True)
 subprocess.run([str(cxx),'-std=c++20','-O2','-static','-I'+str(zlib),'-I'+str(tmp),*[str(root/n) for n in ('menu_assets.cpp','rebirth_script.cpp','esd.cpp','sha256.cpp')],*objects,'-o',str(out),'-Wl,--no-insert-timestamp'],check=True)

# SPDX-License-Identifier: GPL-3.0-or-later
"""Linux-only: compare compiled forward policy with the player's 1.09 leaf.
Usage: python test_item_cycle_original.py PATH_TO_PLAINTEXT_EBOOT
No game code is embedded, saved in the source kit, or modified on disk.
"""
from pathlib import Path
import ctypes as C,mmap,random,runpy,struct,subprocess,tempfile
root=Path(__file__).resolve().parent
read=runpy.run_path(str(root/'test_prompt_game_layout.py'))['read']
code=read(0x14df170,0x12a)
assert code[:16]==bytes.fromhex('8b4f4883f9ff0f84c0000000488d571c') and code[-1]==0xc3
page=mmap.mmap(-1,mmap.PAGESIZE,prot=mmap.PROT_READ|mmap.PROT_WRITE|mmap.PROT_EXEC)
page.write(code)
original=C.CFUNCTYPE(None,C.c_void_p)(C.addressof(C.c_char.from_buffer(page)))
with tempfile.TemporaryDirectory() as temp:
 path=Path(temp);source=path/'policy.cpp';binary=path/'policy.so'
 source.write_text('#include "item_cycle.h"\nextern "C" int select_item(const int* p,int current,bool reverse){std::array<int,6> a{};for(int i=0;i<6;++i)a[i]=p[i];return BbEngine::ItemCycle::Select(a,current,reverse);}\n')
 subprocess.run(['g++','-std=c++20','-O2','-shared','-fPIC','-I'+str(root/'pages'),str(source),'-o',str(binary)],check=True)
 policy=C.CDLL(str(binary)).select_item;policy.argtypes=[C.POINTER(C.c_int32),C.c_int32,C.c_bool];policy.restype=C.c_int32
 rng=random.Random(27)
 for case in range(10000):
  slots=[rng.choice([-1,-1,101,102,103,104,105,106]) for i in range(6)];current=rng.choice(slots+[-1,999])
  state=C.create_string_buffer(0x50)
  for i,value in enumerate(slots):struct.pack_into('<i',state,0x1c+8*i,value)
  struct.pack_into('<i',state,0x48,current);original(C.addressof(state))
  expected=struct.unpack_from('<i',state,0x48)[0]
  actual=policy((C.c_int32*6)(*slots),current,False)
  assert actual==expected,(case,slots,current,expected,actual)
print('PASS: compiled item policy matches the actual original forward leaf in 10,000 cases.')

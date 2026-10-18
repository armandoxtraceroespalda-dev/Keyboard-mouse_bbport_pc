// SPDX-License-Identifier: GPL-3.0-or-later
#include "engine_hooks.h"
#include "menu_memory.h"
#include "guest_layout.h"
#include <sys/mman.h>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <vector>
static int calls=0;static bool skip=false;
static int64_t __attribute__((sysv_abi)) callback(uint64_t,const uint64_t* saved){assert(saved[5]==37);calls++;if(skip){const_cast<uint64_t*>(saved)[-1]=99;return 1;}return 0;}
int main(){
 auto* mem=static_cast<uint8_t*>(mmap(nullptr,4096,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));assert(mem!=MAP_FAILED);
 uint8_t prologue[14]={0x55,0x48,0x89,0xe5};std::memset(prologue+4,0x90,10);std::memcpy(mem,prologue,14);
 const uint8_t tail[]={0x89,0xf8,0x5d,0xc3};std::memcpy(mem+14,tail,sizeof tail);
 BbEngine::Hook hook;assert(BbEngine::Prepare(hook,mem,4096,0,prologue,callback));BbEngine::Commit(std::span<BbEngine::Hook>(&hook,1));
 using Fn=int(*)(int);assert(reinterpret_cast<Fn>(mem)(37)==37);assert(calls==1);
 skip=true;assert(reinterpret_cast<Fn>(mem)(37)==99);assert(calls==2);
 BbEngine::Discard(hook);munmap(mem,4096);
 // Native appearance editor memory expansion must reject any changed budget.
 std::vector<uint8_t> image(0x4740000);uint32_t b=18<<20,p=34<<20;uint64_t m=82<<20;
 // The loader copies ELF p_vaddr directly into the image; no BN-base subtraction.
 static_assert(BbEngine::GuestLayout::StepOffset==0x1c1cce0);
 static_assert(BbEngine::GuestLayout::FrameOffset==0x2034770);
 for(auto offset:{BbEngine::GuestLayout::StepOffset,BbEngine::GuestLayout::FrameOffset}){
  std::memcpy(image.data()+offset,prologue,sizeof prologue);
  BbEngine::Hook checked;
  assert(BbEngine::Prepare(checked,image.data(),image.size(),offset,prologue,callback));
  BbEngine::Discard(checked);
  assert(!BbEngine::Prepare(checked,image.data(),image.size(),offset-0x400000,prologue,callback));
 }
 image[0x1f5a465]=image[0x1f5a479]=0xba;
 std::memcpy(image.data()+0x1f5a466,&b,4);std::memcpy(image.data()+0x1f5a47a,&p,4);
 std::memcpy(image.data()+0x4736d40+2*0x80+9*8,&m,8);
 assert(BbEngine::MenuMemory::Compatible(image.data(),image.size()));
 assert(BbEngine::MenuMemory::Install(image.data(),image.size()));
 assert(!BbEngine::MenuMemory::Compatible(image.data(),image.size()));
 puts("PASS: real guest ABI hook preserves arguments, resumes/skips correctly; appearance memory guards.");
}

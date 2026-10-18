// SPDX-License-Identifier: GPL-2.0-or-later
#include "engine_hooks.h"
#include <sys/mman.h>
#include <cassert>
#include <cstring>
#include <cstdio>
static int64_t __attribute__((sysv_abi)) callback(uint64_t,const uint64_t* args){assert(args[5]==37);asm volatile("pxor %%xmm0,%%xmm0":::"xmm0");return 0;}
int main(){
 auto* mem=static_cast<uint8_t*>(mmap(nullptr,4096,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));assert(mem!=MAP_FAILED);
 uint8_t prefix[18];std::memset(prefix,0x90,sizeof prefix);std::memcpy(mem,prefix,sizeof prefix);
 const uint8_t tail[]={0x0f,0xb6,0xc0,0x66,0x48,0x0f,0x7e,0xc2,0x48,0x01,0xd0,0x48,0x01,0xf8,0xc3};std::memcpy(mem+sizeof(prefix),tail,sizeof tail);
 BbEngine::Hook h;assert(BbEngine::Prepare(h,mem,4096,0,prefix,callback,true));BbEngine::Commit(std::span<BbEngine::Hook>(&h,1));
 uint64_t result=0,vector=11;
 asm volatile("movq %2, %%xmm0; call *%3":"=a"(result):"a"(7ULL),"r"(vector),"r"(mem),"D"(37ULL):"rcx","rdx","rsi","r8","r9","r10","r11","xmm0","xmm1","xmm2","xmm3","xmm4","xmm5","xmm6","xmm7","cc","memory");
 assert(result==55);BbEngine::Discard(h);munmap(mem,4096);
 puts("PASS: variadic hook preserves entry AL, XMM0 and integer arguments despite callback clobbers.");
}

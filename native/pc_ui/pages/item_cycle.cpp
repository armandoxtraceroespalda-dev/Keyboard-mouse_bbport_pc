// SPDX-License-Identifier: GPL-3.0-or-later
// Original 1.09 quick-item selection, with an optional reverse request.
#include "item_cycle.h"
#include "../engine_hooks.h"
#include "bbport_platform.h"
#include "engine_state.h"
#include <atomic>
#include <cstring>
#include <windows.h>
namespace {
std::atomic<std::uint64_t> reverse_until{0};
std::uint8_t* image_base=nullptr;
// This is a complete replacement of a leaf, void-returning function. The
// displaced relative branch is never executed, including the forward path.
std::int64_t __attribute__((sysv_abi)) SelectHook(std::uint64_t,const std::uint64_t* saved){
 auto* list=reinterpret_cast<std::uint8_t*>(saved[5]);
 std::array<std::int32_t,6> slots{};std::int32_t selected=-1;
 if(!list||!BbPlatform::ReadProcessMemory(list+0x48,&selected,4))return 1;
 for(int i=0;i<6;++i)if(!BbPlatform::ReadProcessMemory(list+0x1c+8*i,&slots[i],4))return 1;
 // A reverse request only applies to the live player's quick-item list.
 std::uint64_t manager=0,record=0;
 bool player=image_base&&BbPlatform::ReadProcessMemory(image_base+0x553b130,&manager,8)&&manager&&BbPlatform::ReadProcessMemory(reinterpret_cast<void*>(manager+8),&record,8)&&record&&reinterpret_cast<std::uint64_t>(list)==record+0x3d0;
 bool reverse=false;if(player){auto until=reverse_until.exchange(0);reverse=until&&GetTickCount64()<=until;}
 const auto next=BbEngine::ItemCycle::Select(slots,selected,reverse);
 std::memcpy(list+0x48,&next,4);
 return 1;
}
}
void native_item_cycle_request(bool reverse){reverse_until.store(reverse?GetTickCount64()+200:0);}
void native_item_cycle_cancel(){reverse_until.store(0);}
namespace BbEngine::ItemCycle {
bool Install(std::uint8_t* image,std::size_t size){
 // ELF virtual addresses verified against the supplied plaintext executable.
 constexpr std::uint8_t expected[]={0x8b,0x4f,0x48,0x83,0xf9,0xff,0x0f,0x84,0xc0,0,0,0,0x48,0x8d,0x57,0x1c};
 Hook hook{};
 if(!Prepare(hook,image,size,0x14df170,expected,SelectHook)){
  host_log("quick items: rejected: native selection function differs");return false;
 }
 image_base=image;Commit(std::span<Hook>(&hook,1));
 host_log("quick items: native forward/reverse selection installed");return true;
}
}

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstdint>
namespace BbEngine::ItemCycle {
// The native quick-item list stores six inventory handles, with -1 for empty.
inline int Select(const std::array<std::int32_t,6>& slots,std::int32_t selected,bool reverse){
 int current=-1;for(int i=0;i<6;++i)if(slots[i]==selected&&selected!=-1){current=i;break;}
 for(int step=1;step<=6;++step){
  int index=current<0?(reverse?6-step:step-1):(current+(reverse?-step:step)+12)%6;
  if(slots[index]!=-1)return slots[index];
 }
 return -1;
}
bool Install(std::uint8_t* image,std::size_t size);
}
void native_item_cycle_request(bool reverse);
void native_item_cycle_cancel();

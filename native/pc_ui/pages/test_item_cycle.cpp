// SPDX-License-Identifier: GPL-3.0-or-later
#include "item_cycle.h"
#include <cassert>
int main(){
 using BbEngine::ItemCycle::Select;
 std::array<std::int32_t,6> all{10,20,30,40,50,60},sparse{-1,11,-1,22,-1,33},empty{-1,-1,-1,-1,-1,-1};
 for(int i=0;i<6;++i){assert(Select(all,all[i],false)==all[(i+1)%6]);assert(Select(all,all[i],true)==all[(i+5)%6]);}
 assert(Select(sparse,11,false)==22);assert(Select(sparse,11,true)==33);assert(Select(sparse,33,false)==11);assert(Select(sparse,22,true)==11);
 assert(Select(sparse,-1,false)==11);assert(Select(sparse,-1,true)==33);assert(Select(empty,11,true)==-1);
 std::array<std::int32_t,6> one{-1,-1,55,-1,-1,-1};assert(Select(one,55,false)==55&&Select(one,55,true)==55);
}

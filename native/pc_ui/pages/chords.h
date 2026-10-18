// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <mutex>
#include <vector>
#include <string>
#include <algorithm>
#include "bindings.h"
namespace Chords {
struct Binding{std::vector<int> keys;unsigned mouse=0;std::string text;bool active()const{return !text.empty();}};
inline std::mutex lock;
inline std::array<Binding,kBindCount> bindings;
inline std::array<Binding,kBindCount> blocked_by;
inline void Clear(int a){std::lock_guard g(lock);bindings[a]={};blocked_by[a]={};}
inline Binding Get(int a){std::lock_guard g(lock);return bindings[a];}
inline void Set(int a,Binding value){std::lock_guard g(lock);bindings[a]=std::move(value);}
inline bool Matches(const Binding& b,const bool* keys,int count,unsigned mouse){
 if(!b.active()||(b.mouse&mouse)!=b.mouse)return false;
 for(int k:b.keys)if(!keys||k<=0||k>=count||!keys[k])return false;
 return true;
}
inline void Resolve(const bool* keys,int count,unsigned mouse,bool held[kBindCount],bool suppressed[kBindCount]){
 std::array<Binding,kBindCount> all;{std::lock_guard g(lock);all=bindings;}
 for(int a=0;a<kBindCount;++a)if(all[a].active())held[a]=Matches(all[a],keys,count,mouse);
 for(int a=0;a<kBindCount;++a)if(all[a].active()&&held[a]){
  const auto& chord=all[a];
  for(int b=0;b<kBindCount;++b)if(a!=b){
   bool subset=false;
   if(all[b].active()){
    const auto& other=all[b];subset=(other.mouse&chord.mouse)==other.mouse && other.keys.size()+__builtin_popcount(other.mouse)<chord.keys.size()+__builtin_popcount(chord.mouse);
    for(int k:other.keys)subset=subset&&std::find(chord.keys.begin(),chord.keys.end(),k)!=chord.keys.end();
   }else{
    int k=host_binding_key(b),m=host_binding_mouse(b);
    subset=(k&&std::find(chord.keys.begin(),chord.keys.end(),k)!=chord.keys.end()) || (m&&(chord.mouse&(1u<<(m-1))));
    // A held modifier is also read by the legacy DS3 strong-attack policy.
    if(b==kBindStrongMod)subset=false;
   }
   if(subset){held[b]=false;suppressed[b]=true;std::lock_guard g(lock);blocked_by[b]=chord;}
  }
 }
 {std::lock_guard g(lock);for(int a=0;a<kBindCount;++a)if(blocked_by[a].active()){
  bool any=(blocked_by[a].mouse&mouse)!=0;for(int k:blocked_by[a].keys)any=any||(keys&&k>0&&k<count&&keys[k]);
  if(any){held[a]=false;suppressed[a]=true;}else blocked_by[a]={};
 }}
}
}

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "bindings.h"
namespace BindingPages {
inline constexpr int Count=6,Slots=6;
inline constexpr const char* Names[Count]={"Movement","Combat","Items","Camera","Menus","Other"};
inline constexpr int Actions[Count][Slots]={
 {kBindMoveF,kBindMoveB,kBindMoveL,kBindMoveR,kBindWalk,kBindRoll},
 {kBindAttack,kBindStrong,kBindStrongMod,kBindTransform,kBindFirearm,kBindLockOn},
 {kBindBloodVial,kBindUseItem,kBindSwitchItem,kBindPreviousItem,kBindSwitchRight,kBindSwitchLeft},
 {kBindLookU,kBindLookD,kBindLookL,kBindLookR,-1,-1},
 {kBindInteract,kBindMenu,kBindConfirm,kBindBack,kBindGestures,kBindEffects},
 {kBindL3,kBindDpadUp,kBindDebugMenu,-1,-1,-1}
};
inline int Action(int page,int slot){return page>=0&&page<Count&&slot>=0&&slot<Slots?Actions[page][slot]:-1;}
constexpr bool Complete(){int seen[kBindCount]={};for(auto& page:Actions)for(int a:page)if(a>=0)++seen[a];for(int n:seen)if(n!=1)return false;return true;}
static_assert(Complete(),"every action appears in exactly one category");
}

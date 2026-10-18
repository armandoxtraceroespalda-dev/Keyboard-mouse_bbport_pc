// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cstdint>
namespace SharedCamera {
struct Values {int sensitivity;bool invert_x,invert_y,auto_adjust;};
// Verified original menu record: sensitivity +0, X +9, Y +10,
// auto-target +11, auto-adjust +12. Axis choice 1=normal, 0=inverted.
inline bool DecodeMenu(const std::uint8_t* b,Values& v){
 if(b[0]>10||b[9]>1||b[10]>1||b[12]>1)return false;
 v={b[0],b[9]==0,b[10]==0,b[12]!=0};return true;
}
// Serialized game options contain an additional byte before these flags.
inline bool DecodeGame(const std::uint8_t* b,Values& v){
 if(b[0]>10||b[10]>1||b[11]>1||b[13]>1)return false;
 v={b[0],b[10]==0,b[11]==0,b[13]!=0};return true;
}
}

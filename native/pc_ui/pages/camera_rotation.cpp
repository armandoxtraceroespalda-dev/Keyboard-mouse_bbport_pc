// SPDX-License-Identifier: GPL-3.0-or-later
#include "camera_rotation.h"
#include "bbport_settings.h"
#include "native_api.h"
#include "../engine_hooks.h"
#include <cstring>
#include <cstdint>
namespace {
struct Site {std::size_t offset;uint8_t original[9];uint8_t* at=nullptr;bool fixed=false;};
Site sites[]={
 // Never suppress 0x143c6e8: it is the shared final yaw store (+0x144),
 // including manual mouse/stick motion, not an auto-rotation-only write.
 {0x143c984,{0xc4,0xc1,0x7a,0x11,0x8d,0x30,1,0,0}},
 {0x143dde6,{0xc4,0xc1,0x7a,0x11,0x85,0x94,2,0,0}}};
const uint8_t nop[9]={0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90};
}
void native_rotation_install(unsigned char* image,std::size_t size){
 for(auto& s:sites){if(s.offset+9>size)continue;auto* at=image+s.offset;
  if(!std::memcmp(at,s.original,9))s.at=at;
  else if(!std::memcmp(at,nop,9)){s.at=at;s.fixed=true;}
 }
}
void native_rotation_tick(){
 const auto& settings=BbSettings::Get();
 const bool hold=native_mouse_mode() && settings.mouse_camera && !settings.mouse_auto_rotation;
 for(auto& s:sites){if(!s.at||s.fixed)continue;const auto* wanted=hold?nop:s.original;
  if(std::memcmp(s.at,wanted,9) && (!std::memcmp(s.at,nop,9)||!std::memcmp(s.at,s.original,9)))BbEngine::WriteCode(s.at,wanted,9);
 }
}

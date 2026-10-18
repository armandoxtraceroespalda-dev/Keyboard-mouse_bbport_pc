// SPDX-License-Identifier: GPL-3.0-or-later
#include "camera_rotation.cpp"
#include <vector>
#include <cassert>
#include <cstdio>
namespace { BbSettings::Values config;bool mouse=true; }
namespace BbSettings { Values& Get(){return config;} }
bool native_mouse_mode(){return mouse;}
namespace BbEngine { bool WriteCode(void* at,const void* data,std::size_t size){std::memcpy(at,data,size);return true;} }
int main(){
 std::vector<std::uint8_t> image(0x143ddf0);
 constexpr std::size_t yaw=0x143c6e8;
 const std::uint8_t yawStore[]={0xc4,0xc1,0x7a,0x11,0x85,0x44,1,0,0};
 std::memcpy(image.data()+yaw,yawStore,9);
 for(auto& s:sites)std::memcpy(image.data()+s.offset,s.original,9);
 native_rotation_install(image.data(),image.size());
 for(bool autoAdjust:{false,true})for(bool mouseEnabled:{false,true})for(bool mode:{false,true}){
  config.mouse_auto_rotation=autoAdjust;config.mouse_camera=mouseEnabled;mouse=mode;
  native_rotation_tick();
  assert(!std::memcmp(image.data()+yaw,yawStore,9));
  for(auto& s:sites)assert(!std::memcmp(s.at,mode&&mouseEnabled&&!autoAdjust?nop:s.original,9));
 }
 puts("PASS: the final horizontal camera store remains intact in all mouse/auto-adjust modes; remaining auto-rotation sites switch and restore.");
}

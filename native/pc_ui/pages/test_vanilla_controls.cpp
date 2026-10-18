// SPDX-License-Identifier: GPL-3.0-or-later
#include "shared_camera_settings.h"
#include "binding_pages.h"
#include "menu_navigation.h"
#include "language_text.h"
#include <array>
#include <cassert>
#include <string_view>
int main(){
 std::uint8_t menu[16]={};menu[0]=7;menu[9]=1;menu[10]=0;menu[12]=1;SharedCamera::Values v{};
 assert(SharedCamera::DecodeMenu(menu,v)&&v.sensitivity==7&&!v.invert_x&&v.invert_y&&v.auto_adjust);
 menu[11]=1;assert(SharedCamera::DecodeMenu(menu,v)&&v.auto_adjust); // auto-target is independent
 menu[12]=0;assert(SharedCamera::DecodeMenu(menu,v)&&!v.auto_adjust);
 std::uint8_t loaded[16]={};loaded[0]=7;loaded[10]=1;loaded[11]=0;loaded[13]=0;
 assert(SharedCamera::DecodeGame(loaded,v)&&v.sensitivity==7&&!v.invert_x&&v.invert_y&&!v.auto_adjust);
 menu[0]=11;assert(!SharedCamera::DecodeMenu(menu,v));loaded[10]=9;assert(!SharedCamera::DecodeGame(loaded,v));
 int count[kBindCount]={};for(int p=0;p<BindingPages::Count;++p)for(int i=0;i<BindingPages::Slots;++i){int a=BindingPages::Action(p,i);if(a>=0)++count[a];}
 for(int n:count)assert(n==1);assert(BindingPages::Action(1,0)==kBindAttack);
 assert(std::string_view(PcLanguage::Translate("Keyboard and mouse",false))=="Keyboard and mouse");
 assert(std::string_view(PcLanguage::Translate("Keyboard and mouse",true))=="Teclado y ratón");
 assert(std::string_view(PcLanguage::Translate("Movement",true))=="Movimiento");
 alignas(16) std::array<std::uint8_t,128> parent{};
 for(unsigned i=0;i<4;++i){assert(NativeNavigation::PushOwnedChild(parent.data(),0x100+i));std::uint64_t value=0;std::memcpy(&value,parent.data()+0x18+8*i,8);assert(value==0x100+i);}
 auto read=[](const void* p,void* out,std::size_t n){std::memcpy(out,p,n);return true;};
 assert(NativeNavigation::ContainsChild(parent.data(),0x103,read));
 assert(!NativeNavigation::ContainsChild(parent.data(),0x999,read));
 auto countBefore=parent;std::uint64_t zero=0;std::memcpy(parent.data()+0x40,&zero,8);
 assert(!NativeNavigation::ContainsChild(parent.data(),0x103,read));parent=countBefore;
 assert(std::string_view(PcLanguage::Translate("Play with",true))=="Jugar con");
 assert(std::string_view(PcLanguage::Translate("Use Controller or Keyboard and mouse",true))=="Usar Mando o Teclado y ratón");
 auto before=parent;assert(!NativeNavigation::PushOwnedChild(parent.data(),999));assert(parent==before);
 assert(!NativeNavigation::PushOwnedChild(parent.data(),0));
}

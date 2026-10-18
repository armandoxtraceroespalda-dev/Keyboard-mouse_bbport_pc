// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "bindings.h"
#include <string>
#include <string_view>
#include <functional>
#include <array>
namespace PcPrompts {
enum class Role { Context, Confirm, Back, Interact, Roll };
struct Context {bool menu=false,mouse_menu=false,mouse_camera=true;Role role=Role::Context;};
using Reader=std::function<std::string(int)>;
inline std::string compact(std::string s) {
 for(auto [from,to]:{std::pair{"Left Shift","Shift"}, {"Left Ctrl","Ctrl"},{"Left Alt","Alt"},{"Return","Enter"},{"L Mouse","LMB"},{"R Mouse","RMB"},{"M Mouse","MMB"},{"Mouse1","LMB"},{"Mouse2","RMB"},{"Mouse3","MMB"}}) {
  std::size_t at=0;while((at=s.find(from,at))!=s.npos){s.replace(at,std::char_traits<char>::length(from),to);at+=std::char_traits<char>::length(to);}
 }
 return s;
}
inline std::string label(std::u16string_view image,const Context& ctx,const Reader& read) {
 auto key=[&](int a){auto s=compact(read(a));return s.empty()?std::string("-"):s;};
 auto confirm=[&](){return ctx.mouse_menu?std::string("LMB"):key(kBindConfirm);};
 auto back=[&](){return ctx.mouse_menu?std::string("RMB"):key(kBindBack);};
 if(image==u"KG_OK"||image==u"KG_Cancel") {
  if(ctx.role==Role::Interact)return key(kBindInteract);
  if(ctx.role==Role::Roll)return key(kBindRoll);
  if(ctx.role==Role::Confirm)return confirm();
  if(ctx.role==Role::Back)return back();
  if(ctx.menu)return image==u"KG_Cancel"?confirm():back();
  return image==u"KG_Cancel"?key(kBindInteract):key(kBindRoll);
 }
 for(auto [name,action]:{std::pair{u"KG_R1",kBindAttack},{u"KG_L1",kBindTransform},{u"KG_L2",kBindFirearm},
       {u"KG_R3",kBindLockOn},{u"KG_L3",kBindL3},{u"KG_R_U",kBindBloodVial},{u"KG_R_L",kBindUseItem},
       {u"KG_R_D",kBindInteract},{u"KG_Start",kBindMenu},{u"KG_TP_L",kBindGestures},{u"KG_TP_R",kBindEffects},
       {u"KG_L_U",kBindDpadUp},{u"KG_L_D",kBindSwitchItem},{u"KG_L_L",kBindSwitchLeft},{u"KG_L_R",kBindSwitchRight}})
  if(image==name)return key(action);
 if(image==u"KG_R2") {
  auto value=read(kBindStrong);
  if(value.empty()) {auto mod=read(kBindStrongMod),attack=read(kBindAttack);if(!mod.empty()&&!attack.empty())value=mod+" + "+attack;}
  return value.empty()?"-":compact(value);
 }
 auto group=[&](std::initializer_list<int> actions){std::string value;for(int a:actions){if(!value.empty())value+=" / ";value+=key(a);}return value;};
 if(image==u"KG_LS")return group({kBindMoveF,kBindMoveL,kBindMoveB,kBindMoveR});
 if(image==u"KG_RS")return ctx.mouse_camera?"Mouse":group({kBindLookU,kBindLookL,kBindLookD,kBindLookR});
 if(image==u"KG_L_LR")return group({kBindSwitchLeft,kBindSwitchRight});
 if(image==u"KG_L_UD")return group({kBindDpadUp,kBindSwitchItem});
 if(image==u"KG_L_UDLR")return group({kBindDpadUp,kBindSwitchLeft,kBindSwitchItem,kBindSwitchRight});
 return {};
}
inline std::u16string wide(const std::string& s) {
 std::u16string out;
 for(std::size_t i=0;i<s.size();) {
  unsigned c=static_cast<unsigned char>(s[i++]);unsigned need=0;
  if(c>=0xf0){c&=7;need=3;}else if(c>=0xe0){c&=15;need=2;}else if(c>=0xc0){c&=31;need=1;}
  while(need-- && i<s.size())c=(c<<6)|(static_cast<unsigned char>(s[i++])&63);
  if(c>0xffff){c-=0x10000;out.push_back(char16_t(0xd800+(c>>10)));out.push_back(char16_t(0xdc00+(c&1023)));}
  else out.push_back(char16_t(c));
 }
 return out;
}
inline std::u16string text(const std::string& value,bool format=false) {
 std::u16string out=u"[";
 for(char16_t c:wide(value)){if(format&&c==u'%')out+=u"%%";else if(c==u'&')out+=u"&amp;";else if(c==u'<')out+=u"&lt;";else if(c==u'>')out+=u"&gt;";else out.push_back(c);}
 return out+u"]";
}
// Only replace recognized control images. Preserve message variables, artwork,
// localization, and all other HTML byte-for-byte.
inline std::u16string rewrite(std::u16string_view source,const Context& ctx,const Reader& read,bool format=false) {
 std::u16string out;std::size_t position=0;
 while(position<source.size()) {
  auto at=source.find(u"<img",position);
  if(at==source.npos){out+=source.substr(position);break;}
  out+=source.substr(position,at-position);auto end=source.find(u'>',at);
  if(end==source.npos){out+=source.substr(at);break;}
  auto tag=source.substr(at,end-at+1);auto src=tag.find(u"src");std::string value;
  if(src!=tag.npos) {
   auto eq=tag.find(u'=',src+3);
   if(eq!=tag.npos) {auto q=tag.find_first_of(u"\"'",eq+1);if(q!=tag.npos){auto close=tag.find(tag[q],q+1);if(close!=tag.npos)value=label(tag.substr(q+1,close-q-1),ctx,read);}}
  }
  out+=value.empty()?std::u16string(tag):text(value,format);position=end+1;
 }
 return out;
}
}

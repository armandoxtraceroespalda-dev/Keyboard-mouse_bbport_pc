// SPDX-License-Identifier: GPL-3.0-or-later
#include "input_runtime.h"
#include "bindings.h"
#include "compat.h"
#include "native_api.h"
#include "bbport_settings.h"
#include "movement_policy.h"
#include "chords.h"
#include "item_cycle.h"
#include <atomic>
#include <algorithm>
#include <cstring>
#include <mutex>
#ifdef BBUI_TEST
uint64_t test_ticks();
#define GetTickCount64 test_ticks
#else
#include <windows.h>
#endif
namespace {
std::atomic<uint32_t> mouse_buttons{0},wheel{0};
PcMovement movement{};std::atomic<bool> mouse_on{false};bool mouse_quarantine=false,menu_was=false,block_confirm=false,previous_item_was=false;
struct Held {uint64_t actions=0,start=0,until=0;bool was=false;};Held held_mouse[5];
struct PadData {
 uint32_t buttons;uint8_t lx,ly,rx,ry,l2,r2,pad[2];float orientation[4],acceleration[3],angular[3];
 uint8_t touch_count,reserve[3];uint32_t touch_held;struct Touch{uint16_t x,y;uint8_t id,reserve[3];} touch[2];
};
static_assert(offsetof(PadData,touch_count)==52 && offsetof(PadData,touch)==60);
int mouse_id(int raw){return raw==2?3:raw==3?2:raw;}
}
void native_note_context(bool on){mouse_on=on;}
bool native_rebinding(){return host_bind_capture_blocking();}
void native_input_event(void* e){
 if(!native_options_enabled()||!e)return;
 auto* b=static_cast<uint8_t*>(e);uint32_t type;std::memcpy(&type,b,4);
 if(type==0x300){
  int sc;std::memcpy(&sc,b+24,4);const bool active=host_bind_capturing()>=0;
  if(active){host_bind_capture_key(sc);std::memset(b,0,4);}
  else if(sc==76 && BbSettings::Get().keyboard_mouse)host_bind_note_clear_key();
 }
 if(type==0x401||type==0x402){int id=mouse_id(b[24]);if(id<1||id>5)return;auto bit=1u<<(id-1);
  if(type==0x401){mouse_buttons.fetch_or(bit);if(host_bind_capture_mouse(id))std::memset(b,0,4);}
  else mouse_buttons.fetch_and(~bit);
 }
 if(type==0x403){float y;std::memcpy(&y,b+28,4);uint32_t direction;std::memcpy(&direction,b+32,4);if(direction==1)y=-y;const int id=y>0?6:7;
  if(host_bind_capture_mouse(id))std::memset(b,0,4);else wheel.store(id);
 }
 if(type==0x20e || type==0x20f){mouse_buttons=0;wheel=0;} // SDL focus transition
}
void native_apply_input(void* data,const bool* keys,int count,bool focused,bool overlay,bool menu){
 if(!native_options_enabled()||!data)return;
 auto& d=*static_cast<PadData*>(data);const uint32_t mouse=mouse_buttons.load();
 bool any=mouse!=0;for(int i=0;keys&&i<count;i++)any|=keys[i];host_bind_capture_note_held(any);
 if(!focused||overlay||host_bind_capture_blocking()){
  native_item_cycle_cancel();previous_item_was=false;d.buttons=0;d.lx=d.ly=d.rx=d.ry=128;d.l2=d.r2=0;d.touch_count=0;movement={};mouse_quarantine=true;for(auto& h:held_mouse)h={};wheel=0;return;
 }
 bool held[kBindCount]={};host_bindings_keys_held(keys,count,held);
 bool suppressed[kBindCount]={};
 bool previous_item=false,next_item=false;
 const int wheel_input=wheel.exchange(0);
 Chords::Resolve(keys,count,(mouse_on&&!menu&&!mouse_quarantine?mouse:0)|(wheel_input>=6?1u<<(wheel_input-1):0),held,suppressed);
 if(menu&&!menu_was&&held[kBindMenu]&&held[kBindConfirm])block_confirm=true;
 if(!held[kBindConfirm])block_confirm=false;
 if(block_confirm)held[kBindConfirm]=false;
 menu_was=menu;
 // Preserve Options in menus: the same binding opens and closes the menu.
 if(menu){for(int a=kBindLookU;a<=kBindLookR;a++)held[a]=false;}
 // An explicitly recorded Attack chord has its own meaning: Shift inside
 // that chord must not reinterpret R1 as the legacy DS3 strong attack.
 previous_item=held[kBindPreviousItem];next_item=held[kBindSwitchItem];
 PadState p;host_bindings_apply(held,held[kBindStrongMod]&&!Chords::Get(kBindAttack).active(),p);
 if(menu && keys && count>82){if(keys[82])p.buttons|=0x10;if(keys[81])p.buttons|=0x40;if(keys[80])p.buttons|=0x80;if(keys[79])p.buttons|=0x20;}
 // Menu-only Confirm/Back do not leak into world combat input.
 if(!menu){p.buttons&=~(held[kBindConfirm]?menu_confirm_button():0u);p.buttons&=~(held[kBindBack]?menu_back_button():0u);}
 // Own movement shaping retains the working sprint preset while the original
 // sampler continues to supply the physical controller and mouse camera.
 InputConfig cfg{};native_read_original_config(cfg);
 bool dirs[4]={};for(int a=0;a<4;a++){int k=host_binding_key(a);dirs[a]=!suppressed[a]&&(Chords::Get(a).active()?held[a]:keys&&k>0&&k<count&&keys[k]);}
 int walk_mouse=host_binding_mouse(kBindWalk);if(mouse_on&&!menu&&walk_mouse>=1&&walk_mouse<=5&&(mouse&(1u<<(walk_mouse-1))))held[kBindWalk]=true;
 int x=0,y=0;
 const uint64_t now=GetTickCount64();pc_shape_movement(&movement,dirs,held[kBindWalk],now*1000,cfg.stick_release_delay_ms,cfg.stick_turn_rate,cfg.stick_circular,&x,&y);
 p.lx=uint8_t(128+x);p.ly=uint8_t(128+y);
 const int turn=wheel_input;
 if(menu || !mouse_on)mouse_quarantine=true;
 else if(!mouse)mouse_quarantine=false;
 if(mouse_on && !menu && !mouse_quarantine){
  bool mh[kBindCount]={};
  for(int i=0;i<5;i++){
   auto& h=held_mouse[i];const bool down=(mouse&(1u<<i))!=0;
   if(down && !h.was){h={};h.start=now;h.was=true;
    for(int a=0;a<kBindDebugMenu;a++)if(host_binding_mouse(a)==i+1){int action=(a==kBindAttack&&held[kBindStrongMod])?kBindStrong:a;h.actions|=uint64_t(1)<<action;}
   }
   if(!down && h.was){h.was=false;h.until=std::max(now,h.start+50);}
   if(down || now<h.until)for(int a=0;a<kBindDebugMenu;a++)if(h.actions&(uint64_t(1)<<a))mh[a]=true;
  }
  bool wheel_chord=false;if(turn)for(int a=0;a<kBindCount;++a)if(held[a]&&(Chords::Get(a).mouse&(1u<<(turn-1))))wheel_chord=true;
  if(turn&&!wheel_chord){if(held[kBindStrongMod])mh[turn==6?kBindSwitchRight:kBindSwitchLeft]=true;else for(int a=0;a<kBindDebugMenu;a++)if(host_binding_mouse(a)==turn)mh[a]=true;}
  for(int a=0;a<kBindCount;++a)if(suppressed[a])mh[a]=false;
  previous_item|=mh[kBindPreviousItem];next_item|=mh[kBindSwitchItem];
  host_bindings_apply(mh,false,p);
 }else for(auto& h:held_mouse)h={};
 if(menu){native_item_cycle_cancel();previous_item_was=false;}
 else {if(next_item)native_item_cycle_request(false);else if(previous_item&&!previous_item_was)native_item_cycle_request(true);previous_item_was=previous_item;}
 d.buttons|=p.buttons;d.l2=std::max(d.l2,p.l2);d.r2=std::max(d.r2,p.r2);
 auto combine=[](uint8_t a,uint8_t b){return uint8_t(std::clamp(int(a)+int(b)-128,1,255));};
 d.lx=combine(d.lx,p.lx);d.ly=combine(d.ly,p.ly);d.rx=combine(d.rx,p.rx);d.ry=combine(d.ry,p.ry);
 if(p.touch_count){d.touch_count=std::min<uint8_t>(p.touch_count,2);for(int i=0;i<d.touch_count;i++){d.touch[i].x=p.touch[i].x;d.touch[i].y=p.touch[i].y;d.touch[i].id=p.touch[i].id;}}
}

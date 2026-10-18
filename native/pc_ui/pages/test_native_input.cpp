// SPDX-License-Identifier: GPL-3.0-or-later
#define BBUI_TEST
#include "bindings.cpp"
#include "input_runtime.cpp"
#include <cassert>
#include <map>
#include <cstdio>
static uint64_t ticks=1000;
static bool options=true;
namespace BbSettings { Values& Get(){static Values value;return value;} }
uint64_t test_ticks(){return ticks;}
bool native_options_enabled(){return options;}
bool native_mouse_mode(){return true;}
bool native_read_original_config(InputConfig& c){c.stick_release_delay_ms=60;c.stick_turn_rate=900;c.stick_circular=0;return true;}
void native_request_reload(){}
static int item_direction=0;
void native_item_cycle_request(bool reverse){item_direction=reverse?-1:1;}
void native_item_cycle_cancel(){item_direction=0;}
bool native_reserved_key(int k){return k==64;}
int native_scancode_from_name(const char* n){static std::map<std::string,int> names={{"Escape",41},{"Z",29},{"W",26},{"A",4},{"S",22},{"D",7},{"Space",44},{"Left Shift",225},{"Left Alt",226},{"E",8},{"Return",40},{"Backspace",42},{"Q",20},{"F",9},{"C",6},{"R",21},{"Tab",43}};return names[n];}
const char* native_scancode_name(int sc){if(sc==29)return "Z";if(sc==225)return "Left Shift";if(sc==8)return "E";if(sc==44)return "Space";return "key";}
void host_log(const char*,...){}
bool debug_menu_active(){return false;}
uint32_t menu_confirm_button(){return 0x4000;}uint32_t menu_back_button(){return 0x2000;}
static void neutral(PadData& d){std::memset(&d,0,sizeof d);d.lx=d.ly=d.rx=d.ry=128;}
int main(){
 host_bindings_load_defaults();bool keys[512]={};PadData d;neutral(d);
 keys[26]=true;native_apply_input(&d,keys,512,true,false,false);assert(d.ly==1);
 ticks+=17;keys[4]=true;neutral(d);native_apply_input(&d,keys,512,true,false,false);assert(d.ly<64 && d.lx<128); // sprint direction turns without neutral
 ticks+=17;keys[26]=keys[4]=false;neutral(d);native_apply_input(&d,keys,512,true,false,false);assert(d.ly!=128 || d.lx!=128); // release margin
 native_note_context(true);neutral(d);native_apply_input(&d,keys,512,true,false,false);
 // Right mouse in SDL is button 3; native bindings call it Mouse2.
 alignas(8) unsigned char event[128]={};uint32_t type=0x401;std::memcpy(event,&type,4);event[24]=3;
 native_input_event(event);ticks+=70;neutral(d);native_apply_input(&d,keys,512,true,false,false);assert((d.buttons&0x100) && d.l2==255);
 type=0x402;std::memcpy(event,&type,4);native_input_event(event);ticks+=70;
 // Shift + LMB is exclusive and latches until that mouse press is released.
 type=0x401;std::memcpy(event,&type,4);event[24]=1;keys[225]=true;native_input_event(event);
 neutral(d);native_apply_input(&d,keys,512,true,false,false);assert((d.buttons&0x200) && !(d.buttons&0x800));
 keys[225]=false;ticks+=17;neutral(d);native_apply_input(&d,keys,512,true,false,false);assert((d.buttons&0x200) && !(d.buttons&0x800));
 // A capture changes the live action and does not leak its key into gameplay.
 host_bind_capture_begin(kBindAttack);host_bind_capture_note_held(false);assert(host_bind_capture_key(64));assert(host_bind_capturing()==kBindAttack); // reserved
 assert(host_bind_capture_key(29));assert(host_bind_capturing()==kBindAttack && host_binding_key(kBindAttack)!=29);
 keys[29]=true;neutral(d);native_apply_input(&d,keys,512,true,false,false);assert(d.buttons==0 && d.lx==128);
 keys[29]=false;mouse_buttons=0;ticks+=60;neutral(d);native_apply_input(&d,keys,512,true,false,false);
 assert(!host_bind_capture_blocking());assert(host_binding_key(kBindAttack)==29);
 keys[29]=true;ticks+=60;neutral(d);native_apply_input(&d,keys,512,true,false,false);assert(d.buttons&0x800);
 keys[29]=false;keys[40]=true;neutral(d);native_apply_input(&d,keys,512,true,false,false);assert(d.buttons&8);
 ticks+=17;neutral(d);native_apply_input(&d,keys,512,true,false,true);assert(!(d.buttons&0x4000));
 keys[40]=false;keys[41]=true;host_bindings_parse("menu","Escape");ticks+=17;neutral(d);native_apply_input(&d,keys,512,true,false,true);assert(d.buttons==8); // Options closes menu, never Back
 keys[41]=false;host_bindings_parse("menu","Return");ticks+=17;neutral(d);native_apply_input(&d,keys,512,true,false,true);
 keys[40]=true;ticks+=17;neutral(d);native_apply_input(&d,keys,512,true,false,true);assert(d.buttons&0x4000);
 BbSettings::Get().keyboard_mouse=false;neutral(d);d.buttons=0x1000;d.ly=35;keys[26]=true;
 native_apply_input(&d,keys,512,true,false,false);assert((d.buttons&0x1000)&&(d.buttons&8)&&d.ly==1); // keyboard movement and Options coexist with a physical pad
 keys[26]=false;keys[40]=true;host_bindings_parse("menu","Escape");block_confirm=false;neutral(d);
 native_apply_input(&d,keys,512,true,false,true);assert(d.buttons&0x4000); // mode selector remains keyboard-accessible
 BbSettings::Get().keyboard_mouse=true;
 host_bindings_load_defaults();std::memset(keys,0,sizeof keys);mouse_buttons=0;wheel=0;
 keys[8]=true;neutral(d);native_apply_input(&d,keys,512,true,false,false);assert(d.buttons==0x4000); // E = Cross, never Square
 keys[8]=false;keys[44]=true;neutral(d);native_apply_input(&d,keys,512,true,false,false);assert(d.buttons==0x2000); // Space = Circle, never Cross
 std::memset(keys,0,sizeof keys);mouse_buttons=0;host_bind_capture_begin(kBindStrong);host_bind_capture_note_held(false);
 host_bind_capture_key(225);host_bind_capture_mouse(2);host_bind_capture_note_held(true);
 assert(host_bind_capturing()==kBindStrong);host_bind_capture_note_held(false);
 assert(host_bind_capturing()<0 && host_binding_ini_value(kBindStrong)=="Left Shift + Mouse2");
 keys[225]=true;neutral(d);native_apply_input(&d,keys,512,true,false,false);assert(!(d.buttons&0x300)); // modifier alone
 keys[225]=false;neutral(d);native_apply_input(&d,keys,512,true,false,false); // clear quarantine
 keys[225]=true;mouse_buttons=2;neutral(d);native_apply_input(&d,keys,512,true,false,false);
 assert((d.buttons&0x200)&&!(d.buttons&0x100)); // chord wins over plain RMB
 keys[225]=false;neutral(d);native_apply_input(&d,keys,512,true,false,false);assert(!(d.buttons&0x100)); // no plain RMB on modifier release
 mouse_buttons=0;neutral(d);native_apply_input(&d,keys,512,true,false,false);
 host_binding_clear(kBindStrong);host_bindings_parse("attack","Left Shift + Mouse2");
 keys[225]=true;mouse_buttons=2;neutral(d);native_apply_input(&d,keys,512,true,false,false);
 assert((d.buttons&0x800)&&!(d.buttons&0x300)); // explicit light-attack chord stays R1 despite Shift
 keys[225]=false;mouse_buttons=0;neutral(d);native_apply_input(&d,keys,512,true,false,false);
 // Wheel up requests real previous-item selection, never Blood Bullets.
 host_bindings_load_defaults();std::memset(keys,0,sizeof keys);mouse_buttons=0;native_note_context(true);
 neutral(d);native_apply_input(&d,keys,512,true,false,false);
 wheel=6;item_direction=0;neutral(d);native_apply_input(&d,keys,512,true,false,false);
 assert((d.buttons&0x40)&&!(d.buttons&0x10)&&item_direction==-1);
 neutral(d);native_apply_input(&d,keys,512,true,false,false);
 wheel=7;item_direction=0;neutral(d);native_apply_input(&d,keys,512,true,false,false);
 assert((d.buttons&0x40)&&!(d.buttons&0x10)&&item_direction==1);
 item_direction=-1;neutral(d);native_apply_input(&d,keys,512,true,false,true);assert(item_direction==0);
 options=false;neutral(d);d.buttons=0x1000;native_apply_input(&d,keys,512,true,false,false);assert(d.buttons==0x1000);
 puts("PASS: actual native input code: sprint continuity, SDL mouse normalization, exclusive latched strong attack, reserved/capture/live rebind and opt-out.");
}

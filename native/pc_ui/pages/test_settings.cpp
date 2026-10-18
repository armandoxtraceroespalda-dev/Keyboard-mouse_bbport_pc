// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef BBUI_TEST
#define BBUI_TEST
#endif
#include "settings.cpp"
#include "chords.h"
#include <cassert>
static unsigned reloads=0;
int native_scancode_from_name(const char* n){std::string s=n;if(s=="Escape")return 41;if(s=="Left Ctrl")return 224;if(s=="Return")return 40;if(s=="Backspace")return 42;if(s=="Left Shift")return 225;if(s=="Space")return 44;if(s=="Left Alt")return 226;if(s.size()==1){char c=char(std::toupper(s[0]));if(c>='A'&&c<='Z')return c-'A'+4;}return 0;}
const char* native_scancode_name(int k){static thread_local char v[2];if(k==40)return "Return";if(k==42)return "Backspace";if(k==225)return "Left Shift";if(k==44)return "Space";if(k>=4&&k<=29){v[0]=char('A'+k-4);v[1]=0;return v;}return "";}
bool native_read_original_config(InputConfig& cfg){cfg={};for(auto& line:lines(input_path())){auto e=line.find('=');if(e==line.npos)continue;auto k=trim(line.substr(0,e)),v=trim(line.substr(e+1));for(int a=0;a<kBindDebugMenu;a++)if(out_names[a]&&k==out_names[a]){int sc=native_scancode_from_name(v.c_str());if(sc){int out=outputs[a];auto i=cfg.table.binding_count[out]++;cfg.table.bindings[out][i]={IN_KEY,sc,0};}else if(v=="cross"||v=="circle"||v=="r1"){int out=outputs[a];auto i=cfg.table.binding_count[out]++;cfg.table.bindings[out][i]={IN_CBUTTON,v=="cross"?0:v=="circle"?1:10,0};}}}return true;}
void native_request_reload(){reloads++;}
void native_request_mouse_enable(){}
bool native_reserved_key(int){return false;}
bool debug_menu_active(){return false;}
uint32_t menu_confirm_button(){return 0x4000;}uint32_t menu_back_button(){return 0x2000;}
void host_log(const char*,...){}
int main(){
 auto dir=fs::temp_directory_path()/"bb-native-settings-test";fs::create_directories(dir);
 setenv("BB_CONFIG",(dir/"bbport.ini").c_str(),1);setenv("BB_UI_INPUT_ORIGINAL",(dir/"input.ini").c_str(),1);setenv("BB_INPUT_CONFIG",(dir/"controller.ini").c_str(),1);
 atomic_write(config_path(),{"# preserve renderer","resolution=3840x2160","frame_gen=4","pc_gpu_id=1","pc_mouse_light=left","pc_mouse_heavy=shift+left","pc_mouse_firearm=right"});
 atomic_write(input_path(),{"# original controller","cross = e","cross = Return","cross = cross","circle = Space","circle = Backspace","circle = circle","r1 = f10","r1 = r1","axis_left_y_minus = w","axis_left_y = axis_left_y","mouse_camera_sensitivity = 1"});
 BbSettings::Load();assert(BbSettings::Get().key_prompts);assert(BbSettings::Get().mouse_sensitivity==5);assert(host_binding_key(kBindInteract)==8);assert(host_binding_mouse(kBindAttack)==1);assert(host_binding_mouse(kBindFirearm)==2);
 assert(host_binding_key(kBindRoll)==44);assert(host_binding_key(kBindStrongMod)==225);
 host_bindings_parse("interact","Z");BbSettings::Get().mouse_menu=false;BbSettings::Get().camera_fov_scale=1.25f;BbSettings::Get().mouse_sensitivity=8;BbSettings::Save();
 auto main=values(config_path());assert(main["resolution"]=="3840x2160" && main["frame_gen"]=="4" && main["pc_gpu_id"]=="1");assert(main["pc_game_mouse_menu"]=="1");assert(main["pc_bind.interact"]=="Z");
 auto text=lines(input_path());assert(std::find(text.begin(),text.end(),"cross = z")!=text.end());assert(std::find(text.begin(),text.end(),"cross = cross")!=text.end());
 auto physical=lines(std::getenv("BB_INPUT_CONFIG"));assert(std::find(physical.begin(),physical.end(),"cross = z")==physical.end());assert(std::find(physical.begin(),physical.end(),"cross = cross")!=physical.end());
 BbSettings::Load();assert(host_binding_key(kBindInteract)==29);assert(BbSettings::Get().mouse_menu);assert(BbSettings::Get().mouse_sensitivity==8);assert(reloads==1);
 NativePages::PreserveRenderSave([](){atomic_write(config_path(),{"resolution=2560x1440","frame_gen=2"});});
 main=values(config_path());assert(main["resolution"]=="2560x1440" && main["frame_gen"]=="2");assert(main["pc_gpu_id"]=="1" && main["pc_bind.interact"]=="Z");
 BbSettings::SelectKeyboardMouse(false);assert(!BbSettings::Get().mouse_camera&&BbSettings::Get().mouse_menu);assert(!BbSettings::Get().key_prompts);BbSettings::Save();assert(!BbSettings::Get().keyboard_mouse);
 BbSettings::ApplyDs3Profile();assert(host_binding_mouse(kBindPreviousItem)==6 && host_binding_mouse(kBindSwitchItem)==7);assert(host_binding_key(kBindMenu)==41);assert(host_binding_key(kBindInteract)==8);assert(host_binding_key(kBindRoll)==44);assert(host_binding_key(kBindL3)==29);assert(host_binding_mouse(kBindAttack)==1);assert(host_binding_mouse(kBindLockOn)==3);
 host_bindings_parse("interact","Z");BbSettings::SelectKeyboardMouse(false);assert(!BbSettings::Get().mouse_camera&&BbSettings::Get().mouse_menu);assert(!BbSettings::Get().key_prompts);BbSettings::SelectKeyboardMouse(true);assert(BbSettings::Get().mouse_camera&&BbSettings::Get().mouse_menu);assert(BbSettings::Get().key_prompts);assert(host_binding_key(kBindInteract)==29);
 BbSettings::Save();BbSettings::Load();assert(BbSettings::Get().keyboard_mouse && host_binding_key(kBindInteract)==29);
 atomic_write(input_path(),{"cross = z","cross = circle","circle = cross","r1 = r1"});
 assert(NativePages::ControllerBindingLabel(kBindInteract)=="Circle");
 BbSettings::SelectKeyboardMouse(false);assert(!BbSettings::Get().mouse_camera&&BbSettings::Get().mouse_menu);assert(!BbSettings::Get().key_prompts);BbSettings::RestoreControllerDefaults();BbSettings::Save();
 assert(NativePages::ControllerBindingLabel(kBindInteract)=="Cross (X)");
 text=lines(input_path());assert(std::find(text.begin(),text.end(),"cross = cross")!=text.end());assert(std::find(text.begin(),text.end(),"circle = circle")!=text.end());assert(std::find(text.begin(),text.end(),"cross = circle")==text.end());assert(host_binding_key(kBindInteract)==29);
 host_bindings_parse("interact","Left Ctrl + E");BbSettings::SelectKeyboardMouse(true);assert(BbSettings::Get().mouse_camera&&BbSettings::Get().mouse_menu);assert(BbSettings::Get().key_prompts);BbSettings::Save();
 assert(values(config_path())["pc_bind.interact"]=="Left Ctrl + E");
 BbSettings::Load();assert(host_binding_ini_value(kBindInteract)=="Left Ctrl + E");
 assert(host_binding_key(kBindInteract)==0 && Chords::Get(kBindInteract).keys.size()==2);
 BbSettings::Get().key_prompts=false;BbSettings::Save();BbSettings::Load();assert(!BbSettings::Get().key_prompts);
 update(config_path(),{{"pc_keyboard_mouse","0"},{"pc_mouse_camera","1"},{"pc_game_mouse_menu","0"}});
 BbSettings::Load();assert(!BbSettings::Get().mouse_camera&&BbSettings::Get().mouse_menu);
 update(config_path(),{{"pc_keyboard_mouse","1"},{"pc_mouse_camera","0"}});
 BbSettings::Load();assert(BbSettings::Get().mouse_camera&&BbSettings::Get().mouse_menu);
 puts("PASS: actual native settings persistence, chord reload, controller preservation, isolated physical input, renderer/GPU/FG preservation.");
}

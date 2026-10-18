// SPDX-License-Identifier: GPL-3.0-or-later
#include "bbport_settings.h"
#include "bindings.h"
#include "compat.h"
#include "native_api.h"
#ifndef BBUI_TEST
#include <windows.h>
#endif
#include <filesystem>
#include <fstream>
#include <sstream>
#include <map>
#include <set>
#include <mutex>
#include <algorithm>
#include <cctype>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
namespace fs=std::filesystem;
namespace {
std::mutex file_lock;
BbSettings::Values settings;
uint64_t saved_serial=0;
bool restore_controller=false;
bool kbm_profile_ready=false;
std::string trim(std::string s){auto a=s.find_first_not_of(" \t\r\n"),b=s.find_last_not_of(" \t\r\n");return a==s.npos?"":s.substr(a,b-a+1);}
std::vector<std::string> lines(const fs::path& path){std::ifstream f(path);std::vector<std::string> out;std::string s;while(std::getline(f,s))out.push_back(s);return out;}
std::map<std::string,std::string> values(const fs::path& path){std::map<std::string,std::string> out;for(auto s:lines(path)){auto e=s.find('=');if(e==s.npos||s.empty()||s[0]=='#'||s[0]==';')continue;out[trim(s.substr(0,e))]=trim(s.substr(e+1));}return out;}
fs::path config_path(){const char* p=std::getenv("BB_CONFIG");return p?p:"bbport.ini";}
fs::path input_path(){const char* p=std::getenv("BB_UI_INPUT_ORIGINAL");return p?fs::path(p):config_path().parent_path()/"input.ini";}
void atomic_write(const fs::path& path,const std::vector<std::string>& text){fs::create_directories(path.parent_path());auto tmp=path;tmp+=".native-tmp";{std::ofstream out(tmp,std::ios::binary);for(auto& s:text)out<<s<<'\n';out.flush();if(!out)throw std::runtime_error("cannot write PC settings");}
#ifdef BBUI_TEST
fs::rename(tmp,path);
#else
if(!MoveFileExW(tmp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("cannot replace PC settings");
#endif
}
void update(const fs::path& path,const std::map<std::string,std::string>& entries){auto text=lines(path);std::set<std::string> seen;std::vector<std::string> out;for(auto& s:text){auto e=s.find('=');std::string k=e==s.npos?"":trim(s.substr(0,e));auto v=entries.find(k);if(v==entries.end())out.push_back(s);else if(seen.insert(k).second)out.push_back(k+"="+v->second);}for(auto& [k,v]:entries)if(!seen.count(k))out.push_back(k+"="+v);atomic_write(path,out);}
const int outputs[kBindCount]={OUT_AXIS_LEFT_Y_MINUS,OUT_AXIS_LEFT_Y_PLUS,OUT_AXIS_LEFT_X_MINUS,OUT_AXIS_LEFT_X_PLUS,OUT_LEFTJOYSTICK_HALFMODE,OUT_CIRCLE,OUT_R3,OUT_R1,OUT_R2,-1,OUT_L1,OUT_L2,OUT_TRIANGLE,OUT_SQUARE,OUT_CROSS,OUT_PAD_DOWN,OUT_PAD_RIGHT,OUT_PAD_LEFT,OUT_PAD_UP,OUT_TOUCHPAD_LEFT,OUT_TOUCHPAD_RIGHT,OUT_AXIS_RIGHT_Y_MINUS,OUT_AXIS_RIGHT_Y_PLUS,OUT_AXIS_RIGHT_X_MINUS,OUT_AXIS_RIGHT_X_PLUS,OUT_OPTIONS,-1,-1,OUT_L3,-1,-1};
const char* out_names[kBindCount]={"axis_left_y_minus","axis_left_y_plus","axis_left_x_minus","axis_left_x_plus","leftjoystick_halfmode","circle","r3","r1","r2",nullptr,"l1","l2","triangle","square","cross","pad_down","pad_right","pad_left","pad_up","touchpad_left","touchpad_right","axis_right_y_minus","axis_right_y_plus","axis_right_x_minus","axis_right_x_plus","options",nullptr,nullptr,"l3",nullptr,nullptr};
const std::set<std::string> controller={"cross","circle","square","triangle","l1","r1","l2","r2","l3","r3","options","back","share","pad_up","pad_down","pad_left","pad_right","lpaddle_high","lpaddle_low","rpaddle_high","rpaddle_low","l4","l5","r4","r5","qam","axis_left_x","axis_left_y","axis_right_x","axis_right_y","axis_left_x_minus","axis_left_x_plus","axis_left_y_minus","axis_left_y_plus","axis_right_x_minus","axis_right_x_plus","axis_right_y_minus","axis_right_y_plus"};
std::string token(int sc){std::string s=NativePages::KeyName(sc);std::map<std::string,std::string> aliases={{"Return","enter"},{"Left Shift","lshift"},{"Right Shift","rshift"},{"Left Ctrl","lctrl"},{"Right Ctrl","rctrl"},{"Left Alt","lalt"},{"Right Alt","ralt"},{"Keypad Enter","kpenter"},{"PageUp","pgup"},{"PageDown","pgdown"},{"Pause","pausebreak"},
 {"Left GUI","lmeta"},{"Right GUI","rmeta"},{"`","grave"},{"-","minus"},{"=","equals"},{"[","lbracket"},{"]","rbracket"},{"\\","backslash"},{";","semicolon"},{"'","apostrophe"},{",","comma"},{".","period"},{"/","slash"},
 {"Keypad 0","kp0"},{"Keypad 1","kp1"},{"Keypad 2","kp2"},{"Keypad 3","kp3"},{"Keypad 4","kp4"},{"Keypad 5","kp5"},{"Keypad 6","kp6"},{"Keypad 7","kp7"},{"Keypad 8","kp8"},{"Keypad 9","kp9"},{"Keypad .","kpperiod"},{"Keypad ,","kpcomma"},{"Keypad /","kpslash"},{"Keypad *","kpasterisk"},{"Keypad -","kpminus"},{"Keypad +","kpplus"},{"Keypad =","kpequals"}};if(aliases.count(s))return aliases[s];s.erase(std::remove(s.begin(),s.end(),' '),s.end());std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return char(std::tolower(c));});return s;}
std::string mouse_token(int m){const char* names[]={"","leftbutton","rightbutton","middlebutton","sidebuttonback","sidebuttonforward","mousewheelup","mousewheeldown"};return m>=1&&m<=7?names[m]:"";}
void persist_bindings(std::map<std::string,std::string>& main){
 std::map<std::string,std::vector<std::string>> replacements;
 for(int a=0;a<kBindDebugMenu;a++){
  main["pc_bind."+std::string(host_binding_info(a).key)]=host_binding_ini_value(a);
  if(!out_names[a])continue;
  auto& r=replacements[out_names[a]];const int key=host_binding_key(a),mouse=host_binding_mouse(a);
  if(key){auto t=token(key);if(!t.empty())r.push_back(t);}
  if(mouse)r.push_back(mouse_token(mouse));
  if(r.empty())r.push_back("unmapped");
 }
 auto source=lines(input_path());std::vector<std::string> out;
 for(auto& s:source){auto e=s.find('=');if(e==s.npos){out.push_back(s);continue;}auto k=trim(s.substr(0,e)),v=trim(s.substr(e+1));if(restore_controller && controller.count(v))continue;if(!replacements.count(k)||controller.count(v))out.push_back(s);}
 if(restore_controller){
  for(const char* name:{"cross","circle","square","triangle","l1","r1","l2","r2","l3","r3","options","pad_up","pad_down","pad_left","pad_right","axis_left_x","axis_left_y","axis_right_x","axis_right_y"})out.push_back(std::string(name)+" = "+name);
  out.push_back("touchpad_left = back");
 }
 for(auto& [k,vs]:replacements)for(auto& v:vs)out.push_back(k+" = "+v);
 atomic_write(input_path(),out);restore_controller=false;
 auto combo=[](int m){const char* n[]={"none","left","right","middle","side_back","side_forward"};return std::string(m>=1&&m<=5?n[m]:"none");};
 main["pc_mouse_light"]=combo(host_binding_mouse(kBindAttack));
 main["pc_mouse_firearm"]=combo(host_binding_mouse(kBindFirearm));
 const int heavy=host_binding_mouse(kBindStrong),light=host_binding_mouse(kBindAttack);
 main["pc_mouse_heavy"]=heavy?combo(heavy):(light&&host_binding_key(kBindStrongMod)?"shift+"+combo(light):"none");
}
}
#ifndef BBUI_TEST
int native_scancode_from_name(const char* name){using F=int(*)(const char*);static auto f=reinterpret_cast<F>(GetProcAddress(GetModuleHandleW(L"SDL3.dll"),"SDL_GetScancodeFromName"));return f?f(name):0;}
const char* native_scancode_name(int sc){using F=const char*(*)(int);static auto f=reinterpret_cast<F>(GetProcAddress(GetModuleHandleW(L"SDL3.dll"),"SDL_GetScancodeName"));const char* s=f?f(sc):"";return s?s:"";}
#endif
namespace BbSettings {
Values& Get(){return settings;}
void Load(){
 auto main=values(config_path()),input=values(input_path());
 auto b=[&](const char* k,bool d){auto i=main.find(k);return i==main.end()?d:i->second=="1";};
 auto fl=[&](const char* k,float d){try{return main.count(k)?std::stof(main[k]):d;}catch(...){return d;}};
 settings.keyboard_mouse=b("pc_keyboard_mouse",true);
 settings.key_prompts=b("pc_key_prompts",true);
 kbm_profile_ready=b("pc_kbm_profile_ready",main["pc_controls_mode"]=="ds3");
 settings.mouse_menu=true;settings.change_appearance=b("pc_game_mirror",false);settings.rebirth=b("pc_game_rebirth",false);settings.camera_controls=b("pc_camera_controls",true);
 settings.mouse_camera=settings.keyboard_mouse.load();settings.mouse_invert_x=b("pc_mouse_invert_x",false);settings.mouse_invert_y=b("pc_mouse_invert_y",false);settings.mouse_auto_rotation=b("pc_mouse_auto_rotation",false);
 float sensitivity=1;try{if(input.count("mouse_camera_sensitivity"))sensitivity=std::stof(input["mouse_camera_sensitivity"]);}catch(...){}
 settings.mouse_sensitivity=std::clamp(int(std::lround(sensitivity*5)),0,10);
 settings.camera_fov_scale=std::clamp(fl("pc_camera_fov",1),1.f,1.5f);settings.camera_distance_scale=std::clamp(fl("pc_camera_distance",1),.5f,1.5f);settings.camera_height_scale=std::clamp(fl("pc_camera_height",1),.5f,1.5f);
 NativePages::LoadBindings();saved_serial=host_bindings_serial();
}
void Save(){
 std::lock_guard lock(file_lock);
 settings.mouse_menu=true;settings.mouse_camera=settings.keyboard_mouse.load();
 std::map<std::string,std::string> main;
 auto b=[&](const char* k,bool v){main[k]=v?"1":"0";};
 b("pc_keyboard_mouse",settings.keyboard_mouse);main["pc_controls_mode"]=settings.keyboard_mouse?"ds3":"standard";
 b("pc_kbm_profile_ready",kbm_profile_ready);
 b("pc_key_prompts",settings.key_prompts);b("pc_game_mouse_menu",settings.mouse_menu);b("pc_game_mirror",settings.change_appearance);b("pc_game_rebirth",settings.rebirth);b("pc_camera_controls",settings.camera_controls);b("pc_mouse_camera",settings.mouse_camera);b("pc_mouse_invert_x",settings.mouse_invert_x);b("pc_mouse_invert_y",settings.mouse_invert_y);b("pc_mouse_auto_rotation",settings.mouse_auto_rotation);
 main["pc_camera_fov"]=std::to_string(settings.camera_fov_scale.load());main["pc_camera_distance"]=std::to_string(settings.camera_distance_scale.load());main["pc_camera_height"]=std::to_string(settings.camera_height_scale.load());
 auto before=values(config_path());
 const bool enable_mouse=settings.mouse_camera && before.count("pc_mouse_camera") && before["pc_mouse_camera"]=="0";
 persist_bindings(main);update(config_path(),main);
 update(input_path(),{{"mouse_camera_sensitivity",std::to_string(std::max(.05f,settings.mouse_sensitivity.load()/5.f))},{"mouse_camera","direct"},{"mouse_to_joystick","right"}});
 // The original engine continues reading controller bindings and its mouse
 // camera configuration. Keyboard/mouse actions come from our native registry.
 std::vector<std::string> physical;
 for(auto& s:lines(input_path())){
  auto e=s.find('=');if(e==s.npos){physical.push_back(s);continue;}
  auto k=trim(s.substr(0,e)),v=trim(s.substr(e+1));
  bool action=false;for(auto* n:out_names)if(n && k==n){action=true;break;}
  if(!action || controller.count(v))physical.push_back(s);
 }
 const char* path=std::getenv("BB_INPUT_CONFIG");if(path)atomic_write(path,physical);
 if(enable_mouse)native_request_mouse_enable();
 native_request_reload();saved_serial=host_bindings_serial();
}
void ApplyDs3Profile(){
 host_bindings_load_defaults();
 host_bindings_parse("switch_item","Down, WheelDown");host_bindings_parse("previous_item","WheelUp");
 host_bindings_parse("menu","Escape");host_bindings_parse("l3","Z");
 host_bindings_parse("lock_on","Q, Mouse3");host_bindings_parse("firearm","Left Ctrl, Mouse2");
 for(const char* name:{"look_up","look_down","look_left","look_right"})host_bindings_parse(name,"none");
 settings.keyboard_mouse=true;settings.mouse_camera=true;settings.mouse_sensitivity=5;kbm_profile_ready=true;
}
void RestoreControllerDefaults(){restore_controller=true;}
void SelectKeyboardMouse(bool enabled){settings.keyboard_mouse=enabled;settings.key_prompts=enabled;settings.mouse_camera=enabled;settings.mouse_menu=true;}
}
namespace NativePages {
std::string ControllerBindingLabel(int a){
 if(a<0||a>=kBindCount)return "--";
 if(a==kBindStrongMod||a==kBindDebugMenu)return "--";
 if(a==kBindWalk)return "Left stick tilt";
 if(a==kBindGestures)return "Touchpad left";
 if(a==kBindEffects)return "Touchpad right";
 InputConfig cfg{};if(!native_read_original_config(cfg))return "--";
 int output=outputs[a];
 if(a==kBindConfirm)output=menu_confirm_button()==0x4000?OUT_CROSS:OUT_CIRCLE;
 if(a==kBindBack)output=menu_back_button()==0x2000?OUT_CIRCLE:OUT_CROSS;
 if(output<0)return "--";
 const char* buttons[]={"Cross (X)","Circle","Square","Triangle","SHARE","PS","OPTIONS","L3","R3","L1","R1","D-pad up","D-pad down","D-pad left","D-pad right"};
 auto describe=[&](int out)->std::string{
  for(int i=0;i<cfg.table.binding_count[out];++i){const auto& b=cfg.table.bindings[out][i];
   if(b.kind==IN_CBUTTON && b.value>=0&&b.value<15)return buttons[b.value];
   if(b.kind==IN_AXIS||b.kind==IN_AXIS_HALF){
    if(b.value==4)return "L2";if(b.value==5)return "R2";
    if(b.value>=0&&b.value<4){
     const bool minus=(a==kBindMoveF||a==kBindMoveL||a==kBindLookU||a==kBindLookL);
     return std::string(b.value<2?"Left stick ":"Right stick ")+((b.value%2)?(minus?"up":"down"):(minus?"left":"right"));
    }
   }
  }return "--";
 };
 auto label=describe(output);
 if(label=="--" && a<=kBindMoveR)label=describe((a<=kBindMoveB)?OUT_AXIS_LEFT_Y:OUT_AXIS_LEFT_X);
 if(label=="--" && a>=kBindLookU&&a<=kBindLookR)label=describe((a<=kBindLookD)?OUT_AXIS_RIGHT_Y:OUT_AXIS_RIGHT_X);
 return label;
}
void PreserveRenderSave(void(*save)()){
 std::lock_guard guard(file_lock);
 const auto before=values(config_path());save();const auto after=values(config_path());
 std::map<std::string,std::string> missing;
 for(const auto& [k,v]:before)if(!after.count(k))missing[k]=v;
 if(!missing.empty())update(config_path(),missing);
}
void Tick(){if(host_bindings_serial()!=saved_serial)BbSettings::Save();}
int Scancode(const char* n){return native_scancode_from_name(n);}const char* KeyName(int sc){return native_scancode_name(sc);}
void LoadBindings(){
 host_bindings_load_defaults();InputConfig cfg{};auto main=values(config_path());
 if(native_read_original_config(cfg)){
  for(int a=0;a<kBindDebugMenu;a++){
   if(outputs[a]<0)continue;int key=0,mouse=0;
   // input.ini can have secondary aliases (E then Enter, Space then
   // Backspace). Native rows hold one primary key: preserve the first real
   // binding instead of silently replacing it with the final alias.
   for(int i=0;i<cfg.table.binding_count[outputs[a]];i++){auto& b=cfg.table.bindings[outputs[a]][i];if(b.kind==IN_KEY && !key && !(a>=kBindAttack&&a<=kBindFirearm&&b.value>=67&&b.value<=69))key=b.value;else if(b.kind==IN_MOUSE_BUTTON && !mouse)mouse=b.value==2?3:b.value==3?2:b.value;else if(b.kind==IN_MOUSE_WHEEL && !mouse && b.value<=1)mouse=b.value==0?6:7;}
   std::string text=key?KeyName(key):"";if(mouse){if(!text.empty())text+=", ";text+=mouse==6?"WheelUp":mouse==7?"WheelDown":"Mouse"+std::to_string(mouse);}host_bindings_parse(host_binding_info(a).key,text.empty()?"none":text.c_str());
  }
 }
 for(int a=0;a<kBindDebugMenu;++a){auto k="pc_bind."+std::string(host_binding_info(a).key);if(main.count(k))host_bindings_parse(host_binding_info(a).key,main[k].c_str());}
 // Old presets assigned WheelUp to Blood Bullets. On the first upgrade only,
 // release that inherited wheel alias and initialize the two traversal actions.
 if(kbm_profile_ready && !main.count("pc_bind.previous_item")){
  if(host_binding_mouse(kBindDpadUp)==6){auto key=host_binding_key(kBindDpadUp);host_bindings_parse("dpad_up",key?KeyName(key):"none");}
  if(!host_binding_mouse(kBindSwitchItem)&&(!main.count("pc_bind.switch_item")||main["pc_bind.switch_item"]=="Down"))host_bindings_parse("switch_item","Down, WheelDown");
 }
 // Combat mouse selectors in the launcher remain authoritative on startup.
 for(auto entry:{std::pair{kBindAttack,"pc_mouse_light"},std::pair{kBindStrong,"pc_mouse_heavy"},std::pair{kBindFirearm,"pc_mouse_firearm"}}){
  if(!main.count(entry.second))continue;auto value=main[entry.second];bool shifted=value.rfind("shift+",0)==0;if(shifted)value=value.substr(6);
  const std::map<std::string,int> names={{"left",1},{"right",2},{"middle",3},{"side_back",4},{"side_forward",5},{"none",0}};
  int m=names.count(value)?names.at(value):0;if(entry.first==kBindStrong&&shifted)m=0;
  std::string t=host_binding_key(entry.first)?KeyName(host_binding_key(entry.first)):"";if(m){if(!t.empty())t+=", ";t+="Mouse"+std::to_string(m);}host_bindings_parse(host_binding_info(entry.first).key,t.empty()?"none":t.c_str());
 }
 // input.ini remains compatible with the original parser; simultaneous
 // combinations live in pc_bind.* and are evaluated only by the native layer.
 for(int a=0;a<kBindDebugMenu;++a){auto key="pc_bind."+std::string(host_binding_info(a).key);if(main.count(key)&&main[key].find(" + ")!=std::string::npos)host_bindings_parse(host_binding_info(a).key,main[key].c_str());}
}
}

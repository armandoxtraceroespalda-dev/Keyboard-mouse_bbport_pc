// SPDX-License-Identifier: GPL-3.0-or-later
// Source-native PC UI module. The host supplies a typed, versioned API.
// No host executable offsets, class-layout offsets or binary patching.
#include "compat.h"
#include "../../src/runtime_pc_ui_api.h"
#include "menu_keys_policy.h"
#include "pages/bbport_settings.h"
#include "pages/option_menu.h"
#include "pages/input_runtime.h"
#include "pages/bindings.h"
#include "pages/native_api.h"
#include "native_mirror.h"
#include "runtime_input_config.h"
#include "physical_input.h"
#include <atomic>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <mutex>

static_assert(sizeof(BbPcHostApi)==64 && sizeof(BbPcUiApi)==88, "C/C++ interface ABI");
static BbPcHostApi host_api{};
static std::atomic<bool> enabled{false},installed{false},capture{false},raw_mouse_mode{false};
static std::atomic<uint32_t> edges{0},wheel_bits{0};
static std::atomic<uint64_t> guest_base{0};
static std::atomic<uint32_t> pad_buttons{0};
static std::atomic<uint8_t> pad_x{128},pad_y{128};
static std::atomic<int> last_device{0};
static uint64_t pulse_until=0;
static uint32_t pulse=0;
static std::atomic<uint64_t> movement_until{0};
static std::atomic<bool> reload_pending{false},enable_mouse_pending{false};
static InputConfig original_config{};static std::atomic<bool> original_config_ready{false};
static std::mutex config_lock;
static void refresh_original_config();

void host_log(const char* fmt,...){std::fputs("Native UI: ",stdout);va_list a;va_start(a,fmt);std::vprintf(fmt,a);va_end(a);std::putchar('\n');std::fflush(stdout);}
static bool focused(){DWORD pid=0;HWND w=GetForegroundWindow();if(!w)return false;GetWindowThreadProcessId(w,&pid);return pid==GetCurrentProcessId();}
HostSettings host_settings(){return {installed && enabled && focused()};}
InputDevice host_input_device(){return last_device.load()?InputDevice::Pad:InputDevice::KeyboardMouse;}
bool host_options_open(){return host_api.overlay_open && host_api.overlay_open()!=0;}
bool host_text_entry_open(){return host_options_open();}
bool ingame_menu_open(){return false;}
bool debug_menu_open(){return false;}
bool option_menu_click_decides(uint8_t* p,int i){return BbEngine::Options::ClickDecides(p,i);}
void option_menu_list_update(uint8_t* p,bool focus){BbEngine::Options::ListUpdate(p,focus);}
bool host_mouse_relative(){return capture;}
void host_mouse_describe(char* s,size_t n){std::snprintf(s,n,"original-engine pointer");}
static bool world_started(){
 uint64_t flow=0;uint8_t start=0;auto base=guest_base.load();
 return base && host_read_safe(reinterpret_cast<void*>(base+0x5980e38),&flow,8) && flow && host_read_safe(reinterpret_cast<void*>(flow+0xb11),&start,1) && start;
}
bool native_keyboard_movement(){
 if(!installed || !enabled || !focused() || !world_started() || host_options_open())return false;
 int count=0;const bool* keys=host_api.keyboard_state(&count);
 if(!keys)return false;
 InputConfig local{};const InputConfig* cfg=native_read_original_config(local)?&local:nullptr;
 if(native_options_enabled()){
  if(!BbSettings::Get().keyboard_mouse)return false;
  for(int a=kBindMoveF;a<=kBindMoveR;a++){int k=host_binding_key(a);if(k>0&&k<count&&keys[k]){movement_until=GetTickCount64()+100;return true;}}
  return GetTickCount64()<movement_until.load();
 }
 if(!cfg)return false;
 for(int a=OUT_AXIS_LEFT_X_MINUS;a<=OUT_AXIS_LEFT_Y_PLUS;a++){
  for(int i=0;i<cfg->table.binding_count[a] && i<INPUT_MAX_BINDINGS_PER_OUTPUT;i++){
   const auto& b=cfg->table.bindings[a][i];
   if(b.kind==IN_KEY && b.value>=0 && b.value<count && keys[b.value]){
    movement_until=GetTickCount64()+std::max(100,std::min(250,cfg->stick_release_delay_ms+40));return true;
   }
  }
 }
 return GetTickCount64()<movement_until.load();
}
PadState host_pad_state(){PadState p;p.buttons=pad_buttons;p.lx=pad_x;p.ly=pad_y;
 // Gameplay WASD must not veto mouse hovering in a list.
 if(native_keyboard_movement()){p.lx=p.ly=128;}
 return p;}
bool host_mouse_stage_position(float& x,float& y){
 if(!host_settings().mouse_menu || capture)return false;
 HWND w=GetForegroundWindow();POINT pt;RECT r;
 if(!GetCursorPos(&pt)||!ScreenToClient(w,&pt)||!GetClientRect(w,&r))return false;
 const float width=float(r.right-r.left),height=float(r.bottom-r.top);
 if(width<=0||height<=0)return false;
 const float scale=std::min(width/1920.f,height/1080.f);
 x=(pt.x-(width-1920.f*scale)*.5f)/scale;
 y=(pt.y-(height-1080.f*scale)*.5f)/scale;
 return x>=0 && y>=0 && x<1920 && y<1080;
}

static void InstallImage(uint8_t* image,size_t size){
 if(!std::getenv("BB_NATIVE_MENUS") && !std::getenv("BB_NATIVE_MIRROR") && !native_options_enabled())return;
 if(native_options_enabled())BbSettings::Load();
 // Both possible linker coordinate systems are checked structurally. Never
 // assume that a guest RVA and a preferred-address disassembly are the same.
 intptr_t adjustment=0;bool found=false;
 const uint8_t padding[]={0x66,0x66,0x66,0x66,0x66,0x2e,0x0f,0x1f,0x84,0,0,0,0,0};
 for(intptr_t a:{intptr_t(0),-intptr_t(0x400000)}){
  if(size<=size_t(0x5799fb0+a+32))continue;
  uint64_t target=0;std::memcpy(&target,image+0x5799fb0+a+24,8);
  if(target==reinterpret_cast<uint64_t>(image+0x1ecff30+a) && !std::memcmp(image+0x1ec1e02+a,padding,14)) {adjustment=a;found=true;break;}
 }
 if(!found){host_log("ERROR: unsupported game menu layout; no guest code changed");ExitProcess(1);}
 const auto base=reinterpret_cast<uint64_t>(image)+adjustment;
 guest_base=base;
 ElfImage elf{{base+0x400000,size-size_t(0x400000+adjustment)}};
 if(std::getenv("BB_NATIVE_MENUS") || native_options_enabled())menu_pointer_install(&elf);
 if((std::getenv("BB_NATIVE_MENUS") || native_options_enabled()) && !menu_pointer_ready()){host_log("ERROR: menu hook signatures differ; launch stopped");ExitProcess(1);}
 if(std::getenv("BB_NATIVE_MIRROR") || native_options_enabled()){
   if(adjustment!=-intptr_t(0x400000)) {host_log("ERROR: PC lifecycle requires the verified image coordinate layout; launch stopped");ExitProcess(1);}
   if(!native_mirror_install(image,size)) {host_log("ERROR: native PC initialization failed; see the specific check above; launch stopped");std::fflush(stdout);ExitProcess(1);}
 }
 installed=std::getenv("BB_NATIVE_MENUS")!=nullptr || native_options_enabled();host_log("Original engine: native menu pointer installed; title WASD retained, world list input filtered independently of player pad");
}

static bool WindowMode(bool saved){
 if(enable_mouse_pending.exchange(false))saved=true;
 raw_mouse_mode=saved;native_note_context(saved);
 const bool allowed=native_options_enabled()?BbSettings::Get().mouse_menu.load():std::getenv("BB_NATIVE_MENUS")!=nullptr;
 enabled=installed && saved && allowed;
 return saved;
}
static bool TakeReload(){if(!reload_pending.exchange(false))return false;refresh_original_config();return true;}
static bool CaptureAllowed(){return !(enabled && menu_pointer_in_menu());}
static void CaptureNote(bool value){capture=value;}
static void NoteEvent(void* event){
 if(!event||!installed)return;
 native_input_event(event);
 auto* b=static_cast<uint8_t*>(event);uint32_t type;std::memcpy(&type,b,4);
 if(type==0x300 || type==0x400)last_device=0;
 if(type==0x651 || type==0x652)last_device=1;
 if(type==0x650){int16_t axis;std::memcpy(&axis,b+24,2);if(axis>12000||axis<-12000)last_device=1;}
 if(type==0x401 && enabled && menu_pointer_in_menu() && !host_options_open()){
  last_device=0;const unsigned button=b[24];if(button==1)edges.fetch_or(1);if(button==3)edges.fetch_or(2);
 }
 if(type==0x403 && enabled && menu_pointer_in_menu() && !host_options_open()){
  float y;uint32_t direction;std::memcpy(&y,b+28,4);std::memcpy(&direction,b+32,4);if(direction==1)y=-y;if(y>0)wheel_bits.fetch_or(0x10);if(y<0)wheel_bits.fetch_or(0x40);
 }
}
static bool space_bound_to_back(uint32_t back){
 int count=0;const bool* keys=host_api.keyboard_state(&count);
 constexpr int space=44; // SDL_SCANCODE_SPACE, SDL3 public ABI.
 if(!keys || count<=space || !keys[space])return false;
 if(native_options_enabled())return host_binding_key(kBindRoll)==space || host_binding_key(kBindBack)==space;
 InputConfig local{};const InputConfig* cfg=native_read_original_config(local)?&local:nullptr;
 if(!cfg)return false;
 const int action=back==0x2000u?OUT_CIRCLE:OUT_CROSS;
 for(int i=0;i<cfg->table.binding_count[action] && i<INPUT_MAX_BINDINGS_PER_OUTPUT;i++){
  const auto& b=cfg->table.bindings[action][i];
  if(b.kind==IN_KEY && b.value==space)return true;
 }
 return false;
}
static void update_pad(void* data){
 if(!installed||!data)return;
 int nkeys=0;const bool* keyboard=host_api.keyboard_state(&nkeys);
 native_apply_input(data,keyboard,nkeys,focused(),host_options_open(),menu_pointer_in_menu());
 auto* d=static_cast<uint8_t*>(data);uint32_t buttons;std::memcpy(&buttons,d,4);
 pad_buttons=buttons;pad_x=d[4];pad_y=d[5];
 if(!enabled || !focused() || host_options_open() || !menu_pointer_in_menu()) {edges=0;wheel_bits=0;pulse=0;return;}
 const uint32_t back=menu_back_button();
 buttons=native_menu_space_filter(buttons,back,installed&&enabled,!native_options_enabled()||BbSettings::Get().keyboard_mouse,world_started(),true,true,false,
     host_input_device()==InputDevice::KeyboardMouse,space_bound_to_back(back));
 const auto e=edges.exchange(0);
 uint32_t press=menu_pointer_take_press();
 if(e&1){if(menu_pointer_list_focused())menu_pointer_click();else press|=menu_confirm_button();}
 const auto wheel=wheel_bits.exchange(0);
 if(wheel)menu_pointer_scroll((wheel&0x10)?-1:1);
 if(e&2)press|=menu_back_button();
 if(press){pulse=press;pulse_until=GetTickCount64()+50;}
 if(GetTickCount64()<pulse_until)buttons|=pulse;else pulse=0;
 std::memcpy(d,&buttons,4);
 pad_buttons=buttons;
}
extern "C" int runtime_file_translate(const char* a,char* b,size_t n){return host_api.file_translate(a,b,n);}
extern "C" int runtime_file_mount(const char* a,const char* b){return host_api.file_mount(a,b);}
extern "C" void runtime_file_unmount(const char* a){host_api.file_unmount(a);}

bool native_options_enabled(){return std::getenv("BB_NATIVE_OPTIONS")!=nullptr;}
static void FilterConfig(InputConfig* cfg){
 if(cfg && native_options_enabled())KeepPhysicalControllerBindings(*cfg);
}
static void refresh_original_config(){
 std::lock_guard guard(config_lock);
 char resolved[1024]{};
 const char* path=std::getenv("BB_UI_INPUT_ORIGINAL");
 if(!path)path=std::getenv("BB_INPUT_CONFIG");
 if(!path && host_api.input_path(resolved,sizeof resolved))path=resolved;
 if(path)original_config_ready=host_api.input_load(&original_config,path)!=0;
}
bool native_read_original_config(InputConfig& cfg){
 if(!original_config_ready)refresh_original_config();
 std::lock_guard guard(config_lock);cfg=original_config;return original_config_ready.load();
}
void native_request_reload(){reload_pending=true;}
bool native_mouse_mode(){return raw_mouse_mode;}
uint32_t input_delivered_buttons(){return pad_buttons;}
bool debug_menu_active(){return false;}
static bool MouseMotion(float* x,float* y){
 if(!x||!y)return false;
 if(native_options_enabled()){
  const auto& settings=BbSettings::Get();
  if(!settings.mouse_camera || native_rebinding()){*x=*y=0;return false;}
  if(settings.mouse_invert_x)*x=-*x;if(settings.mouse_invert_y)*y=-*y;
 }
 return true;
}

bool native_reserved_key(int sc){InputConfig cfg{};native_read_original_config(cfg);const bool supported=(sc>=4&&sc<=99&&sc!=83)||sc==103||sc==133||(sc>=224&&sc<=231);return !supported || sc==73 || sc==66 || sc==cfg.toggle_scancode || sc==cfg.reload_scancode;}

void native_request_mouse_enable(){enable_mouse_pending=true;}

static void RenderSave(void(*save)()){
 if(!save)return;
 try {NativePages::PreserveRenderSave(save);}
 catch(const std::exception& e){host_log("settings preservation: %s",e.what());}
}
extern "C" __declspec(dllexport) const BbPcUiApi* BbPcGetApi(const BbPcHostApi* api){
 if(!api || api->version!=BB_PC_UI_API_VERSION || api->size!=sizeof(BbPcHostApi) ||
    !api->overlay_open || !api->keyboard_state || !api->input_load || !api->input_path ||
    !api->file_translate || !api->file_mount || !api->file_unmount)return nullptr;
 host_api=*api;
 static const BbPcUiApi result={sizeof(BbPcUiApi),BB_PC_UI_API_VERSION,
    InstallImage,NoteEvent,WindowMode,TakeReload,CaptureAllowed,CaptureNote,
    update_pad,FilterConfig,MouseMotion,RenderSave};
 return &result;
}

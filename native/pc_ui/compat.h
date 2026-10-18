// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#ifndef BBUI_TEST
#include <windows.h>
#endif
#include <cstdint>
#include <cstddef>
#include <string>
#define GUEST_ABI __attribute__((sysv_abi))
constexpr uint64_t kPreferredGuestSlide=0x400000;
struct GuestMemory {uint64_t slide;size_t size;};
struct ElfImage {GuestMemory mem;};
inline void* guest_ptr(const GuestMemory& m,uint64_t va){return va>=m.slide && va-m.slide<m.size?reinterpret_cast<void*>(va):nullptr;}
inline bool guest_protect_rw(GuestMemory* m,uint64_t va,size_t n){return va>=m->slide && va-m->slide<=m->size && n<=m->size-(va-m->slide);}
inline bool guest_protect_rwx(GuestMemory* m,uint64_t va,size_t n){return guest_protect_rw(m,va,n);}
inline bool guest_protect_rx(GuestMemory*,uint64_t,size_t){return true;}
#ifdef BBUI_TEST
bool test_safe_read(const void*,void*,size_t);
inline bool host_read_safe(const void* p,void* out,size_t n){return test_safe_read(p,out,n);}
#else
inline bool host_read_safe(const void* p,void* out,size_t n){SIZE_T done=0;return ReadProcessMemory(GetCurrentProcess(),p,out,n,&done) && done==n;}
#endif
inline void* hle_wrap_fn(void* p){return p;}
#include <type_traits>
template<typename T> inline int64_t arg(T v){if constexpr(std::is_pointer_v<T>)return reinterpret_cast<int64_t>(v);else return static_cast<int64_t>(v);}
#ifdef BBUI_TEST
int64_t test_guest_call(void*,const int64_t*,size_t);
template<typename T=int64_t,typename... A> T hle_call_guest(void* p,A... a){const int64_t args[]={arg(a)...};return static_cast<T>(test_guest_call(p,args,sizeof...(A)));}
#else
template<typename T=int64_t,typename... A> T hle_call_guest(void* p,A... a){using Fn=int64_t(GUEST_ABI *)(decltype(arg(a))...);return static_cast<T>(reinterpret_cast<Fn>(p)(arg(a)...));}
#endif
constexpr uint8_t kStickRest=128,kStickLow=1,kStickHigh=255;
constexpr int kPadTouchW=1920,kPadTouchH=943;
struct PadState{uint32_t buttons=0;uint8_t lx=128,ly=128,rx=128,ry=128,l2=0,r2=0;bool connected=true;
 struct Touch{uint16_t x=0,y=0;uint8_t id=0;bool down=false;} touch[2];uint8_t touch_count=0;};
enum class InputDevice{KeyboardMouse,Pad};
struct HostSettings{bool mouse_menu;};
HostSettings host_settings();InputDevice host_input_device();PadState host_pad_state();
bool host_mouse_stage_position(float&,float&);bool host_mouse_relative();void host_mouse_describe(char*,size_t);
bool host_options_open();bool ingame_menu_open();bool host_text_entry_open();bool debug_menu_open();
bool option_menu_click_decides(uint8_t*,int);void option_menu_list_update(uint8_t*,bool);
bool native_keyboard_movement();
void host_log(const char*,...);
void menu_pointer_install(ElfImage*);void menu_pointer_click();uint32_t menu_pointer_take_press();
bool menu_pointer_in_menu();uint32_t menu_confirm_button();uint32_t menu_back_button();

bool menu_pointer_ready();

bool menu_pointer_list_focused();
void menu_pointer_scroll(int);

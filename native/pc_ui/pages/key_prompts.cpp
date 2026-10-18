// SPDX-License-Identifier: GPL-2.0-or-later
// Native text-path hooks verified against the user's plaintext Bloodborne 1.09 SELF.
#include "../compat.h"
#include "../engine_hooks.h"
#include "key_prompts.h"
#include "prompt_text.h"
#include "bbport_settings.h"
#include "chords.h"
#include <array>
#include <atomic>
#include <cstring>
namespace BbEngine::KeyPrompts {
namespace {
std::uintptr_t slide=0;
std::atomic<unsigned> shown{0};
constexpr std::size_t FormatOffset=0x1ae7270,CopyOffset=0x1321f10,ImageFormat=0x49985be;
constexpr std::uint8_t FormatBytes[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x54,0x53,0x48,0x81,0xec,0x20,0x01,0,0};
constexpr std::uint8_t CopyBytes[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x83,0xec,0x18};
constexpr std::size_t ActionOffset=0x1b57460,IconOffset=0x1acbd90;
constexpr std::uint8_t ActionBytes[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x81,0xec,0x68,0x03,0,0};
constexpr std::uint8_t IconBytes[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x81,0xec,0xc8,0x03,0,0};
bool active(){return BbSettings::Get().keyboard_mouse && BbSettings::Get().key_prompts;}
bool read_wide(std::uint64_t pointer,std::u16string& out,std::size_t limit=4096) {
 if(!pointer)return false;out.clear();
 for(std::size_t pos=0;pos<limit;) {
  char16_t block[64];auto count=std::min(std::size(block),limit-pos);
  if(!::host_read_safe(reinterpret_cast<void*>(pointer+pos*2),block,count*2)){
   count=1;if(!::host_read_safe(reinterpret_cast<void*>(pointer+pos*2),block,2))return false;
  }
  for(std::size_t i=0;i<count;++i){if(!block[i])return true;out.push_back(block[i]);}
  pos+=count;
 }
 return false;
}
const char16_t* stable(std::u16string value) {
 // Retain multiple nested formatter arguments on each thread until copied.
 thread_local std::array<std::u16string,32> ring;thread_local unsigned next=0;
 auto& slot=ring[next++%ring.size()];slot=std::move(value);return slot.c_str();
}
std::string binding(int action){char out[256]{};host_binding_prompt(action,out,sizeof(out));return out;}
PcPrompts::Reader reader(const PcPrompts::Context& ctx) {
 const bool menu=ctx.menu && ctx.role!=PcPrompts::Role::Interact;
 return [menu](int action) {
  if(menu) {
   const auto chord=Chords::Get(action);
   // Menu clicks are Confirm/Back, not the gameplay mouse-attack bindings.
   if((chord.active()&&chord.mouse)||(!chord.active()&&!host_binding_key(action)))return std::string{};
  }
  return binding(action);
 };
}
PcPrompts::Context context(std::uint64_t caller) {
 PcPrompts::Context ctx{menu_pointer_in_menu(),bool(BbSettings::Get().mouse_menu),bool(BbSettings::Get().mouse_camera)};
 const auto offset=caller>=slide?caller-slide:0;
 // The action-caption builder loads message group 107 / fallback 2, prefixes
 // it with the region's decide icon, then substitutes OKIcon and CancelIcon.
 // This is gameplay Interact even when a non-pausing menu is open.
 if(offset==0x1ae7c9a||offset==0x1ae7fe3)ctx.role=PcPrompts::Role::Interact;
 else if(offset==0x1ae8023)ctx.role=PcPrompts::Role::Roll;
 else if(offset==0x1ae633c)ctx.role=PcPrompts::Role::Confirm;
 else if(offset==0x1ae655c||offset==0x1c27ea1)ctx.role=PcPrompts::Role::Back;
 return ctx;
}
void describe(const char* kind,const std::string& label){if(shown.fetch_add(1)<6)::host_log("Key prompts: %s -> %s",kind,label.c_str());}
// ActionButton1 has a separate UTF-16 caption and an animated button sprite.
// The input record owns a guest wstring at +0x08 (SSO buffer +0x10,
// length +0x20, capacity +0x28, allocator +0x30). Never edit or free it.
std::int64_t GUEST_ABI ActionHook(std::uint64_t,const std::uint64_t* saved) noexcept {
 if(!active())return 0;
 try {
  struct Record {alignas(16) std::array<unsigned char,0x40> bytes;std::u16string caption;};
  thread_local std::array<Record,8> records;thread_local unsigned next=0;
  auto& r=records[next++%records.size()];
  if(!host_read_safe(reinterpret_cast<void*>(saved[4]),r.bytes.data(),r.bytes.size()))return 0;
  std::uint64_t length,capacity,pointer;
  std::memcpy(&length,r.bytes.data()+0x20,8);std::memcpy(&capacity,r.bytes.data()+0x28,8);
  if(!length||length>4096)return 0;
  if(capacity<8)pointer=saved[4]+0x10;else std::memcpy(&pointer,r.bytes.data()+0x10,8);
  std::u16string original;if(!read_wide(pointer,original)||original.size()!=length)return 0;
  // The game supplies localization; replace only the control, not the words.
  r.caption=PcPrompts::text(binding(kBindInteract))+u" "+original;
  pointer=reinterpret_cast<std::uint64_t>(r.caption.c_str());length=r.caption.size();capacity=std::max<std::uint64_t>(8,length);
  std::memcpy(r.bytes.data()+0x10,&pointer,8);std::memcpy(r.bytes.data()+0x20,&length,8);std::memcpy(r.bytes.data()+0x28,&capacity,8);
  const_cast<std::uint64_t*>(saved)[4]=reinterpret_cast<std::uint64_t>(r.bytes.data());
 }catch(...){ }
 return 0;
}
std::int64_t GUEST_ABI IconHook(std::uint64_t,const std::uint64_t* saved) noexcept {
 // Only the ActionButton1 sprite. Frame zero takes the game's own hidden
 // path; the text and every other icon retain their normal rendering.
 if(active()&&saved[6]==slide+0x1b57948) {
  auto* stack=const_cast<std::uint64_t*>(saved)+7;
  *stack &= 0xffffffff00000000ULL;
 }
 return 0;
}
std::int64_t GUEST_ABI FormatHook(std::uint64_t,const std::uint64_t* saved) noexcept {
 if(!active())return 0;
 try {
  auto* args=const_cast<std::uint64_t*>(saved);
  if(saved[4]==slide+ImageFormat) {
   std::u16string image;if(!read_wide(saved[3],image,64))return 0;
   const auto ctx=context(saved[6]);const auto value=PcPrompts::label(image,ctx,reader(ctx));if(value.empty())return 0;
   auto escaped=PcPrompts::text(value);escaped.erase(0,1);escaped.pop_back();
   args[4]=reinterpret_cast<std::uint64_t>(u"[%s]");args[3]=reinterpret_cast<std::uint64_t>(stable(std::move(escaped)));
   describe("image tag",value);return 0;
  }
  // The same formatter also sees some literal help markup as its format text.
  char16_t first=0;if(!::host_read_safe(reinterpret_cast<void*>(saved[4]),&first,2)||first!=u'<')return 0;
  std::u16string format;if(!read_wide(saved[4],format))return 0;
  auto ctx=context(saved[6]);ctx.menu=false;
  auto rewritten=PcPrompts::rewrite(format,ctx,reader(ctx),true);
  if(rewritten!=format)args[4]=reinterpret_cast<std::uint64_t>(stable(std::move(rewritten)));
 }catch(...){/* Preserve the original text if a host allocation fails. */}
 return 0;
}
std::int64_t GUEST_ABI CopyHook(std::uint64_t,const std::uint64_t* saved) noexcept {
 if(!active())return 0;
 try {
  std::u16string source;if(!read_wide(saved[4],source)||source.find(u"<img")==source.npos)return 0;
  auto ctx=context(saved[6]);ctx.menu=false;
  auto rewritten=PcPrompts::rewrite(source,ctx,reader(ctx));
  if(rewritten!=source) {
   const_cast<std::uint64_t*>(saved)[4]=reinterpret_cast<std::uint64_t>(stable(std::move(rewritten)));
   describe("literal message","keyboard labels");
  }
 }catch(...){ }
 return 0;
}
}
bool Install(unsigned char* image,std::size_t size) {
 Hook hooks[4];slide=reinterpret_cast<std::uintptr_t>(image);
 if(!Prepare(hooks[0],image,size,FormatOffset,FormatBytes,FormatHook,true)||
    !Prepare(hooks[1],image,size,CopyOffset,CopyBytes,CopyHook)||
    !Prepare(hooks[2],image,size,ActionOffset,ActionBytes,ActionHook)||
    !Prepare(hooks[3],image,size,IconOffset,IconBytes,IconHook)) {
  for(auto& hook:hooks)Discard(hook);
  ::host_log("Key prompts: text hook signature mismatch; original control images retained");return false;
 }
 Commit(hooks);::host_log("Key prompts: native UTF-16 formatter, message-copy and gameplay action hooks installed; original textures retained");return true;
}
}

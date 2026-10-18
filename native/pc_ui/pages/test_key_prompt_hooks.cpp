// SPDX-License-Identifier: GPL-2.0-or-later
#include "../compat.h"
#include "bbport_settings.h"
#include "key_prompts.h"
#include "bindings.h"
#include <sys/mman.h>
#include <cassert>
#include <cstring>
#include <map>
#include <cstdio>
#include <cstdarg>
static bool in_menu=false;
static std::map<int,std::string> bindings={{kBindInteract,"E"},{kBindConfirm,"Return"},{kBindBack,"Backspace"},{kBindAttack,"L Mouse"},{kBindRoll,"Space"}};
bool test_safe_read(const void* p,void* out,size_t n){if(!p)return false;std::memcpy(out,p,n);return true;}
bool menu_pointer_in_menu(){return in_menu;}
void host_log(const char*,...){}
int host_binding_key(int a){return a==kBindAttack?0:1;}
void host_binding_prompt(int a,char* out,size_t n){std::snprintf(out,n,"%s",bindings[a].c_str());}
namespace BbSettings {Values& Get(){static Values v;return v;}}
int main(){
 constexpr size_t size=0x4a00000,format=0x1ae7270,copy=0x1321f10,img=0x49985be,actionoffset=0x1b57460,iconoffset=0x1acbd90;
 auto* mem=static_cast<unsigned char*>(mmap(nullptr,size,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));assert(mem!=MAP_FAILED);
 const unsigned char fp[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x54,0x53,0x48,0x81,0xec,0x20,0x01,0,0};
 const unsigned char cp[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x83,0xec,0x18};
 const unsigned char ap[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x81,0xec,0x68,0x03,0,0};
 const unsigned char ip[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x81,0xec,0xc8,0x03,0,0};
 std::memcpy(mem+actionoffset,ap,sizeof ap);std::memcpy(mem+iconoffset,ip,sizeof ip);
 std::memcpy(mem+format,fp,sizeof fp);std::memcpy(mem+copy,cp,sizeof cp);mem[copy]=0xcc;
 assert(!BbEngine::KeyPrompts::Install(mem,size));assert(!std::memcmp(mem+format,fp,sizeof fp));mem[copy]=cp[0];
 const unsigned char ft[]={0x48,0x89,0x37,0x48,0x89,0x57,0x08,0x48,0x89,0xf8,0x48,0x81,0xc4,0x20,0x01,0,0,0x5b,0x41,0x5c,0x41,0x5e,0x41,0x5f,0x5d,0xc3};
 const unsigned char ct[]={0x48,0x89,0x37,0x48,0x89,0xf8,0x48,0x83,0xc4,0x18,0x5b,0x41,0x5c,0x41,0x5d,0x41,0x5e,0x41,0x5f,0x5d,0xc3};
 std::memcpy(mem+format+sizeof fp,ft,sizeof ft);std::memcpy(mem+copy+sizeof cp,ct,sizeof ct);
 const char16_t image_format[]=u"<img src='%s' vspace='-8'>";std::memcpy(mem+img,image_format,sizeof image_format);
 const unsigned char at[]={0x48,0x89,0x37,0x48,0x89,0xf8,0x48,0x81,0xc4,0x68,0x03,0,0,0x5b,0x41,0x5c,0x41,0x5d,0x41,0x5e,0x41,0x5f,0x5d,0xc3};
 const unsigned char it[]={0x48,0x8b,0x45,0x10,0x48,0x89,0x07,0x48,0x81,0xc4,0xc8,0x03,0,0,0x5b,0x41,0x5c,0x41,0x5d,0x41,0x5e,0x41,0x5f,0x5d,0xc3};
 std::memcpy(mem+actionoffset+sizeof ap,at,sizeof at);std::memcpy(mem+iconoffset+sizeof ip,it,sizeof it);
 assert(BbEngine::KeyPrompts::Install(mem,size));
 using F=void*(*)(const char16_t**,const char16_t*,const char16_t*);using C=void*(*)(const char16_t**,const char16_t*);
 auto f=reinterpret_cast<F>(mem+format);auto c=reinterpret_cast<C>(mem+copy);const char16_t* out[2]{};
 f(out,reinterpret_cast<const char16_t*>(mem+img),u"KG_Cancel");assert(std::u16string(out[0])==u"[%s]");assert(std::u16string(out[1])==u"E");
 bindings[kBindInteract]="T";f(out,reinterpret_cast<const char16_t*>(mem+img),u"KG_Cancel");assert(std::u16string(out[1])==u"T");
 in_menu=true;f(out,reinterpret_cast<const char16_t*>(mem+img),u"KG_Cancel");assert(std::u16string(out[1])==u"LMB");
 // Reproduce the supplied eboot's action-builder CALL return address exactly.
 auto* action=mem+0x1ae7c91;unsigned char helper[]={0x48,0x83,0xec,0x08,0xe8,0,0,0,0,0x48,0x83,0xc4,0x08,0xc3};
 int32_t rel=int32_t(format-0x1ae7c9a);std::memcpy(helper+5,&rel,4);std::memcpy(action,helper,sizeof helper);
 reinterpret_cast<F>(action)(out,reinterpret_cast<const char16_t*>(mem+img),u"KG_Cancel");assert(std::u16string(out[1])==u"T");
 c(out,u"Attack: <img src='KG_R1'>; <?itemName?>");assert(std::u16string(out[0])==u"Attack: [LMB]; <?itemName?>");
 c(out,u"Dodge: <img src='KG_OK'>");assert(std::u16string(out[0])==u"Dodge: [Space]");
 // Exercise the separate ActionButton1 record: heap and inline strings.
 using A=void*(*)(const void**,const void*);auto a=reinterpret_cast<A>(mem+actionoffset);
 alignas(16) unsigned char record[64]={};record[0]=record[1]=1;
 const char16_t* caption=u"Abrir puerta";std::uint64_t pointer=reinterpret_cast<std::uint64_t>(caption),length=12,capacity=12;
 std::memcpy(record+16,&pointer,8);std::memcpy(record+32,&length,8);std::memcpy(record+40,&capacity,8);
 unsigned char original_record[64];std::memcpy(original_record,record,64);const void* result=nullptr;
 a(&result,record);std::memcpy(&pointer,static_cast<const unsigned char*>(result)+16,8);
 assert(std::u16string(reinterpret_cast<const char16_t*>(pointer))==u"[T] Abrir puerta");assert(!std::memcmp(record,original_record,64));
 bindings[kBindInteract]="E";a(&result,record);std::memcpy(&pointer,static_cast<const unsigned char*>(result)+16,8);assert(std::u16string(reinterpret_cast<const char16_t*>(pointer))==u"[E] Abrir puerta");
 std::memset(record+16,0,16);std::memcpy(record+16,u"Jalar",10);length=5;capacity=7;std::memcpy(record+32,&length,8);std::memcpy(record+40,&capacity,8);
 a(&result,record);std::memcpy(&pointer,static_cast<const unsigned char*>(result)+16,8);assert(std::u16string(reinterpret_cast<const char16_t*>(pointer))==u"[E] Jalar");
 // Exact gameplay caller, with its by-value stack record. Frame zero hides it.
 auto* shim=mem+0x1b57900;std::memset(shim,0x90,0x43);
 const unsigned char prefix[]={0x48,0x83,0xec,0x48,0x48,0xc7,0x04,0x24,4,0,0,0};std::memcpy(shim,prefix,sizeof prefix);
 mem[0x1b57943]=0xe8;int32_t ir=int32_t(iconoffset-0x1b57948);std::memcpy(mem+0x1b57944,&ir,4);
 const unsigned char suffix[]={0x48,0x83,0xc4,0x48,0xc3};std::memcpy(mem+0x1b57948,suffix,sizeof suffix);
 std::uint64_t frame=99;reinterpret_cast<void(*)(std::uint64_t*)>(shim)(&frame);assert(frame==0);
 BbSettings::Get().key_prompts=false;reinterpret_cast<void(*)(std::uint64_t*)>(shim)(&frame);assert(frame==4);a(&result,record);assert(result==record);BbSettings::Get().key_prompts=true;
 BbSettings::Get().keyboard_mouse=false;
 a(&result,record);assert(result==record);
 f(out,reinterpret_cast<const char16_t*>(mem+img),u"KG_Cancel");assert(out[0]==reinterpret_cast<const char16_t*>(mem+img));assert(std::u16string(out[1])==u"KG_Cancel");
 const char16_t* original=u"<img src='KG_R1'>";c(out,original);assert(out[0]==original);
 BbSettings::Get().keyboard_mouse=true;BbSettings::Get().key_prompts=false;c(out,original);assert(out[0]==original);
 munmap(mem,size);puts("PASS: installed hooks execute original eboot prologues, rewrite real argument slots, distinguish world/menu calls, reflect rebinds, and preserve controller/off paths.");
}

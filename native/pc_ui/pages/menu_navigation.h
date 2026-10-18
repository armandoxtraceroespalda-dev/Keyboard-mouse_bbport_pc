// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <cstring>
namespace NativeNavigation {
// Verified in the vanilla Defaults dialog callback (1b24924..1b24965).
// Transfers one owning DLReferenceCountObject reference to the parent's stack.
template<class Read> inline bool ContainsChild(const std::uint8_t* parent,std::uint64_t child,Read read){
 if(!child)return false;std::uint64_t count=0;if(!read(parent+0x40,&count,8)||count>4)return false;
 auto padding=(-reinterpret_cast<std::uintptr_t>(parent+0x18))&7;
 for(unsigned i=0;i<count;++i){std::uint64_t value=0;if(!read(parent+0x18+padding+i*8,&value,8))return false;if(value==child)return true;}
 return false;
}
inline bool PushOwnedChild(std::uint8_t* parent,std::uint64_t child){
 std::uint64_t count=0;std::memcpy(&count,parent+0x40,8);
 if(!child||count>=4)return false;
 auto padding=(-reinterpret_cast<std::uintptr_t>(parent+0x18))&7;
 std::memcpy(parent+0x18+padding+8*count,&child,8);++count;
 std::memcpy(parent+0x40,&count,8);return true;
}
}

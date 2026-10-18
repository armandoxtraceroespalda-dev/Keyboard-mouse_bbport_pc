// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <windows.h>
#include <cstddef>
namespace BbPlatform {
inline bool ReadProcessMemory(const void* src,void* out,std::size_t n){SIZE_T done=0;return ::ReadProcessMemory(GetCurrentProcess(),src,out,n,&done) && done==n;}
}

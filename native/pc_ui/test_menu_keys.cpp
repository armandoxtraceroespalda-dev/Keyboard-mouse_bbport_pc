// SPDX-License-Identifier: GPL-3.0-or-later
#include "menu_keys_policy.h"
#include <cassert>
#include <cstdio>
int main(){
 constexpr uint32_t back=0x2000,other=0x1000;
 // The only filtering combination: master option + KBM + in-world menu,
 // focused, not overlay, keyboard device, Space really bound to Back and held.
 for(unsigned flags=0;flags<256;flags++){
  bool a[8];for(int i=0;i<8;i++)a[i]=(flags>>i)&1;
  const auto value=native_menu_space_filter(back|other,back,a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7]);
  const bool filter=a[0]&&a[1]&&a[2]&&a[3]&&a[4]&&!a[5]&&a[6]&&a[7];
  assert(value==(filter?other:(other|back)));
  assert((value|back)==(other|back)); // RMB Back, added after filtering, still works.
 }
 puts("PASS: all 256 Space/menu/option/KBM/focus/title/controller gates; right-click Back retained.");
}

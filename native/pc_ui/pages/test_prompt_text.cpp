// SPDX-License-Identifier: GPL-2.0-or-later
#include "prompt_text.h"
#include <cassert>
#include <map>
#include <cstdio>
int main(){
 std::map<int,std::string> keys={{kBindInteract,"E"},{kBindRoll,"Space"},{kBindConfirm,"Return"},{kBindBack,"Backspace"},{kBindAttack,"L Mouse"},{kBindStrongMod,"Left Shift"},{kBindFirearm,"R Mouse"},{kBindBloodVial,"C"},{kBindUseItem,"R"}};
 PcPrompts::Reader read=[&](int a){return keys[a];};
 PcPrompts::Context world,menus{true,true,true};
 assert(PcPrompts::rewrite(u"<img src='KG_Cancel' vspace='-8'> Open door",world,read)==u"[E] Open door");
 assert(PcPrompts::rewrite(u"<img src=\"KG_OK\"> + dodge",world,read)==u"[Space] + dodge");
 assert(PcPrompts::rewrite(u"<img src='KG_R2'>",world,read)==u"[Shift + LMB]");
 assert(PcPrompts::rewrite(u"<img src='KG_Cancel'> Select; <img src='KG_OK'> Back",menus,read)==u"[LMB] Select; [RMB] Back");
 menus.mouse_menu=false;
 assert(PcPrompts::rewrite(u"<img src='KG_Cancel'> <img src='KG_OK'>",menus,read)==u"[Enter] [Backspace]");
 // The world action prompt still uses Interact while an inventory is open.
 menus.role=PcPrompts::Role::Interact;
 assert(PcPrompts::label(u"KG_OK",menus,read)=="E");
 keys[kBindInteract]="T";assert(PcPrompts::label(u"KG_Cancel",world,read)=="T");
 keys[kBindStrong]="Left Ctrl + Mouse2";assert(PcPrompts::label(u"KG_R2",world,read)=="Ctrl + RMB");
 std::u16string other=u"<?itemName?> <img src='ItemIcon'> <font color='#fff'>Équipé</font>";
 assert(PcPrompts::rewrite(other,world,read)==other);
 assert(PcPrompts::rewrite(u"<img src='KG_R_L'> / <img src='KG_R_U'>",world,read)==u"[R] / [C]");
 keys[kBindInteract]="< & >";assert(PcPrompts::rewrite(u"<img src='KG_Cancel'>",world,read)==u"[&lt; &amp; &gt;]");
 keys[kBindInteract]="";assert(PcPrompts::rewrite(u"<img src='KG_Cancel'>",world,read)==u"[-]");
 keys[kBindInteract]="Keypad %";assert(PcPrompts::rewrite(u"<img src='KG_Cancel'> %d",world,read,true)==u"[Keypad %%] %d");
 puts("PASS: world/menu semantics, live rebinding/chords, native strong modifier, localized text and non-control images preserved.");
}

// SPDX-License-Identifier: GPL-3.0-or-later
#include "language_text.h"
#include "prompt_text.h"
#include <cassert>
int main(){
 const char* english="Strong Attack";
 assert(PcLanguage::Translate(english,false)==english);
 assert(std::string_view(PcLanguage::Translate(english,true))=="Ataque fuerte");
 assert(PcPrompts::wide(PcLanguage::Translate("Menu",true))==u"Menú");
 assert(std::string_view(PcLanguage::Translate("Controller",true))=="Mando");
 assert(std::string_view(PcLanguage::Translate("Default",true))=="Predeterminado");
 assert(std::string_view(PcLanguage::Translate("Open or close the game menu.",true))=="Abre o cierra el menú del juego.");
}

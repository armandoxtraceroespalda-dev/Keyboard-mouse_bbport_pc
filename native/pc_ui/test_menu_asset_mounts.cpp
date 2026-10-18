// SPDX-License-Identifier: GPL-3.0-or-later
#include "menu_asset_mounts.h"
#include <cassert>
#include <fstream>
#include <iostream>
int main(int argc,char** argv){
 assert(argc==2);namespace fs=std::filesystem;
 fs::path root=argv[1];fs::create_directories(root);
 std::vector<std::pair<std::string,std::string>> mounts={{"/app0/dvdroot_ps4/menu/optionsetting.gfx","movie"}};
 for(int i=0;i<22;++i){
  auto relative=fs::path("lang"+std::to_string(i))/"menu.msgbnd.dcx";
  auto prepared=root/"prepared"/relative,merged=root/"messages-view"/relative;
  fs::create_directories(prepared.parent_path());fs::create_directories(merged.parent_path());
  std::ofstream(prepared)<<"text";std::ofstream(merged)<<"text";
  mounts.emplace_back("/app0/dvdroot_ps4/msg/"+relative.generic_string(),prepared.string());
 }
 auto before=mounts;
 assert(6+mounts.size()>16); // Six existing mounts, including mirror.
 assert(BbEngine::ConsolidateMessageMounts(mounts,root));
 assert(mounts.size()==2 && 6+mounts.size()+1<16); // also reserves Rebirth/save mounts
 assert(mounts[0]==before[0] && mounts[1].first=="/app0/dvdroot_ps4/msg");
 fs::remove(root/"messages-view/lang21/menu.msgbnd.dcx");
 assert(!BbEngine::ConsolidateMessageMounts(before,root));
 assert(before.size()==23); // Invalid plan must not partially replace mounts.
 std::cout<<"PASS: 22 languages require only two mounts; missing language rejected.\n";
}

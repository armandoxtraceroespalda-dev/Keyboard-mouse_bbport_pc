// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <filesystem>
#include <string>
#include <utility>
#include <vector>
namespace BbEngine {
// Validate ownership per language before calling this. Keep every other message
// bundle in the private merged directory; original runtime has 16 mount slots.
inline bool ConsolidateMessageMounts(std::vector<std::pair<std::string,std::string>>& mounts,
                                    const std::filesystem::path& root) {
    constexpr const char* prefix="/app0/dvdroot_ps4/msg/";
    const auto view=root/"messages-view";
    if(mounts.size()<2 || !std::filesystem::is_directory(view))return false;
    for(std::size_t i=1;i<mounts.size();++i){
        const auto& guest=mounts[i].first;
        if(guest.rfind(prefix,0)!=0)return false;
        const auto relative=guest.substr(std::char_traits<char>::length(prefix));
        const auto merged=view/relative;
        if(!std::filesystem::is_regular_file(merged) ||
           std::filesystem::file_size(merged)!=std::filesystem::file_size(std::filesystem::u8path(mounts[i].second)))return false;
    }
    mounts.resize(1);
    mounts.emplace_back("/app0/dvdroot_ps4/msg",view.string());
    return true;
}
}

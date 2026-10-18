// SPDX-License-Identifier: GPL-3.0-or-later
// Uses a locally supplied original movie. No game resources are included.
#define main asset_tool_main
#include "menu_assets.cpp"
#undef main
#include <cassert>
std::map<std::string,std::uint16_t> main_names(const Bytes& b){std::map<std::string,std::uint16_t> names;for(auto& t:tags(b,body_start(b),b.size()))if(t.code==26||t.code==70){auto p=placement(b,t.code,t.s);if(p.name&&p.cid)names[*p.name]=*p.cid;}return names;}
std::map<std::string,Placement> section_rows(const Bytes& b,std::uint16_t cid){std::map<std::string,Placement> rows;auto s=find_sprite(b,cid);assert(s);for(auto& t:tags(b,s->s+4,s->e))if(t.code==26||t.code==70){auto p=placement(b,t.code,t.s);if(p.name&&starts_with(*p.name,"Item_")){assert(!rows.count(*p.name));rows[*p.name]=p;}}return rows;}
void verify_frames(const Bytes& b,std::uint16_t cid,int normal,int button,int defaults){
 auto sp=find_sprite(b,cid);std::map<int,Placement> state;std::map<std::string,int> names;int frames=0;
 for(const auto& t:tags(b,sp->s+4,sp->e)){
  if(t.code==26){auto p=placement(b,t.code,t.s);auto& existing=state[p.depth];
   if(p.name)names[*p.name]=p.depth;
   if(p.has_mat){existing.tx=p.tx;existing.ty=p.ty;}
   if(p.cid)existing.cid=p.cid;
  }
  if(t.code==1){++frames;
   auto get=[&](int i)->Placement{return state.at(names.at("Item_"+std::to_string(i)+"_0"));};
   auto right=get(defaults);if(button>=0){auto left=get(button);assert(left.ty==right.ty);assert(left.tx+caption_x(b,*left.cid)==get(0).tx+caption_x(b,*get(0).cid)+600);assert(left.cid==right.cid);}else assert(right.cid==100);
   if(normal==7)for(int i=1;i<normal;++i)assert(get(i).ty-get(i-1).ty==1100);
  }
 }
 assert(frames==19);
}
std::vector<std::int64_t> footer_y(const Bytes& b,std::uint16_t cid,int index){
 auto sp=find_sprite(b,cid);std::map<int,std::int64_t> ys;int depth=-1;std::vector<std::int64_t> out;
 for(const auto& t:tags(b,sp->s+4,sp->e)){
  if(t.code==26){auto p=parse_place2(b,t.s,t.e);if(p.name&&*p.name=="Item_"+std::to_string(index)+"_0")depth=p.depth;if(p.mat)ys[p.depth]=p.mat->ty;}
  if(t.code==1){assert(depth>=0);out.push_back(ys.at(depth));}
 }return out;
}
int main(int argc,char** argv){try{assert(argc==2);std::ifstream f(argv[1],std::ios::binary);Bytes original((std::istreambuf_iterator<char>(f)),{});auto changed=build_option_movie(original);auto names=main_names(changed),old=main_names(original);
 assert(!names.count("PCControls")&&!names.count("PCEnhance"));assert(names.count("PCKeys")&&names.count("PCCamera"));
 for(auto name:{"ControllSetting","EnvironmentSetting","Top"})assert(names.at(name)==old.at(name));
 auto controls=section_rows(changed,names.at("ControllSetting"));assert(controls.size()==9);
 auto classes=symbol_classes(changed);
 assert(classes.at(names.at("PCKeys"))=="OptionSetting_fla._Option_ItemList_71"&&classes.at(names.at("PCCamera"))=="OptionSetting_fla._Option_ItemList_71");
 for(const auto& [id,name]:symbol_classes(original))assert(classes.at(id)==name);
 auto oldc=section_rows(original,old.at("ControllSetting"));
 auto before=footer_y(original,old.at("ControllSetting"),6),after=footer_y(changed,names.at("ControllSetting"),8);
 assert(before.size()==19&&after.size()==19);for(int i=0;i<19;++i)assert(after[i]-before[i]==400);assert(controls.at("Item_8_0").cid==oldc.at("Item_6_0").cid);assert(controls.at("Item_8_0").ty==oldc.at("Item_6_0").ty+400);
 for(int i=0;i<7;++i){auto row=controls.at("Item_"+std::to_string(i)+"_0");assert(row.cid==oldc.at("Item_0_0").cid);assert(row.ty<controls.at("Item_8_0").ty);}
 auto env=section_rows(changed,names.at("EnvironmentSetting"));assert(env.size()==7);assert(env.at("Item_5_0").cid==env.at("Item_6_0").cid);
 assert(controls.at("Item_7_0").cid==controls.at("Item_8_0").cid);
 assert(section_rows(original,old.at("Top")).size()==section_rows(changed,names.at("Top")).size());
 verify_frames(changed,names.at("ControllSetting"),7,7,8);
 verify_frames(changed,names.at("EnvironmentSetting"),5,5,6);
 verify_frames(changed,names.at("PCKeys"),7,-1,7);
 auto keys=section_rows(changed,names.at("PCKeys"));assert(keys.size()==8);
 assert(keys.at("Item_7_0").cid==oldc.at("Item_6_0").cid);
 assert(keys.at("Item_7_0").tx==oldc.at("Item_6_0").tx&&keys.at("Item_7_0").ty==oldc.at("Item_6_0").ty+400);
 auto keyY=footer_y(changed,names.at("PCKeys"),7);assert(keyY.size()==before.size());for(int i=0;i<19;++i)assert(keyY[i]-before[i]==400);
 for(int i=1;i<7;++i)assert(keys.at("Item_"+std::to_string(i)+"_0").ty-keys.at("Item_"+std::to_string(i-1)+"_0").ty==1100);
 // Original non-sprite definitions (fonts, shapes, images) are byte-identical.
 for(auto& t:tags(original,body_start(original),original.size()))if(t.code!=39&&t.code!=76&&t.code!=26&&t.code!=70){Bytes data(original.begin()+t.head,original.begin()+t.e);assert(std::search(changed.begin(),changed.end(),data.begin(),data.end())!=changed.end());}
 if(const char* out=std::getenv("BB_TEST_MOVIE_OUT")){std::ofstream stream(out,std::ios::binary);stream.write(reinterpret_cast<const char*>(changed.data()),changed.size());}
 puts("PASS: actual original movie extended in place; native footer buttons, all 19 animation frames aligned, original font/art retained; PCControls and PCEnhance absent.");}catch(const Fail& e){std::fprintf(stderr,"FAIL: %s\n",e.why.c_str());return 1;}}

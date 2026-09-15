#include <windows.h>
#include "renderdoc_replay.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string>
REPLAY_PROGRAM_MARKER();
static unsigned long long ID(ResourceId r) { unsigned long long x=0; memcpy(&x,&r,sizeof(x)); return x; }
static void Walk(const rdcarray<ActionDescription> &nodes, std::ofstream &f, unsigned &last) {
 for(const auto &a:nodes) {
   last=std::max(last,a.eventId);
   f<<a.eventId<<'\t'<<static_cast<unsigned>(a.flags)<<'\t'<<a.customName.c_str()<<'\t'<<ID(a.copySource)<<'\t'<<ID(a.copyDestination)<<'\n';
   Walk(a.children,f,last);
 }
}
int main(int argc,char **argv) {
 if(argc<3) { fprintf(stderr,"usage: replay capture.rdc outputdir [resource_ids...]\n");return 2; }
 setvbuf(stdout,nullptr,_IONBF,0);
 std::filesystem::create_directories(argv[2]);
 std::string base=argv[2];
 std::ofstream res(base+"/resources.tsv"), actions(base+"/actions.tsv");
 std::ofstream tex(base+"/textures.tsv"), debug(base+"/debug-messages.tsv");
 if(!res || !actions || !tex || !debug) {
   fprintf(stderr,"FAILURE: cannot open output TSV files\n"); return 5;
 }
 std::set<unsigned long long> selected;
 for(int i=3;i<argc;i++) {
   try {
     size_t consumed=0; const std::string value=argv[i];
     auto id=std::stoull(value,&consumed);
     if(consumed!=value.size() || value.empty() || value.front()=='-') throw std::invalid_argument("resource ID");
     selected.insert(id);
   } catch(const std::exception&) { fprintf(stderr,"FAILURE: invalid resource ID %s\n",argv[i]); return 5; }
 }
 printf("initialise\n");
 RENDERDOC_InitialiseReplay(GlobalEnvironment(),{});
 ICaptureFile *cap=RENDERDOC_OpenCaptureFile();
 auto status=cap->OpenFile(argv[1],"",nullptr);
 printf("OpenFile %u %s\n",unsigned(status.code),(status.internal_msg?status.internal_msg->c_str():""));
 if(!status.OK()) {cap->Shutdown();RENDERDOC_ShutdownReplay();return 3;}
 printf("opening GPU replay\n");
 auto result=cap->OpenCapture(ReplayOptions(),nullptr);
 printf("OpenCapture %u %s\n",unsigned(result.first.code),(result.first.internal_msg?result.first.internal_msg->c_str():""));
 if(!result.first.OK()) {cap->Shutdown();RENDERDOC_ShutdownReplay();return 4;}
 IReplayController *ctrl=result.second;
 std::map<unsigned long long,std::string> names;
 for(const auto &r:ctrl->GetResources()) {
   names[ID(r.resourceId)]=r.name.c_str();
   res<<ID(r.resourceId)<<'\t'<<unsigned(r.type)<<'\t'<<r.name.c_str()<<'\n';
 }
 unsigned last=0;
 Walk(ctrl->GetRootActions(),actions,last);
 printf("replaying event %u\n",last);
 ctrl->SetFrameEvent(last,true);
 std::set<unsigned long long> remaining=selected;
 unsigned saved=0,failed=0;
 for(const auto &t:ctrl->GetTextures()) {
   auto id=ID(t.resourceId);
   tex<<id<<'\t'<<t.width<<'\t'<<t.height<<'\t'<<t.depth<<'\t'<<t.arraysize<<'\t'<<unsigned(t.creationFlags)<<'\t'<<t.format.Name().c_str()<<'\t'<<names[id]<<'\n';
   bool take=selected.empty()? bool(t.creationFlags & TextureCategory::ColorTarget):selected.count(id)!=0;
   if(!take) continue;
   TextureSave save;save.resourceId=t.resourceId;save.destType=FileType::PNG;save.mip=0;save.slice.sliceIndex=0;
   std::string file=base+"/texture-"+std::to_string(id)+".png";
   auto s=ctrl->SaveTexture(save,file.c_str());
   printf("Save %llu %ux%u %u %s\n",id,t.width,t.height,unsigned(s.code),(s.internal_msg?s.internal_msg->c_str():""));
   if(s.OK()) { ++saved; remaining.erase(id); } else { ++failed; }
 }
 for(const auto &m:ctrl->GetDebugMessages()) debug<<m.eventId<<'\t'<<unsigned(m.severity)<<'\t'<<m.description.c_str()<<'\n';
 res.flush();actions.flush();tex.flush();debug.flush();
 const bool reportsOkay=res && actions && tex && debug;
 for(auto id:remaining) printf("Requested texture not saved: %llu\n",id);
 const bool okay=saved>0 && failed==0 && remaining.empty() && reportsOkay;
 ctrl->Shutdown();cap->Shutdown();
 printf("%s saved=%u failed=%u requested_unsaved=%zu reports_ok=%d last=%u\n",
        okay?"SUCCESS":"FAILURE",saved,failed,remaining.size(),reportsOkay,last);
 RENDERDOC_ShutdownReplay();return okay?0:6;
}

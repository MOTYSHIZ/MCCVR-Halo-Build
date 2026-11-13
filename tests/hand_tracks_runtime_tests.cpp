#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>
#include "../src/dll/hand_tracks.h"
#include "../src/common/config.h"

namespace {
unsigned checks=0,failures=0;
void Check(bool value,const char* name){++checks;if(!value){++failures;std::printf("FAIL: %s\n",name);}}

std::wstring MakeFolder()
{
    wchar_t temp[MAX_PATH]{};GetTempPathW(MAX_PATH,temp);
    wchar_t folder[MAX_PATH]{};
    swprintf_s(folder,L"%shalomccvr_hand_tracks_test_%lu\\",temp,GetCurrentProcessId());
    CreateDirectoryW(folder,nullptr);
    return folder;
}
std::wstring FilePath(const std::wstring& folder){return folder+L"halomccvr_hand_tracks.txt";}
bool ReadAll(const std::wstring& path,std::string& out)
{
    FILE* f=nullptr;if(_wfopen_s(&f,path.c_str(),L"rb")||!f)return false;
    char buffer[4096];size_t n=0;out.clear();
    while((n=fread(buffer,1,sizeof(buffer),f))>0)out.append(buffer,n);
    fclose(f);return true;
}
void WriteAll(const std::wstring& path,const char* text)
{
    FILE* f=nullptr;if(_wfopen_s(&f,path.c_str(),L"wb")||!f)return;
    fwrite(text,1,strlen(text),f);fclose(f);
}
}

int main()
{
    g_config.support_hand_tracks=true;
    const std::wstring folder=MakeFolder();
    DeleteFileW(FilePath(folder).c_str());
    hand_track::Sampled sample{};

    // Before any library is loaded, nothing plays.
    HandTracks_Request(HandTrackRequest::Preview);
    HandTracks_Update(100,GameTitle::Halo3,0,false);
    Check(!HandTracks_Current(sample),"no library, no track");

    HandTracks_SetFolder(folder.c_str());
    HandTracks_Poll(1000);
    std::string written;
    Check(ReadAll(FilePath(folder),written)&&written==hand_track::kDefaultText,"first run writes the built-in tracks file");
    HandTracksStatus status{};HandTracks_GetStatus(status);
    Check(status.loaded&&!status.fromFile&&status.tracks==2,"built-in tracks in use until the file is read");
    HandTracks_Poll(3100);
    HandTracks_GetStatus(status);
    Check(status.loaded&&status.fromFile&&status.tracks==2&&!status.errors,"the written file is read back");

    HandTracks_Request(HandTrackRequest::Preview);
    HandTracks_Update(5000,GameTitle::Halo3,0,false);
    Check(HandTracks_Current(sample)&&sample.a.frame==hand_track::Frame::Authored,"preview plays on Halo 3");
    HandTracks_GetStatus(status);
    Check(status.playing&&std::strcmp(status.playingName,"preview")==0&&status.plays==1,"status names the playing track");
    HandTracks_Update(5800,GameTitle::Halo3,0,false);
    Check(HandTracks_Current(sample)&&sample.weight>0.99f,"mid-track the hand is fully on the path");
    HandTracks_Update(5000+1600+250,GameTitle::Halo3,0,false);
    Check(!HandTracks_Current(sample),"track ends after its blend out");
    HandTracks_GetStatus(status);
    Check(!status.playing,"status clears when the track ends");

    HandTracks_Request(HandTrackRequest::Reload);
    HandTracks_Update(7000,GameTitle::Halo3ODST,0,false);
    Check(HandTracks_Current(sample),"reload plays on ODST");
    HandTracks_Update(7050,GameTitle::Halo3ODST,0,true);
    Check(!HandTracks_Current(sample),"dual wielding stops the track");

    HandTracks_Request(HandTrackRequest::Reload);
    HandTracks_Update(8000,GameTitle::HaloReach,0,false);
    Check(!HandTracks_Current(sample),"other titles never play tracks");

    g_config.support_hand_tracks=false;
    HandTracks_Request(HandTrackRequest::Reload);
    HandTracks_Update(9000,GameTitle::Halo3,0,false);
    Check(!HandTracks_Current(sample),"the setting turns tracks off");
    g_config.support_hand_tracks=true;

    // An edited file reloads by itself; a broken track is reported and skipped.
    WriteAll(FilePath(folder),
        "track reload *\nkey 0 weapon 0.1 0 0 0 0 0 1\nkey 0.5 weapon 0.1 0 0 0 0 0 1\nend\n"
        "track reload assault_rifle\nkey 0 weapon 0.2 0 0 0 0 0 1\nend\n"
        "track broken *\nkey 0 nowhere 0 0 0 0 0 0 1\nend\n");
    HandTracks_Poll(20000);
    HandTracks_GetStatus(status);
    Check(status.fromFile&&status.tracks==2&&status.dropped==1&&status.firstErrorLine==9,"edited file reloads and reports its problem line");
    HandTracks_Request(HandTrackRequest::Reload);
    HandTracks_Update(21000,GameTitle::Halo3,0x0000000013120D11ull,false);
    Check(HandTracks_Current(sample)&&sample.a.offset[0]==0.2f,"the assault rifle uses its own reload track");
    HandTracks_Request(HandTrackRequest::Preview);
    HandTracks_Update(21100,GameTitle::Halo3,0x0000000013120D11ull,false);
    Check(HandTracks_Current(sample)&&sample.a.offset[0]==0.2f,"a missing track leaves the current one playing");

    // Deleting the file restores the built-in tracks and writes them again.
    DeleteFileW(FilePath(folder).c_str());
    HandTracks_Poll(30000);
    HandTracks_GetStatus(status);
    Check(!status.fromFile&&status.tracks==2,"deleted file falls back to the built-in tracks");
    Check(ReadAll(FilePath(folder),written)&&written==hand_track::kDefaultText,"deleted file is written again");

    DeleteFileW(FilePath(folder).c_str());
    RemoveDirectoryW(folder.c_str());
    std::printf("hand_tracks_runtime_tests: %u checks, %u failures\n",checks,failures);
    return failures?1:0;
}

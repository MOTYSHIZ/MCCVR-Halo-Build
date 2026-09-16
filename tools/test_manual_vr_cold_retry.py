"""Compile the actual failed cold-retry functions against bounded state fakes.

Usage: python tools/test_manual_vr_cold_retry.py --build
This executes policy only: no game process, native module, driver or hook access.
"""
from pathlib import Path
import argparse, hashlib, json, subprocess

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build', action='store_true', help='build and run the generated fixture')
args=parser.parse_args()
root=Path(__file__).resolve().parents[1]
out=root/'out/reentry-cold-retry-test'
out.mkdir(parents=True,exist_ok=True)
cases=[('h2','halo2_cold_observation.cpp','Halo2ColdObservation_RetryFailed'),
       ('h4','halo4_cold_observation.cpp','Halo4ColdObservation_RetryFailed'),
       ('reach','reach_render_candidate.cpp','ReachRenderCandidate_RetryFailed')]
src='#include <cstdint>\n#include <cstddef>\n#include <cstdio>\n#define LOG(...) ((void)0)\n'
source_hashes={}
for ns,path,name in cases:
    text=(root/'src/dll'/path).read_text(encoding='utf-8')
    begin=text.index('bool '+name+'(')
    brace=text.index('{',begin)
    depth=1; end=brace+1
    while depth:
        depth += (text[end]=='{')-(text[end]=='}'); end+=1
    body=text[begin:end]
    source_hashes[path]=hashlib.sha256(body.encode()).hexdigest()
    src+='namespace '+ns+' {\n'
    if ns=='h2':
        src+='uintptr_t g_gateBase=0,g_completedBase=0; size_t g_gateSize=0; uint32_t g_gateGeneration=0,g_completedGeneration=0,g_passedGeneration=0; bool g_passed=false,g_gateAnchorsChecked=false,g_gateAnchorsProven=false,g_completed=false; unsigned resets=0; void ResetGate(uintptr_t b,size_t s,uint32_t g) {g_gateBase=b;g_gateSize=s;g_gateGeneration=g;g_gateAnchorsChecked=g_gateAnchorsProven=false;++resets;}\n'
    elif ns=='h4':
        src+='uintptr_t g_attemptedBase=0; uint32_t g_attemptedGeneration=0; bool g_passed=false,g_attempted=false;\n'
    else:
        src+='struct ReachModuleEpoch {uintptr_t moduleBase=0;uint32_t generation=0;}; bool ReachModuleEpochValid(const ReachModuleEpoch&e){return e.moduleBase&&e.generation;} bool ReachSameModuleEpoch(const ReachModuleEpoch&a,const ReachModuleEpoch&b){return a.moduleBase==b.moduleBase&&a.generation==b.generation;} ReachModuleEpoch g_attemptedEpoch; bool g_attempted=false; struct Publication{bool current=false;bool HasCurrent(){return current;}} g_preflightPublication;\n'
    src+=body+'\n}\n'
src+='''unsigned checks=0,failures=0;
void check(bool v,const char* name){++checks;if(!v){++failures;std::printf("FAIL %s\\n",name);}}
int main(){
for(int state=0;state<9;++state){
 using namespace h2;
 g_gateBase=g_completedBase=0x1000;g_gateGeneration=g_completedGeneration=g_passedGeneration=3;g_gateSize=4096;
 g_passed=false;g_completed=true;g_gateAnchorsChecked=g_gateAnchorsProven=true;resets=0;
 uintptr_t base=0x1000;uint32_t gen=3;
 if(state==0)g_passed=true;
 if(state==1)base=0;
 if(state==2)gen=0;
 if(state==3)base=0x2000;
 if(state==4)gen=4;
 if(state==5){g_completed=false;g_gateAnchorsProven=false;}
 if(state==6){g_completed=false;}
 if(state==8){g_passed=true;g_completedBase=0x2000;g_completedGeneration=g_passedGeneration=2;g_gateAnchorsProven=false;}
 const bool expected=state==5||state==7||state==8;
 check(Halo2ColdObservation_RetryFailed(base,gen)==expected,"H2 exact failed state");
 check(resets==unsigned(expected),"H2 only matching failed gate resets");
 if(expected){check((state==8||!g_completed)&&!g_gateAnchorsChecked&&!g_gateAnchorsProven,"H2 proof/liveness must be re-earned");check(!Halo2ColdObservation_RetryFailed(base,gen),"H2 no duplicate retry");}
 if(state==8)check(g_passed&&g_completed&&g_completedBase==0x2000&&g_passedGeneration==2,"H2 unrelated old PASS preserved while new gate retries");
 if(state==0)check(g_passed&&g_completed&&g_gateAnchorsProven,"H2 PASS preserved");
}
for(int state=0;state<7;++state){
 using namespace h4;
 g_attemptedBase=0x1000;g_attemptedGeneration=3;g_passed=false;g_attempted=true;
 uintptr_t base=0x1000;uint32_t gen=3;
 if(state==0)g_passed=true;if(state==1)base=0;if(state==2)gen=0;if(state==3)base=0x2000;if(state==4)gen=4;if(state==5)g_attempted=false;
 const bool expected=state==6;
 check(Halo4ColdObservation_RetryFailed(base,gen)==expected,"H4 exact failed state");
 if(expected){check(!g_attempted&&!g_attemptedBase&&!g_attemptedGeneration,"H4 normal poll must reverify");check(!Halo4ColdObservation_RetryFailed(base,gen),"H4 no duplicate retry");}
 if(state==0)check(g_passed&&g_attempted,"H4 PASS preserved");
}
for(int state=0;state<7;++state){
 using namespace reach;
 g_attemptedEpoch={0x1000,3};g_preflightPublication.current=false;g_attempted=true;
 ReachModuleEpoch epoch{0x1000,3};
 if(state==0)g_preflightPublication.current=true;if(state==1)epoch.moduleBase=0;if(state==2)epoch.generation=0;if(state==3)epoch.moduleBase=0x2000;if(state==4)epoch.generation=4;if(state==5)g_attempted=false;
 const bool expected=state==6;
 check(ReachRenderCandidate_RetryFailed(epoch)==expected,"Reach exact failed state");
 if(expected){check(!g_attempted&&!g_attemptedEpoch.moduleBase&&!g_attemptedEpoch.generation,"Reach normal poll must reverify");check(!ReachRenderCandidate_RetryFailed(epoch),"Reach no duplicate retry");}
 if(state==0)check(g_preflightPublication.current&&g_attempted,"Reach PASS preserved");
}
std::printf("Extracted production cold-retry functions: %u checks, %u failures\\n",checks,failures);return failures?1:0;
}
'''
(out/'fixture.cpp').write_text(src,encoding='utf-8')
(out/'source-hashes.json').write_text(json.dumps(source_hashes,indent=2)+'\n',encoding='utf-8')
(out/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.24)\nproject(manual_cold_retry LANGUAGES CXX)\nadd_executable(manual_cold_retry fixture.cpp)\nset_property(TARGET manual_cold_retry PROPERTY CXX_STANDARD 20)\n',encoding='utf-8')
if args.build:
    subprocess.run(['cmake','-S',str(out),'-B',str(out/'build'),'-A','x64'],check=True)
    subprocess.run(['cmake','--build',str(out/'build'),'--config','Release'],check=True)
    result=subprocess.run([str(out/'build/Release/manual_cold_retry.exe')],text=True,capture_output=True)
    (out/'result.txt').write_text(result.stdout+result.stderr,encoding='utf-8')
    print(result.stdout+result.stderr,end='')
    result.check_returncode()

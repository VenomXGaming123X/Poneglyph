#include "poneglyph/report.hpp"
#include "poneglyph/util.hpp"
#include <algorithm>
#include <cstdio>
namespace pg {
std::vector<std::string> buildRecommendations(const ReportInput& in){
    std::vector<std::string> r; char b[256];
    for(auto& a:in.addons){
        if(a.status==AddonStatus::BlockedProxy) r.push_back("Remove or move '"+a.rel+"' out of the Addons folder: it is a Direct3D 9 proxy and conflicts with Poneglyph's engine.");
        if(a.status==AddonStatus::WrongArch) r.push_back("'"+a.rel+"' is not 32-bit and cannot work in this game. Get an x86 version.");
        if(a.status==AddonStatus::MissingDependency) r.push_back("'"+a.rel+"': "+a.reason);
        if(!a.duplicateOfHash.empty()) r.push_back("'"+a.rel+"' is byte-identical to '"+a.duplicateOfHash+"'. Keep only one copy.");
    }
    for(auto& l:in.loadRecords){
        if(l.result=="Failed") r.push_back("Add-on '"+l.file+"' failed to load ("+l.detail+"). Update it or disable it.");
        if(l.result=="Loaded"&&l.ms>500.0){ std::snprintf(b,sizeof b,"Add-on '%s' took %.0f ms to initialise at startup. That is a measured load-time cost; it does not by itself prove in-game slowdowns.",l.file.c_str(),l.ms); r.push_back(b); }
    }
    if(in.haveFrames&&in.frames.frames>200){
        if(in.frames.spikes>0&&in.frames.p99>in.frames.p50*2.5){
            std::snprintf(b,sizeof b,"Frame-time spikes were measured (p99 %.1f ms vs median %.1f ms, %zu spikes). To find the cause, disable add-ons one at a time and repeat the same scene; installing an add-on during a slowdown is not proof it caused it.",in.frames.p99,in.frames.p50,in.frames.spikes); r.push_back(b); }
    }
    if(in.exe.valid&&in.exe.isX86()&&!in.exe.laa) r.push_back("The game executable is not Large Address Aware. The Memory page can enable the 4 GB flag (with automatic backup). This raises the 32-bit address-space limit to 4 GB on 64-bit Windows; it does not add RAM and never exceeds 4 GB.");
    if(r.empty()) r.push_back("No problems were detected by the checks that can be measured. This is not a guarantee of performance.");
    return r;
}
std::string buildReport(const ReportInput& in){
    std::string o; char b[512];
    o+="PONEGLYPH REPORT\n================\n";
    o+="Created      : "+nowStamp()+"\n";
    o+="Manager      : "+in.managerVersion+"   Engine: "+(in.engineVersion.empty()?"unknown":in.engineVersion)+"\n";
    o+="Game         : "+in.gameName+"\nExecutable   : "+in.exePath+"\n";
    if(in.exe.valid){ o+="Architecture : "+in.exe.archName()+"\n4 GB flag    : "+std::string(in.exe.laa?"ON (Large Address Aware)":"OFF")+"\n"; }
    if(!in.note.empty()) o+="Note         : "+in.note+"\n";
    o+="\nADD-ONS ("+std::to_string(in.addons.size())+" found)\n";
    for(auto& a:in.addons){ o+=" - "+a.rel+"  ["+addonStatusText(a.status)+"]"+(a.order>=0?" order "+std::to_string(a.order+1):"")+"\n"; if(!a.reason.empty()) o+="     "+a.reason+"\n";
        if(!a.sha256.empty()) o+="     SHA-256 "+a.sha256+"\n"; o+="     Compatibility: "+a.compat+"\n"; }
    o+="\nLAST ENGINE LOAD RESULTS\n"; if(in.loadRecords.empty()) o+=" (none - the game has not been started with Poneglyph yet)\n";
    for(auto& l:in.loadRecords){ std::snprintf(b,sizeof b," - %-32s %-8s %8.2f ms  %s\n",l.file.c_str(),l.result.c_str(),l.ms,l.detail.c_str()); o+=b; }
    o+="\nPROCESS MONITOR (Level 1)\n";
    if(in.samples.empty()) o+=" (no samples recorded)\n"; else {
        double cs=0,cm=0; uint64_t wm=0,ws=0; for(auto& s:in.samples){ cs+=s.cpuPct; cm=std::max(cm,s.cpuPct); wm=std::max(wm,s.workingSetBytes); ws+=s.workingSetBytes; }
        std::snprintf(b,sizeof b," samples=%zu  CPU avg %.1f%%  max %.1f%%   Working set avg %s  peak %s\n",in.samples.size(),cs/(double)in.samples.size(),cm,formatBytes(ws/in.samples.size()).c_str(),formatBytes(wm).c_str()); o+=b; }
    o+="\nFRAME PERFORMANCE (Level 2)\n";
    if(!in.haveFrames) o+=" No measured frame data. Poneglyph does not invent FPS values; enable 'Frame statistics' in Settings and play a session.\n";
    else { auto& f=in.frames; std::snprintf(b,sizeof b," frames=%zu  avg %.1f FPS (%.2f ms)  median %.2f ms  p95 %.2f  p99 %.2f  max %.2f ms  1%% low %.1f FPS  0.1%% low %.1f FPS  spikes %zu\n",f.frames,f.fps,f.avgMs,f.p50,f.p95,f.p99,f.maxMs,f.low1Fps,f.low01Fps,f.spikes); o+=b; }
    o+="\nRECOMMENDATIONS (Level 4 - suggestions only, nothing is changed automatically)\n"; for(auto& r:buildRecommendations(in)) o+=" * "+r+"\n";
    if(!in.warnings.empty()){ o+="\nWARNINGS\n"; for(auto& w:in.warnings) o+=" ! "+w+"\n"; }
    if(!in.changes.empty()){ o+="\nCHANGES MADE DURING THIS SESSION\n"; for(auto& c:in.changes) o+=" + "+c+"\n"; }
    o+="\nLimits: add-on callback profiling (Level 3 per-callback timing) is not implemented in v1.0; only load/initialisation time is measured.\n";
    return o;
}
}

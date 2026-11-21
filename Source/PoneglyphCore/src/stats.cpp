#include "poneglyph/stats.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
namespace pg {
double percentile(std::vector<double> v,double p){
    if(v.empty()) return 0; std::sort(v.begin(),v.end());
    double r=std::ceil(p/100.0*(double)v.size()); size_t i=(size_t)std::max(1.0,r)-1; if(i>=v.size()) i=v.size()-1; return v[i];
}
FrameSummary summarizeFrames(std::vector<double> ms){
    FrameSummary s; s.frames=ms.size(); if(ms.empty()) return s;
    double sum=0; for(double m:ms){ sum+=m; if(m>s.maxMs) s.maxMs=m; }
    s.seconds=sum/1000.0; s.avgMs=sum/(double)ms.size(); s.fps=s.avgMs>0?1000.0/s.avgMs:0;
    s.p50=percentile(ms,50); s.p95=percentile(ms,95); s.p99=percentile(ms,99);
    double thr=std::max(2.0*s.p50,25.0); for(double m:ms) if(m>thr) s.spikes++;
    std::sort(ms.begin(),ms.end(),std::greater<double>());
    auto lowFps=[&](double frac){ size_t n=(size_t)std::max(1.0,std::ceil(frac*(double)ms.size())); double a=0; for(size_t i=0;i<n;i++) a+=ms[i]; a/=(double)n; return a>0?1000.0/a:0.0; };
    s.low1Fps=lowFps(0.01); s.low01Fps=lowFps(0.001); return s;
}
std::string summaryCsvHeader(){ return "timestamp,frames,seconds,avg_fps,avg_ms,p50_ms,p95_ms,p99_ms,max_ms,low1pct_fps,low0.1pct_fps,spikes"; }
std::string summaryCsvLine(const std::string& t,const FrameSummary& s){
    char b[384]; std::snprintf(b,sizeof b,"%s,%zu,%.2f,%.2f,%.3f,%.3f,%.3f,%.3f,%.3f,%.2f,%.2f,%zu",t.c_str(),s.frames,s.seconds,s.fps,s.avgMs,s.p50,s.p95,s.p99,s.maxMs,s.low1Fps,s.low01Fps,s.spikes); return b;
}
}

// Poneglyph - frame-time statistics (used by the engine's lightweight sampler and the manager).
#pragma once
#include <vector>
#include <string>
namespace pg {
struct FrameSummary {
    size_t frames=0; double seconds=0, avgMs=0, fps=0, p50=0, p95=0, p99=0, maxMs=0, low1Fps=0, low01Fps=0; size_t spikes=0;
};
// frameMs: duration of each frame in milliseconds. A "spike" is a frame > max(2.0 * median, 25 ms).
FrameSummary summarizeFrames(std::vector<double> frameMs);
double percentile(std::vector<double> v,double p);   // p in [0,100], nearest-rank
std::string summaryCsvHeader();
std::string summaryCsvLine(const std::string& timestamp,const FrameSummary& s);
}

// Poneglyph - plain-text diagnostic report (Master Plan section 6).
#pragma once
#include <string>
#include <vector>
#include "poneglyph/addons.hpp"
#include "poneglyph/stats.hpp"
namespace pg {
struct ProcSample { std::string time; double cpuPct=0; uint64_t workingSetBytes=0; };
struct ReportInput {
    std::string gameName, exePath, engineVersion, managerVersion, note;
    PeInfo exe; std::vector<AddonEntry> addons; std::vector<AddonLoadRecord> loadRecords;
    std::vector<ProcSample> samples; bool haveFrames=false; FrameSummary frames; std::vector<std::string> warnings, changes;
};
std::string buildReport(const ReportInput& in);
// Level 4 recommendations: evidence-based only. Never claims causation from installation alone.
std::vector<std::string> buildRecommendations(const ReportInput& in);
}

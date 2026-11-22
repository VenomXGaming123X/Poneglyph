#include "poneglyph/launchopts.hpp"
#include "poneglyph/util.hpp"
namespace pg {
bool launchOptionsSupported(GameId g){ return g==GameId::FFXIII||g==GameId::FFXIII2; }
LaunchOptions loadLaunchOptions(const IniFile& c){
    LaunchOptions o; o.fullscreen=c.getBool("Launch","FullScreen",false); o.width=c.getInt("Launch","Width",0); o.height=c.getInt("Launch","Height",0);
    o.msaa=c.getInt("Launch","MSAA",0); o.shadow=c.getInt("Launch","Shadow",0); o.voiceJP=c.getBool("Launch","VoiceJP",false); o.decideReverse=c.getBool("Launch","DecideButtonReverse",false); return o;
}
void saveLaunchOptions(IniFile& c,const LaunchOptions& o){
    c.setBool("Launch","FullScreen",o.fullscreen); c.setInt("Launch","Width",o.width); c.setInt("Launch","Height",o.height); c.setInt("Launch","MSAA",o.msaa);
    c.setInt("Launch","Shadow",o.shadow); c.setBool("Launch","VoiceJP",o.voiceJP); c.setBool("Launch","DecideButtonReverse",o.decideReverse);
}
std::vector<std::string> validateLaunchOptions(const LaunchOptions& o){
    std::vector<std::string> p;
    if((o.width==0)!=(o.height==0)) p.push_back("Width and height must both be set, or both left on 'Game default'.");
    if(o.width!=0&&(o.width<640||o.width>7680)) p.push_back("Width must be between 640 and 7680.");
    if(o.height!=0&&(o.height<360||o.height>4320)) p.push_back("Height must be between 360 and 4320.");
    if(!(o.msaa==0||o.msaa==2||o.msaa==4||o.msaa==8||o.msaa==16)) p.push_back("Anti-aliasing must be Off, 2x, 4x, 8x or 16x.");
    if(!(o.shadow==0||o.shadow==512||o.shadow==1024||o.shadow==2048||o.shadow==4096||o.shadow==8192)) p.push_back("Shadow quality must be one of 512, 1024, 2048, 4096 or 8192.");
    return p;
}
std::vector<std::string> buildLaunchArgs(GameId g,const LaunchOptions& o){
    std::vector<std::string> a; if(!launchOptionsSupported(g)||!validateLaunchOptions(o).empty()) return a;
    if(o.width>0&&o.height>0){ a.push_back("-Width="+std::to_string(o.width)); a.push_back("-Height="+std::to_string(o.height)); }
    if(o.fullscreen) a.push_back("-FullScreenMode=Force");
    if(o.msaa>0) a.push_back("-MSAA="+std::to_string(o.msaa));
    if(o.shadow>0) a.push_back("-Shadow="+std::to_string(o.shadow));
    if(o.voiceJP) a.push_back("-VoiceJPMode");
    if(o.decideReverse) a.push_back("-DecideButtonReverse");
    return a;
}
std::string joinArgs(const std::vector<std::string>& a){ return join(a," "); }
}

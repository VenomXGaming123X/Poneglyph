// Poneglyph Core unit tests. Self-contained (no framework). Exit code 0 = all passed.
#include "poneglyph/sha256.hpp"
#include "poneglyph/ini.hpp"
#include "poneglyph/util.hpp"
#include "poneglyph/pe.hpp"
#include "poneglyph/zip.hpp"
#include "poneglyph/games.hpp"
#include "poneglyph/addons.hpp"
#include "poneglyph/ops.hpp"
#include "poneglyph/stats.hpp"
#include "poneglyph/report.hpp"
#include "poneglyph/log.hpp"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <cstdlib>
using namespace pg;
static int g_pass=0,g_fail=0;
#define CHECK(c) do{ if(c){g_pass++;}else{g_fail++;std::printf("  FAIL %s:%d  %s\n",__FILE__,__LINE__,#c);} }while(0)
static fs::path T;
static void wr(const fs::path& p,const std::vector<uint8_t>& d){ fs::create_directories(p.parent_path()); std::ofstream o(p,std::ios::binary); o.write((const char*)d.data(),(std::streamsize)d.size()); }
static void put16(std::vector<uint8_t>& v,size_t o,uint16_t x){ memcpy(&v[o],&x,2); }
static void put32(std::vector<uint8_t>& v,size_t o,uint32_t x){ memcpy(&v[o],&x,4); }
// Builds a minimal valid PE32 with an export table (names) and an import of kernel32.dll.
static std::vector<uint8_t> makePe(uint16_t machine,uint16_t chars,const std::vector<std::string>& exports,uint32_t checksum=0,bool pe32plus=false){
    std::vector<uint8_t> v(0x400+0x400,0);
    v[0]='M';v[1]='Z'; put32(v,0x3c,0x80); put32(v,0x80,0x4550);
    put16(v,0x84,machine); put16(v,0x86,1); put32(v,0x88,0x5f000000); put16(v,0x94,pe32plus?240:224); put16(v,0x96,chars);
    size_t o=0x98; put16(v,o,pe32plus?0x20b:0x10b); size_t dd=o+(pe32plus?112:96);
    put32(v,o+60,0x400); put32(v,o+64,checksum); put16(v,o+68,3); put32(v,dd-4,16);
    // section .rdata va 0x1000 raw 0x400 size 0x400
    size_t s=o+(pe32plus?240:224)-0; s=0x98+(pe32plus?240:224);
    memcpy(&v[s],".rdata",6); put32(v,s+8,0x400); put32(v,s+12,0x1000); put32(v,s+16,0x400); put32(v,s+20,0x400);
    if(v.size()<s+40) v.resize(s+40);
    // export dir at rva 0x1000 (file 0x400)
    size_t e=0x400; uint32_t n=(uint32_t)exports.size();
    put32(v,e+16,1); put32(v,e+20,n); put32(v,e+24,n); put32(v,e+28,0x1100); put32(v,e+32,0x1140); put32(v,e+36,0x1180);
    size_t str=0x400+0x200; 
    for(uint32_t i=0;i<n;i++){ put32(v,0x400+0x100+i*4,0x1000+0x100+0); /*func rva placeholder*/ put32(v,0x400+0x100+i*4,0x1300); put32(v,0x400+0x140+i*4,(uint32_t)(0x1000+(str-0x400))); put16(v,0x400+0x180+i*2,(uint16_t)i);
        memcpy(&v[str],exports[i].c_str(),exports[i].size()+1); str+=exports[i].size()+1; }
    put32(v,dd+0,0x1000); put32(v,dd+4,0x100); // export dir (size small so funcs not "forwarded")
    // import dir at 0x1200 -> descriptor then name
    size_t im=0x400+0x200+0x1f0; (void)im;
    return v;
}
static void testSha(){
    CHECK(Sha256::ofBuffer("abc",3)=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(Sha256::ofBuffer("",0)=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    std::string m(1000000,'a'); CHECK(Sha256::ofBuffer(m.data(),m.size())=="cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}
static void testIniUtil(){
    IniFile i; i.loadFromString("; c\n[General]\nGame = ffxiii2\nEnabled=1\n[Addons]\nfoo/bar.asi=0\n");
    CHECK(i.get("general","GAME")=="ffxiii2"); CHECK(i.getBool("General","enabled",false)); CHECK(!i.getBool("Addons","foo/bar.asi",true)); CHECK(i.getInt("X","y",7)==7);
    i.set("Addons","new","1"); IniFile j; j.loadFromString(i.toString()); CHECK(j.get("Addons","new")=="1");
    CHECK(isSafeRelativePath("a/b.dll")); CHECK(!isSafeRelativePath("../x")); CHECK(!isSafeRelativePath("a/../../x")); CHECK(!isSafeRelativePath("/etc/passwd"));
    CHECK(!isSafeRelativePath("C:/x")); CHECK(!isSafeRelativePath("a\\..\\b")); CHECK(!isSafeRelativePath("con.txt")); CHECK(!isSafeRelativePath("a/b:stream")); CHECK(!isSafeRelativePath("x."));
    CHECK(isInside("/tmp/a","/tmp/a/b/c")); CHECK(!isInside("/tmp/a","/tmp/ab/c")); CHECK(!isInside("/tmp/a","/tmp/a/../b"));
}
static void testPe(){
    auto good=makePe(0x14c,0x0102|0x2000,{"Direct3DCreate9","Foo"});
    wr(T/"pe/good.dll",good); auto p=inspectPe(T/"pe/good.dll");
    CHECK(p.valid); CHECK(p.isX86()); CHECK(p.isDll); CHECK(!p.laa); CHECK(p.exports.size()==2); CHECK(p.exports[0].name=="Direct3DCreate9"); CHECK(p.exports[0].ordinal==1&&p.exports[1].ordinal==2);
    auto x64=makePe(0x8664,0x0022,{},0,true); wr(T/"pe/x64.exe",x64); auto q=inspectPe(T/"pe/x64.exe"); CHECK(q.valid&&q.is64()&&!q.isX86()&&q.laa);
    std::vector<uint8_t> junk(100,7); wr(T/"pe/junk.bin",junk); CHECK(!inspectPe(T/"pe/junk.bin").valid);
    std::vector<uint8_t> trunc(good.begin(),good.begin()+0x90); wr(T/"pe/trunc.exe",trunc); CHECK(!inspectPe(T/"pe/trunc.exe").valid);
    CHECK(!inspectPe(T/"pe/missing.exe").valid);
}
static void testLaa(){
    auto exe=makePe(0x14c,0x0102,{},0); for(size_t i=0x500;i<exe.size();i++) exe[i]=(uint8_t)(i*7);
    wr(T/"laa/ffxiii2img.exe",exe); std::string h0; Sha256::ofFile(T/"laa/ffxiii2img.exe",h0);
    auto bad=patchLaa(T/"laa/ffxiii2img.exe",T/"laa/bak",{"LRFF13.exe"}); CHECK(bad.result==LaaResult::WrongExecutable);
    std::string h1; Sha256::ofFile(T/"laa/ffxiii2img.exe",h1); CHECK(h0==h1);
    auto r=patchLaa(T/"laa/ffxiii2img.exe",T/"laa/bak",{"ffxiii2img.exe"}); CHECK(r.result==LaaResult::Patched);
    auto p=inspectPe(T/"laa/ffxiii2img.exe"); CHECK(p.valid&&p.laa&&p.isX86());
    std::ifstream a(T/"laa/ffxiii2img.exe",std::ios::binary); std::vector<uint8_t> now((std::istreambuf_iterator<char>(a)),{}); int diff=0; for(size_t i=0;i<exe.size();i++) if(now[i]!=exe[i]) diff++; CHECK(diff==1); // only the flag byte
    CHECK(patchLaa(T/"laa/ffxiii2img.exe",T/"laa/bak").result==LaaResult::AlreadyLaa);
    auto rs=restoreFromBackup(r.backupPath,T/"laa/ffxiii2img.exe"); CHECK(rs.result==LaaResult::Patched);
    std::string h2; Sha256::ofFile(T/"laa/ffxiii2img.exe",h2); CHECK(h2==h0);
    // checksum-bearing file: stored checksum updated and equals computed
    auto ck=makePe(0x14c,0x0102,{},0); put32(ck,0x98+64,0xdeadbeef); wr(T/"laa/ck.exe",ck);
    auto r2=patchLaa(T/"laa/ck.exe",T/"laa/bak"); CHECK(r2.result==LaaResult::Patched);
    auto p2=inspectPe(T/"laa/ck.exe"); CHECK(p2.storedChecksum==p2.calculatedChecksum&&p2.storedChecksum!=0xdeadbeef);
    // refusals
    wr(T/"laa/x64.exe",makePe(0x8664,0x0002,{},0,true)); CHECK(patchLaa(T/"laa/x64.exe",T/"laa/bak").result==LaaResult::NotPe32);
    wr(T/"laa/lib.dll",makePe(0x14c,0x2102,{},0)); CHECK(patchLaa(T/"laa/lib.dll",T/"laa/bak").result==LaaResult::IsDll);
    wr(T/"laa/junk.exe",std::vector<uint8_t>(50,1)); CHECK(patchLaa(T/"laa/junk.exe",T/"laa/bak").result==LaaResult::FileError);
    // tampered backup is refused
    { std::fstream f(r.backupPath,std::ios::in|std::ios::out|std::ios::binary); f.seekp(0x600); char c=0x55; f.write(&c,1); }
    CHECK(restoreFromBackup(r.backupPath,T/"laa/ffxiii2img.exe").result==LaaResult::VerifyFailed);
}
// ---- zip fixtures
static void le16(std::vector<uint8_t>& v,uint16_t x){ v.push_back(x&255); v.push_back(x>>8); }
static void le32(std::vector<uint8_t>& v,uint32_t x){ for(int i=0;i<4;i++) v.push_back((x>>(8*i))&255); }
struct ZE{ std::string name; std::vector<uint8_t> data; uint16_t method; uint32_t crc; uint32_t usize; };
static std::vector<uint8_t> makeZip(const std::vector<ZE>& es){
    std::vector<uint8_t> v,cd; std::vector<uint32_t> offs;
    for(auto& e:es){ offs.push_back((uint32_t)v.size()); le32(v,0x04034b50); le16(v,20); le16(v,0); le16(v,e.method); le16(v,0); le16(v,0); le32(v,e.crc); le32(v,(uint32_t)e.data.size()); le32(v,e.usize); le16(v,(uint16_t)e.name.size()); le16(v,0); v.insert(v.end(),e.name.begin(),e.name.end()); v.insert(v.end(),e.data.begin(),e.data.end()); }
    for(size_t i=0;i<es.size();i++){ auto& e=es[i]; le32(cd,0x02014b50); le16(cd,20); le16(cd,20); le16(cd,0); le16(cd,e.method); le16(cd,0); le16(cd,0); le32(cd,e.crc); le32(cd,(uint32_t)e.data.size()); le32(cd,e.usize); le16(cd,(uint16_t)e.name.size()); le16(cd,0); le16(cd,0); le16(cd,0); le16(cd,0); le32(cd,0); le32(cd,offs[i]); cd.insert(cd.end(),e.name.begin(),e.name.end()); }
    uint32_t cdo=(uint32_t)v.size(); v.insert(v.end(),cd.begin(),cd.end()); le32(v,0x06054b50); le16(v,0); le16(v,0); le16(v,(uint16_t)es.size()); le16(v,(uint16_t)es.size()); le32(v,(uint32_t)cd.size()); le32(v,cdo); le16(v,0); return v;
}
static ZE stored(const std::string& n,const std::string& s){ std::vector<uint8_t> d(s.begin(),s.end()); return {n,d,0,crc32Buf(d.data(),d.size()),(uint32_t)d.size()}; }
static void testZip(){
    static const uint8_t defl[]={0x0b,0xc8,0xcf,0x4b,0x4d,0xcf,0xa9,0x2c,0xc8,0x50,0x48,0x49,0x4d,0xcb,0x49,0x2c,0x49,0x55,0x28,0x49,0x2d,0x2e,0x51,0xc8,0xc9,0xcc,0x4b,0xd5,0x53,0x08,0x18,0x95,0x1c,0x95,0x1c,0x95,0x1c,0x95,0xa4,0xbd,0x24,0x00};
    std::string text; for(int i=0;i<40;i++) text+="Poneglyph deflate test line. ";
    std::vector<uint8_t> out; CHECK(inflateRaw(defl,sizeof defl,out,text.size(),text.size())); CHECK(std::string(out.begin(),out.end())==text);
    ZE d{"mods/pack/deflated.txt",std::vector<uint8_t>(defl,defl+sizeof defl),8,crc32Buf((const uint8_t*)text.data(),text.size()),(uint32_t)text.size()};
    wr(T/"zip/ok.zip",makeZip({stored("readme.txt","hello"),d,stored("sub/",""),stored("sub/a.asi","MZ")}));
    ZipArchive z; std::string err; CHECK(z.open(T/"zip/ok.zip",err)); CHECK(z.entries().size()==4);
    std::vector<std::string> w; CHECK(z.extractAll(T/"zip/out_ok",w,err)); CHECK(w.size()==3);
    std::string s; CHECK(readFileText(T/"zip/out_ok/mods/pack/deflated.txt",s)&&s==text);
    // traversal variants must be refused entirely, nothing extracted, nothing outside
    for(auto bad:{"../evil.txt","a/../../evil.txt","/abs/evil.txt","C:/evil.txt","..\\evil.txt","ok/..\\..\\evil.txt"}){
        wr(T/"zip/bad.zip",makeZip({stored("fine.txt","x"),stored(bad,"pwn")})); ZipArchive zb; CHECK(zb.open(T/"zip/bad.zip",err));
        fs::remove_all(T/"zip/out_bad"); bool ok=zb.extractAll(T/"zip/out_bad",w,err); CHECK(!ok); CHECK(!fs::exists(T/"zip/out_bad/fine.txt")); CHECK(!fs::exists(T/"zip/evil.txt")&&!fs::exists(T/"evil.txt")); }
    // crc mismatch
    auto bz=stored("x.txt","abc"); bz.crc^=1; wr(T/"zip/crc.zip",makeZip({bz})); ZipArchive zc; CHECK(zc.open(T/"zip/crc.zip",err)); fs::remove_all(T/"zip/out_crc"); CHECK(!zc.extractAll(T/"zip/out_crc",w,err)); CHECK(!fs::exists(T/"zip/out_crc/x.txt"));
    wr(T/"zip/notzip.zip",std::vector<uint8_t>(64,3)); ZipArchive zn; CHECK(!zn.open(T/"zip/notzip.zip",err));
    // bomb: claims huge ratio
    ZE bomb{"b.bin",std::vector<uint8_t>(10,0),8,0,0x7fffffff}; wr(T/"zip/bomb.zip",makeZip({bomb})); ZipArchive zbm; CHECK(zbm.open(T/"zip/bomb.zip",err)); CHECK(!zbm.extractAll(T/"zip/out_bomb",w,err));
}
static void testAddons(){
    fs::path A=T/"addons"; fs::remove_all(A);
    wr(A/"b_good.asi",makePe(0x14c,0x2102,{"Foo"})); wr(A/"a_good.dll",makePe(0x14c,0x2102,{"Bar"}));
    wr(A/"x64.asi",makePe(0x8664,0x2022,{"Baz"},0,true)); wr(A/"junk.asi",std::vector<uint8_t>(40,1));
    wr(A/"d3d9.dll",makePe(0x14c,0x2102,{"Direct3DCreate9"})); wr(A/"sub/b_good.asi",makePe(0x14c,0x2102,{"Foo2"})); // duplicate file name
    wr(A/"_Disabled/never.asi",makePe(0x14c,0x2102,{"N"}));
    wr(A/"off.asi",makePe(0x14c,0x2102,{"O"})); wr(A/"needs.asi",makePe(0x14c,0x2102,{"Q"}));
    { std::ofstream m(A/"needs.asi.pgmeta"); m<<"[Addon]\nName=Needs\nDependencies=missing.asi\nGames=ffxiii2\n"; }
    wr(A/"c1.asi",makePe(0x14c,0x2102,{"C1"})); wr(A/"c2.asi",makePe(0x14c,0x2102,{"C2"}));
    { std::ofstream m(A/"c2.asi.pgmeta"); m<<"[Addon]\nConflicts=c1.asi\nLoadOrder=2000\n"; }
    wr(A/"dep.asi",makePe(0x14c,0x2102,{"D"})); wr(A/"base.asi",makePe(0x14c,0x2102,{"B"}));
    { std::ofstream m(A/"dep.asi.pgmeta"); m<<"[Addon]\nDependencies=base.asi\nLoadOrder=1\n"; }
    wr(A/"lr.asi",makePe(0x14c,0x2102,{"L"})); { std::ofstream m(A/"lr.asi.pgmeta"); m<<"[Addon]\nGames=lr13\n"; }
    IniFile cfg; cfg.setBool("Addons","off.asi",false);
    ScanOptions so; so.game=GameId::FFXIII2; auto all=scanAddons(A,cfg,so);
    auto st=[&](const std::string& k){ for(auto& e:all) if(e.key==k) return e.status; return AddonStatus::InvalidPe; };
    CHECK(st("a_good.dll")==AddonStatus::Ready); CHECK(st("b_good.asi")==AddonStatus::Ready); CHECK(st("sub/b_good.asi")==AddonStatus::Duplicate);
    CHECK(st("x64.asi")==AddonStatus::WrongArch); CHECK(st("junk.asi")==AddonStatus::InvalidPe); CHECK(st("d3d9.dll")==AddonStatus::BlockedProxy);
    CHECK(st("off.asi")==AddonStatus::Disabled); CHECK(st("needs.asi")==AddonStatus::MissingDependency); CHECK(st("c1.asi")==AddonStatus::Ready); CHECK(st("c2.asi")==AddonStatus::Conflict);
    CHECK(st("lr.asi")==AddonStatus::WrongGame); bool seenDisabledDir=false; for(auto& e:all) if(e.key.find("_disabled")!=std::string::npos) seenDisabledDir=true; CHECK(!seenDisabledDir);
    auto ll=loadList(all); int pb=-1,pd=-1; for(size_t i=0;i<ll.size();i++){ if(ll[i]->key=="base.asi") pb=(int)i; if(ll[i]->key=="dep.asi") pd=(int)i; }
    CHECK(pb>=0&&pd>=0&&pb<pd);  // dependency loads first even though dep has lower LoadOrder
    // determinism: same result twice
    auto all2=scanAddons(A,cfg,so); auto l2=loadList(all2); CHECK(l2.size()==ll.size()); for(size_t i=0;i<ll.size();i++) CHECK(ll[i]->key==l2[i]->key);
    // cycle
    fs::remove_all(T/"cyc"); wr(T/"cyc/p.asi",makePe(0x14c,0x2102,{"P"})); wr(T/"cyc/q.asi",makePe(0x14c,0x2102,{"Q"}));
    { std::ofstream m(T/"cyc/p.asi.pgmeta"); m<<"[Addon]\nDependencies=q.asi\n"; } { std::ofstream m(T/"cyc/q.asi.pgmeta"); m<<"[Addon]\nDependencies=p.asi\n"; }
    auto cy=scanAddons(T/"cyc",IniFile(),so); CHECK(cy.size()==2&&cy[0].status==AddonStatus::DependencyCycle&&cy[1].status==AddonStatus::DependencyCycle);
    // crash recovery
    { std::ofstream m(T/"marker.txt"); m<<"c1.asi\n"; } std::string msg; auto k=applyCrashRecovery(T/"marker.txt",T/"rc.ini",msg); CHECK(k=="c1.asi"&&!fs::exists(T/"marker.txt")); IniFile rc; rc.load(T/"rc.ini"); CHECK(!rc.getBool("Addons","c1.asi",true));
    std::vector<AddonLoadRecord> recs={{"a","a.asi","Loaded","",1.5},{"b","b.asi","Failed","err 193",0}}; CHECK(writeLoadStatus(T/"st.ini",recs,"ffxiii2","1.0.0")); auto back=readLoadStatus(T/"st.ini"); CHECK(back.size()==2&&back[1].result=="Failed"&&back[0].ms>1.4);
}
static void testOps(){
    fs::path G=T/"game"; fs::remove_all(G); wr(G/"ffxiii2img.exe",makePe(0x14c,0x0102,{},0));
    auto pp=ProfilePaths::forGameDir(G); Logger lg;
    // engine candidate containing the marker
    auto eng=makePe(0x14c,0x2102,{"Direct3DCreate9"}); const char* id="PONEGLYPH_ENGINE_ID_7A3F"; eng.insert(eng.end(),id,id+strlen(id)+1); wr(T/"eng/d3d9.dll",eng);
    auto r=installEngine(pp,GameId::FFXIII2,T/"eng/d3d9.dll",{},lg); CHECK(r.ok); CHECK(classifyD3D9(G/"d3d9.dll")==ProxyKind::Poneglyph); CHECK(fs::exists(pp.config));
    // wrong game folder refused
    auto pp2=ProfilePaths::forGameDir(G); CHECK(!installEngine(pp2,GameId::LR,T/"eng/d3d9.dll",{},lg).ok);
    // foreign proxy: refused without consent, backed up and restorable with consent
    fs::remove(G/"d3d9.dll"); { std::vector<uint8_t> nf=makePe(0x14c,0x2102,{"Direct3DCreate9"}); const char* s="NovaFix"; nf.insert(nf.end(),s,s+8); wr(G/"d3d9.dll",nf); }
    std::string before; Sha256::ofFile(G/"d3d9.dll",before); CHECK(classifyD3D9(G/"d3d9.dll")==ProxyKind::NovaFix);
    auto r1=installEngine(pp,GameId::FFXIII2,T/"eng/d3d9.dll",{},lg); CHECK(!r1.ok); std::string chk; Sha256::ofFile(G/"d3d9.dll",chk); CHECK(chk==before);
    InstallOptions io; io.replaceForeignProxy=true; auto r2=installEngine(pp,GameId::FFXIII2,T/"eng/d3d9.dll",io,lg); CHECK(r2.ok); CHECK(classifyD3D9(G/"d3d9.dll")==ProxyKind::Poneglyph);
    auto u=uninstallEngine(pp,lg); CHECK(u.ok); std::string after; Sha256::ofFile(G/"d3d9.dll",after); CHECK(after==before);
    // uninstall refuses to delete a file that is not ours
    auto u2=uninstallEngine(pp,lg); CHECK(!u2.ok); CHECK(fs::exists(G/"d3d9.dll"));
    // non-x86 / non-proxy engine file rejected
    wr(T/"eng/bad.dll",makePe(0x8664,0x2022,{"Direct3DCreate9"},0,true)); fs::remove(G/"d3d9.dll"); CHECK(!installEngine(pp,GameId::FFXIII2,T/"eng/bad.dll",{},lg).ok);
    // addon package install / uninstall keeps user files
    ZE e1=stored("my.asi","MZxx"), e2=stored("cfg/my.ini","a=1"); wr(T/"pkg/MyMod.zip",makeZip({e1,e2}));
    auto ai=installAddonZip(pp,T/"pkg/MyMod.zip",lg); CHECK(ai.ok); CHECK(fs::exists(pp.addons/"MyMod/my.asi")); CHECK(installAddonZip(pp,T/"pkg/MyMod.zip",lg).ok==false);
    wr(pp.addons/"MyMod/user_notes.txt",std::vector<uint8_t>{'h','i'}); { std::ofstream f(pp.addons/"MyMod/cfg/my.ini",std::ios::trunc); f<<"a=2 user edited"; }
    auto au=uninstallAddonPackage(pp,"MyMod",lg); CHECK(au.ok); CHECK(!fs::exists(pp.addons/"MyMod/my.asi")); CHECK(fs::exists(pp.addons/"MyMod/user_notes.txt")); CHECK(fs::exists(pp.addons/"MyMod/cfg/my.ini"));
    CHECK(!uninstallAddonPackage(pp,"NotOurs",lg).ok);
    wr(T/"pkg/Evil.zip",makeZip({stored("../../escape.asi","x")})); CHECK(!installAddonZip(pp,T/"pkg/Evil.zip",lg).ok); CHECK(!fs::exists(G/"escape.asi")&&!fs::exists(T/"escape.asi"));
    CHECK(setAddonEnabled(pp,"MyMod/My.asi",false,lg).ok); IniFile c; c.load(pp.config); CHECK(!c.getBool("Addons","mymod/my.asi",true));
}
static void testGamesStats(){
    CHECK(identifyByExeName("FFXIIIIMG.EXE")==GameId::FFXIII); CHECK(identifyByExeName("ffxiii2img.exe")==GameId::FFXIII2); CHECK(identifyByExeName("lrff13.exe")==GameId::LR); CHECK(identifyByExeName("notepad.exe")==GameId::Unknown);
    fs::path R=T/"root"; fs::remove_all(R); wr(R/"alba_data/prog/win/bin/ffxiii2img.exe",makePe(0x14c,0x0102,{})); CHECK(!findGameExe(R,GameId::FFXIII2).empty()); CHECK(findGameExe(R,GameId::LR).empty());
    CHECK(verifyGameExe(R/"alba_data/prog/win/bin/ffxiii2img.exe",GameId::FFXIII2).ok); CHECK(!verifyGameExe(R/"alba_data/prog/win/bin/ffxiii2img.exe",GameId::FFXIII).ok);
    std::vector<double> f(1000,16.667); f[10]=80; f[500]=120; auto s=summarizeFrames(f); CHECK(s.frames==1000&&s.spikes==2&&s.maxMs==120&&s.p50>16.6&&s.p50<16.7&&s.fps>55&&s.fps<60); CHECK(s.low01Fps<10);
    CHECK(percentile({1,2,3,4,5,6,7,8,9,10},95)==10); CHECK(summarizeFrames({}).frames==0);
    Logger lg; fs::remove_all(T/"logs"); lg.open(T/"logs/t.log",400,2); for(int i=0;i<60;i++) lg.log(LogLevel::Info,"line %d padding padding padding",i); lg.close();
    CHECK(fs::exists(T/"logs/t.log")&&fs::exists(T/"logs/t.log.1")); CHECK(!fs::exists(T/"logs/t.log.3")); CHECK(fs::file_size(T/"logs/t.log")<=400);
    ReportInput ri; ri.gameName="FFXIII-2"; ri.managerVersion="1.0.0"; auto rep=buildReport(ri); CHECK(rep.find("No measured frame data")!=std::string::npos); CHECK(rep.find("RECOMMENDATIONS")!=std::string::npos);
}
static void testRealFixtures(){
    const char* e=std::getenv("PG_FIXTURES"); if(!e){ std::printf("  (PG_FIXTURES not set: skipping real-binary checks)\n"); return; }
    fs::path F=e; struct X{ const char* f; bool laa; } xs[]={{"ffxiiiimg.exe",false},{"ffxiii2img.exe",true},{"LRFF13.exe",true}};
    for(auto& x:xs){ auto p=inspectPe(F/x.f,{false,false,false}); CHECK(p.valid&&p.isX86()&&!p.isDll&&p.laa==x.laa); }
    auto nd=inspectPe(F/"d3d9.dll"); CHECK(nd.valid&&nd.isX86()&&nd.isDll&&nd.exports.size()>=15); bool c=false; for(auto& x:nd.exports) if(x.name=="Direct3DCreate9"&&x.ordinal==37) c=true; CHECK(c);
    std::string h; Sha256::ofFile(F/"d3d9.dll",h); CHECK(h=="1e2c96f2101f81bf7d6097e9725c508b127cd143aeb07decbcdda56291b6a458");
    CHECK(classifyD3D9(F/"d3d9.dll")==ProxyKind::NovaFix);
}
int main(){
    T=fs::temp_directory_path()/"pg_tests"; std::error_code ec; fs::remove_all(T,ec); fs::create_directories(T);
    struct{const char* n; void(*f)();} tests[]={{"sha256",testSha},{"ini+util",testIniUtil},{"pe",testPe},{"laa",testLaa},{"zip",testZip},{"addons",testAddons},{"ops",testOps},{"games+stats+log+report",testGamesStats},{"real-fixtures",testRealFixtures}};
    for(auto& t:tests){ int f0=g_fail; std::printf("[ RUN  ] %s\n",t.n); t.f(); std::printf("[ %s ] %s\n",g_fail==f0?" OK ":"FAIL",t.n); }
    std::printf("\n%d checks passed, %d failed\n",g_pass,g_fail); return g_fail?1:0;
}

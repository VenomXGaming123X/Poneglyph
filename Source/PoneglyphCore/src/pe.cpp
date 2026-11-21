#include "poneglyph/pe.hpp"
#include "poneglyph/sha256.hpp"
#include "poneglyph/util.hpp"
#include <fstream>
#include <cstring>
#include <algorithm>
namespace pg {
namespace {
struct Reader {
    std::ifstream f; uint64_t size=0;
    explicit Reader(const fs::path& p):f(p,std::ios::binary){ if(f){ f.seekg(0,std::ios::end); size=(uint64_t)f.tellg(); f.seekg(0); } }
    bool ok() const { return (bool)f; }
    bool read(uint64_t off,void* dst,size_t n){
        if(off>size||n>size-off) return false;
        f.clear(); f.seekg((std::streamoff)off); f.read((char*)dst,(std::streamsize)n); return (size_t)f.gcount()==n;
    }
    template<class T> bool rd(uint64_t off,T& v){ return read(off,&v,sizeof(T)); }
};
struct Section { char name[9]; uint32_t va, vsize, rawPtr, rawSize; };
bool rvaToOff(const std::vector<Section>& s,uint32_t rva,uint32_t headersSize,uint64_t& off){
    for(auto& x:s){ uint32_t span=std::max(x.vsize,x.rawSize); if(rva>=x.va && rva<x.va+span){ uint32_t d=rva-x.va; if(d>=x.rawSize) return false; off=(uint64_t)x.rawPtr+d; return true; } }
    if(rva<headersSize){ off=rva; return true; }
    return false;
}
bool readCStr(Reader& r,uint64_t off,std::string& out,size_t maxLen=512){
    out.clear(); for(size_t i=0;i<maxLen;i++){ char c; if(!r.rd(off+i,c)) return false; if(!c) return true; out+=c; } return false;
}
}
std::string PeInfo::archName() const {
    if(!valid) return "invalid";
    if(machine==0x14c) return "x86 (32-bit)";
    if(machine==0x8664) return "x64 (64-bit)";
    if(machine==0xAA64) return "ARM64";
    return "unknown";
}
uint32_t computePeChecksum(const fs::path& file,uint32_t csOff,uint64_t* sizeOut){
    std::ifstream f(file,std::ios::binary); if(!f) return 0;
    uint64_t sum=0, pos=0; std::vector<unsigned char> buf(1<<16); int carry=-1; // pending low byte
    unsigned pending=0; bool havePending=false;
    while(f){
        f.read((char*)buf.data(),(std::streamsize)buf.size()); size_t n=(size_t)f.gcount();
        for(size_t i=0;i<n;i++,pos++){
            unsigned b=buf[i];
            if(pos>=csOff && pos<csOff+4) b=0;     // checksum field counts as zero
            if(!havePending){ pending=b; havePending=true; }
            else { sum+= pending | (b<<8); sum=(sum&0xffff)+(sum>>16); havePending=false; }
        }
    }
    (void)carry;
    if(havePending){ sum+=pending; sum=(sum&0xffff)+(sum>>16); }
    sum=(sum&0xffff)+(sum>>16); sum&=0xffff;
    if(sizeOut) *sizeOut=pos;
    return (uint32_t)(sum+pos);
}
PeInfo inspectPe(const fs::path& file,const PeParseOptions& opt){
    PeInfo pi; Reader r(file);
    if(!r.ok()){ pi.error="Cannot open file."; return pi; }
    pi.fileSize=r.size;
    uint16_t mz=0; if(!r.rd(0,mz)||mz!=0x5A4D){ pi.error="Not a PE file (missing MZ signature)."; return pi; }
    uint32_t peOff=0; if(!r.rd(0x3c,peOff)||peOff<0x40||peOff>r.size-24){ pi.error="Invalid PE header offset."; return pi; }
    uint32_t sig=0; if(!r.rd(peOff,sig)||sig!=0x00004550){ pi.error="Missing PE signature."; return pi; }
    uint16_t machine,nsec,optSize,chars; uint32_t ts;
    if(!r.rd(peOff+4,machine)||!r.rd(peOff+6,nsec)||!r.rd(peOff+8,ts)||!r.rd(peOff+20,optSize)||!r.rd(peOff+22,chars)){ pi.error="Truncated COFF header."; return pi; }
    uint64_t opt0=peOff+24; uint16_t magic=0;
    if(!r.rd(opt0,magic)||(magic!=0x10b&&magic!=0x20b)){ pi.error="Unknown optional-header magic."; return pi; }
    if(optSize<96||nsec==0||nsec>96){ pi.error="Implausible optional header or section count."; return pi; }
    pi.machine=machine; pi.optMagic=magic; pi.timestamp=ts; pi.characteristics=chars;
    pi.charactOffset=peOff+22; pi.laa=(chars&0x0020)!=0; pi.isDll=(chars&0x2000)!=0; pi.isExe=(chars&0x0002)!=0&&!pi.isDll;
    pi.checksumOffset=(uint32_t)(opt0+64);
    uint32_t cs=0; r.rd(opt0+64,cs); pi.storedChecksum=cs;
    uint16_t sub=0; r.rd(opt0+68,sub); pi.subsystem=sub;
    uint32_t headersSize=0; r.rd(opt0+60,headersSize);
    uint64_t ddOff=opt0+(magic==0x10b?96:112);
    uint32_t nDirs=0; r.rd(ddOff-4,nDirs);
    // sections
    std::vector<Section> secs; uint64_t sp=opt0+optSize;
    for(unsigned i=0;i<nsec;i++){
        unsigned char b[40]; if(!r.read(sp+i*40,b,40)){ pi.error="Truncated section table."; return pi; }
        Section s{}; memcpy(s.name,b,8); s.name[8]=0;
        memcpy(&s.vsize,b+8,4); memcpy(&s.va,b+12,4); memcpy(&s.rawSize,b+16,4); memcpy(&s.rawPtr,b+20,4); secs.push_back(s);
    }
    pi.valid=true;
    auto dir=[&](unsigned idx,uint32_t& rva,uint32_t& sz){ rva=sz=0; if(idx>=nDirs) return; r.rd(ddOff+idx*8,rva); r.rd(ddOff+idx*8+4,sz); };
    if(opt.exports){
        uint32_t rva,sz; dir(0,rva,sz); uint64_t eo;
        if(rva && rvaToOff(secs,rva,headersSize,eo)){
            uint32_t base=0,nfun=0,nnames=0,aF=0,aN=0,aO=0;
            if(r.rd(eo+16,base)&&r.rd(eo+20,nfun)&&r.rd(eo+24,nnames)&&r.rd(eo+28,aF)&&r.rd(eo+32,aN)&&r.rd(eo+36,aO)&&nfun<=65536&&nnames<=65536){
                pi.exportBase=base; uint64_t fo,no,oo;
                if(rvaToOff(secs,aF,headersSize,fo)){
                    std::vector<uint32_t> fn(nfun); std::vector<uint32_t> nm(nnames); std::vector<uint16_t> od(nnames);
                    bool okf=nfun==0||r.read(fo,fn.data(),nfun*4);
                    bool okn=nnames==0|| (rvaToOff(secs,aN,headersSize,no)&&r.read(no,nm.data(),nnames*4));
                    bool oko=nnames==0|| (rvaToOff(secs,aO,headersSize,oo)&&r.read(oo,od.data(),nnames*2));
                    if(okf&&okn&&oko){
                        std::vector<bool> named(nfun,false);
                        for(uint32_t i=0;i<nnames;i++){
                            PeExport e; e.ordinal=base+od[i]; uint64_t so; if(od[i]<nfun) named[od[i]]=true;
                            if(rvaToOff(secs,nm[i],headersSize,so)) readCStr(r,so,e.name);
                            if(od[i]<nfun){ uint32_t frva=fn[od[i]]; if(frva>=rva&&frva<rva+sz){ e.forwarded=true; uint64_t fo2; if(rvaToOff(secs,frva,headersSize,fo2)) readCStr(r,fo2,e.forwardTarget); } }
                            pi.exports.push_back(e);
                        }
                        for(uint32_t i=0;i<nfun;i++) if(!named[i]&&fn[i]){ PeExport e; e.ordinal=base+i; pi.exports.push_back(e); }
                        std::sort(pi.exports.begin(),pi.exports.end(),[](const PeExport&a,const PeExport&b){return a.ordinal<b.ordinal;});
                    }
                }
            }
        }
    }
    if(opt.imports){
        uint32_t rva,sz; dir(1,rva,sz); uint64_t io;
        if(rva && rvaToOff(secs,rva,headersSize,io)){
            for(unsigned i=0;i<512;i++){
                uint32_t d[5]; if(!r.read(io+i*20,d,20)) break; if(!d[0]&&!d[3]&&!d[4]) break;
                uint64_t no; std::string n; if(rvaToOff(secs,d[3],headersSize,no)&&readCStr(r,no,n)) pi.importDlls.push_back(toLower(n));
            }
        }
    }
    if(opt.checksum) pi.calculatedChecksum=computePeChecksum(file,pi.charactOffset-22+24+64);
    return pi;
}

// ---------------------------------------------------------------- LAA patch
static bool copyFileExact(const fs::path& a,const fs::path& b){
    std::error_code ec; fs::create_directories(b.parent_path(),ec);
    std::ifstream i(a,std::ios::binary); std::ofstream o(b,std::ios::binary|std::ios::trunc); if(!i||!o) return false;
    char buf[1<<16]; while(i){ i.read(buf,sizeof buf); auto n=i.gcount(); if(n>0) o.write(buf,n); } o.flush(); return (bool)o;
}
LaaReport patchLaa(const fs::path& exe,const fs::path& backupDir,const std::vector<std::string>& expected){
    LaaReport rep;
    std::error_code ec;
    if(!fs::exists(exe,ec)){ rep.result=LaaResult::FileError; rep.message="File does not exist."; return rep; }
    if(!expected.empty()){
        bool match=false; auto fn=toLower(pathToUtf8(exe.filename()));
        for(auto& e:expected) if(toLower(e)==fn) match=true;
        if(!match){ rep.result=LaaResult::WrongExecutable; rep.message="This file name is not one of the expected game executables ("+join(expected,", ")+"). Nothing was changed."; return rep; }
    }
    PeInfo pi=inspectPe(exe,{false,false,true});
    if(!pi.valid){ rep.result=LaaResult::FileError; rep.message="Not a valid PE file: "+pi.error; return rep; }
    if(pi.is64()){ rep.result=LaaResult::NotPe32; rep.message="This is already a 64-bit (PE32+) program. The 4 GB flag does not apply."; return rep; }
    if(!pi.isX86()){ rep.result=LaaResult::NotX86; rep.message="Not an x86 PE32 executable (machine 0x"+std::to_string(pi.machine)+"). Nothing was changed."; return rep; }
    if(pi.isDll){ rep.result=LaaResult::IsDll; rep.message="This is a DLL, not a game executable. Nothing was changed."; return rep; }
    Sha256::ofFile(exe,rep.sha256Before);
    if(pi.laa){ rep.result=LaaResult::AlreadyLaa; rep.message="Already Large Address Aware. No change needed."; rep.sha256After=rep.sha256Before; return rep; }
    // 1. byte-for-byte backup, named with the hash so it can never be confused or overwritten
    fs::path bak=backupDir/(pathToUtf8(exe.filename())+"."+rep.sha256Before.substr(0,16)+".pgbak");
    if(!fs::exists(bak,ec)){
        if(!copyFileExact(exe,bak)){ rep.result=LaaResult::BackupFailed; rep.message="Could not create the backup copy. Nothing was changed."; return rep; }
    }
    std::string bh; if(!Sha256::ofFile(bak,bh)||bh!=rep.sha256Before){ rep.result=LaaResult::BackupFailed; rep.message="Backup verification failed (hash mismatch). Nothing was changed."; fs::remove(bak,ec); return rep; }
    rep.backupPath=bak;
    // 2. patch a temporary copy, never the only copy
    fs::path tmp=exe; tmp+=".pgpatch";
    if(!copyFileExact(exe,tmp)){ rep.result=LaaResult::FileError; rep.message="Could not create temporary file (is the folder read-only?). Nothing was changed."; return rep; }
    {
        std::fstream f(tmp,std::ios::in|std::ios::out|std::ios::binary);
        uint16_t nc=(uint16_t)(pi.characteristics|0x0020);
        f.seekp(pi.charactOffset); f.write((const char*)&nc,2); f.flush();
        if(!f){ fs::remove(tmp,ec); rep.result=LaaResult::FileError; rep.message="Write failed. Nothing was changed."; return rep; }
    }
    if(pi.storedChecksum!=0){
        uint32_t c=computePeChecksum(tmp,pi.checksumOffset);
        std::fstream f(tmp,std::ios::in|std::ios::out|std::ios::binary);
        f.seekp(pi.checksumOffset); f.write((const char*)&c,4); f.flush();
    }
    // 3. verify temp: LAA set, still valid x86 exe, ONLY allowed bytes differ
    PeInfo np=inspectPe(tmp,{false,false,false});
    bool ok=np.valid&&np.isX86()&&np.laa&&!np.isDll&&np.fileSize==pi.fileSize&&np.timestamp==pi.timestamp;
    if(ok){
        std::ifstream a(exe,std::ios::binary),b(tmp,std::ios::binary); uint64_t pos=0; char ba[1<<15],bb[1<<15];
        while(a&&b&&ok){
            a.read(ba,sizeof ba); b.read(bb,sizeof bb); auto n=a.gcount(); if(n!=b.gcount()){ ok=false; break; }
            for(std::streamsize i=0;i<n;i++,pos++) if(ba[i]!=bb[i]){
                bool inChar=pos>=pi.charactOffset&&pos<pi.charactOffset+2;
                bool inCs=pos>=pi.checksumOffset&&pos<pi.checksumOffset+4;
                if(!(inChar||inCs)){ ok=false; break; }
            }
        }
    }
    if(!ok){ fs::remove(tmp,ec); rep.result=LaaResult::VerifyFailed; rep.message="Verification of the patched copy failed. The original file was NOT touched."; return rep; }
    fs::rename(tmp,exe,ec);
    if(ec){ fs::remove(tmp,ec); rep.result=LaaResult::FileError; rep.message="Could not replace the executable (in use or no permission?). The original is unchanged."; return rep; }
    PeInfo fin=inspectPe(exe,{false,false,false});
    Sha256::ofFile(exe,rep.sha256After);
    if(!fin.valid||!fin.laa){ rep.result=LaaResult::VerifyFailed; rep.message="Post-patch check failed. Use Restore to return to the backup: "+pathToUtf8(bak); return rep; }
    rep.result=LaaResult::Patched; rep.message="Patched. Only the Large Address Aware flag"+std::string(pi.storedChecksum?" and the PE checksum were":" was")+" changed. Backup: "+pathToUtf8(bak);
    return rep;
}
LaaReport restoreFromBackup(const fs::path& bak,const fs::path& target){
    LaaReport r; std::error_code ec;
    if(!fs::exists(bak,ec)){ r.result=LaaResult::FileError; r.message="Backup file not found."; return r; }
    // name = <orig>.<16 hex>.pgbak
    std::string n=pathToUtf8(bak.filename()); auto ext=n.rfind(".pgbak");
    if(ext==std::string::npos||ext<17){ r.result=LaaResult::FileError; r.message="Not a Poneglyph backup file name."; return r; }
    std::string want=n.substr(ext-16,16); std::string h;
    if(!Sha256::ofFile(bak,h)||h.substr(0,16)!=want){ r.result=LaaResult::VerifyFailed; r.message="The backup is damaged (hash does not match its name). Restore refused."; return r; }
    fs::path tmp=target; tmp+=".pgrestore";
    if(!copyFileExact(bak,tmp)){ r.result=LaaResult::FileError; r.message="Could not write the temporary restore file."; return r; }
    fs::rename(tmp,target,ec);
    if(ec){ fs::remove(tmp,ec); r.result=LaaResult::FileError; r.message="Could not replace the target file (is the game running?)."; return r; }
    std::string h2; Sha256::ofFile(target,h2);
    if(h2!=h){ r.result=LaaResult::VerifyFailed; r.message="Restored file hash mismatch."; return r; }
    r.result=LaaResult::Patched; r.sha256After=h2; r.backupPath=bak; r.message="Original executable restored and verified."; return r;
}
}

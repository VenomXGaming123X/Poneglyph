#include "poneglyph/zip.hpp"
#include "poneglyph/util.hpp"
#include <fstream>
#include <cstring>
#include <algorithm>
namespace pg {
// ---------------------------------------------------------------- CRC32
uint32_t crc32Buf(const uint8_t* d,size_t n,uint32_t crc){
    static uint32_t T[256]; static bool init=false;
    if(!init){ for(uint32_t i=0;i<256;i++){ uint32_t c=i; for(int k=0;k<8;k++) c=c&1?0xEDB88320u^(c>>1):c>>1; T[i]=c; } init=true; }
    crc=~crc; for(size_t i=0;i<n;i++) crc=T[(crc^d[i])&255]^(crc>>8); return ~crc;
}
// ---------------------------------------------------------------- inflate (RFC 1951)
namespace {
struct Bits { const uint8_t* p; size_t n,pos=0; uint32_t buf=0; int cnt=0; bool err=false;
    int bit(){ if(cnt==0){ if(pos>=n){err=true;return 0;} buf=p[pos++]; cnt=8; } int b=buf&1; buf>>=1; cnt--; return b; }
    uint32_t bits(int k){ uint32_t v=0; for(int i=0;i<k;i++) v|=(uint32_t)bit()<<i; return v; } };
struct Huff { uint16_t count[16]; uint16_t sym[288]; };
void build(Huff& h,const uint8_t* len,int n){
    memset(h.count,0,sizeof h.count); for(int i=0;i<n;i++) h.count[len[i]]++; h.count[0]=0;
    uint16_t offs[16]; offs[1]=0; for(int i=1;i<15;i++) offs[i+1]=offs[i]+h.count[i];
    for(int i=0;i<n;i++) if(len[i]) h.sym[offs[len[i]]++]=(uint16_t)i;
}
int decode(Bits& b,const Huff& h){
    int code=0,first=0,index=0;
    for(int len=1;len<=15;len++){ code|=b.bit(); int c=h.count[len]; if(code-c<first) return h.sym[index+(code-first)]; index+=c; first+=c; first<<=1; code<<=1; if(b.err) return -1; }
    return -1;
}
const uint16_t LB[]={3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
const uint16_t LE[]={0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
const uint16_t DB[]={1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
const uint16_t DE[]={0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
}
bool inflateRaw(const uint8_t* in,size_t inLen,std::vector<uint8_t>& out,size_t expected,size_t maxOut){
    Bits b{in,inLen}; out.clear(); out.reserve(expected<maxOut?expected:maxOut);
    int last;
    do{
        last=b.bit(); int type=(int)b.bits(2); if(b.err) return false;
        if(type==0){
            b.cnt=0; if(b.pos+4>inLen) return false;
            unsigned len=in[b.pos]|(in[b.pos+1]<<8), nl=in[b.pos+2]|(in[b.pos+3]<<8); b.pos+=4;
            if((len^0xffff)!=nl||b.pos+len>inLen) return false;
            if(out.size()+len>maxOut) return false;
            out.insert(out.end(),in+b.pos,in+b.pos+len); b.pos+=len;
        } else if(type==1||type==2){
            Huff lit,dist; uint8_t lens[320];
            if(type==1){
                int i=0; for(;i<144;i++)lens[i]=8; for(;i<256;i++)lens[i]=9; for(;i<280;i++)lens[i]=7; for(;i<288;i++)lens[i]=8; build(lit,lens,288);
                for(i=0;i<30;i++)lens[i]=5; build(dist,lens,30);
            } else {
                int nlen=(int)b.bits(5)+257, ndist=(int)b.bits(5)+1, ncode=(int)b.bits(4)+4; if(nlen>286||ndist>30) return false;
                static const uint8_t ord[19]={16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
                uint8_t cl[19]={0}; for(int i=0;i<ncode;i++) cl[ord[i]]=(uint8_t)b.bits(3);
                Huff ch; build(ch,cl,19); int idx=0;
                while(idx<nlen+ndist){
                    int s=decode(b,ch); if(s<0) return false;
                    if(s<16) lens[idx++]=(uint8_t)s;
                    else { int prev=0,rep; if(s==16){ if(idx==0) return false; prev=lens[idx-1]; rep=3+(int)b.bits(2);} else if(s==17) rep=3+(int)b.bits(3); else rep=11+(int)b.bits(7);
                        if(idx+rep>nlen+ndist) return false; while(rep--) lens[idx++]=(uint8_t)prev; }
                }
                build(lit,lens,nlen); build(dist,lens+nlen,ndist);
            }
            for(;;){
                int s=decode(b,lit); if(s<0) return false;
                if(s<256){ if(out.size()>=maxOut) return false; out.push_back((uint8_t)s); }
                else if(s==256) break;
                else { s-=257; if(s>=29) return false; int len=LB[s]+(int)b.bits(LE[s]); int ds=decode(b,dist); if(ds<0||ds>=30) return false;
                    size_t d=DB[ds]+b.bits(DE[ds]); if(d>out.size()||out.size()+len>maxOut||b.err) return false;
                    for(int i=0;i<len;i++) out.push_back(out[out.size()-d]); }
            }
        } else return false;
        if(b.err) return false;
    } while(!last);
    return true;
}
// ---------------------------------------------------------------- zip container
static uint16_t u16(const uint8_t* p){ return (uint16_t)(p[0]|(p[1]<<8)); }
static uint32_t u32(const uint8_t* p){ return p[0]|(p[1]<<8)|(p[2]<<16)|((uint32_t)p[3]<<24); }
bool ZipArchive::open(const fs::path& p,std::string& err){
    path_=p; e_.clear(); std::ifstream f(p,std::ios::binary); if(!f){ err="Cannot open archive."; return false; }
    f.seekg(0,std::ios::end); uint64_t sz=(uint64_t)f.tellg(); if(sz<22){ err="File too small to be a ZIP."; return false; }
    uint64_t tail=std::min<uint64_t>(sz,65557); std::vector<uint8_t> t((size_t)tail); f.seekg((std::streamoff)(sz-tail)); f.read((char*)t.data(),(std::streamsize)tail);
    int64_t eo=-1; for(int64_t i=(int64_t)tail-22;i>=0;i--) if(u32(&t[(size_t)i])==0x06054b50){ eo=i; break; }
    if(eo<0){ err="Not a ZIP archive (end record not found)."; return false; }
    const uint8_t* e=&t[(size_t)eo]; uint32_t n=u16(e+10), cdSize=u32(e+12), cdOff=u32(e+16);
    if(n==0xffff||cdOff==0xffffffffu){ err="ZIP64 archives are not supported."; return false; }
    if((uint64_t)cdOff+cdSize>sz){ err="Corrupt central directory."; return false; }
    std::vector<uint8_t> cd(cdSize); f.seekg(cdOff); f.read((char*)cd.data(),cdSize); if((uint32_t)f.gcount()!=cdSize){ err="Central directory truncated."; return false; }
    size_t pos=0;
    for(uint32_t i=0;i<n;i++){
        if(pos+46>cd.size()||u32(&cd[pos])!=0x02014b50){ err="Corrupt central directory entry."; return false; }
        ZipEntry z; uint16_t flags=u16(&cd[pos+8]); z.method=u16(&cd[pos+10]); z.crc=u32(&cd[pos+16]); z.csize=u32(&cd[pos+20]); z.usize=u32(&cd[pos+24]);
        uint16_t nl=u16(&cd[pos+28]),xl=u16(&cd[pos+30]),cl=u16(&cd[pos+32]); z.localOffset=u32(&cd[pos+42]); uint32_t ext=u32(&cd[pos+38]);
        if(pos+46+nl+xl+cl>cd.size()){ err="Corrupt entry lengths."; return false; }
        z.name.assign((const char*)&cd[pos+46],nl); z.encrypted=(flags&1)!=0;
        std::string nm=z.name; std::replace(nm.begin(),nm.end(),'\\','/'); z.isDir=!nm.empty()&&nm.back()=='/';
        std::string chk=z.isDir?nm.substr(0,nm.size()-1):nm;
        if(z.name.find('\0')!=std::string::npos){ z.safe=false; z.unsafeReason="NUL in name"; }
        else if(!isSafeRelativePath(chk)){ z.safe=false; z.unsafeReason="unsafe path (traversal, absolute, reserved or invalid characters)"; }
        else if(((ext>>16)&0xF000)==0xA000){ z.safe=false; z.unsafeReason="symbolic link entry"; }
        if(z.encrypted){ z.safe=false; z.unsafeReason="encrypted entry"; }
        e_.push_back(z); pos+=46+nl+xl+cl;
    }
    return true;
}
bool ZipArchive::readEntry(const ZipEntry& z,std::vector<uint8_t>& out,std::string& err){
    std::ifstream f(path_,std::ios::binary); if(!f){ err="Cannot reopen archive."; return false; }
    uint8_t lh[30]; f.seekg((std::streamoff)z.localOffset); f.read((char*)lh,30);
    if(f.gcount()!=30||u32(lh)!=0x04034b50){ err="Bad local header for "+z.name; return false; }
    uint64_t dataOff=z.localOffset+30+u16(lh+26)+u16(lh+28);
    std::vector<uint8_t> c((size_t)z.csize); f.seekg((std::streamoff)dataOff); f.read((char*)c.data(),(std::streamsize)z.csize);
    if((uint64_t)f.gcount()!=z.csize){ err="Truncated data for "+z.name; return false; }
    if(z.method==0){ out=std::move(c); }
    else if(z.method==8){ if(!inflateRaw(c.data(),c.size(),out,(size_t)z.usize,(size_t)z.usize)){ err="Decompression failed for "+z.name; return false; } }
    else { err="Unsupported compression method for "+z.name; return false; }
    if(out.size()!=z.usize){ err="Size mismatch for "+z.name; return false; }
    if(crc32Buf(out.data(),out.size())!=z.crc){ err="CRC mismatch for "+z.name; return false; }
    return true;
}
bool ZipArchive::extractAll(const fs::path& root,std::vector<std::string>& written,std::string& err,bool skipUnsafe,const ZipLimits& lim){
    written.clear();
    if(e_.size()>lim.maxEntries){ err="Archive has too many entries."; return false; }
    uint64_t total=0;
    for(auto& z:e_){
        if(!z.safe){ if(!skipUnsafe){ err="Refused: '"+z.name+"' is an "+z.unsafeReason+". Nothing was extracted."; return false; } continue; }
        if(z.usize>lim.maxFileBytes){ err="Refused: '"+z.name+"' is too large."; return false; }
        if(z.csize>0 && z.usize/z.csize>lim.maxRatio && z.usize>1024*1024){ err="Refused: '"+z.name+"' has a suspicious compression ratio (possible zip bomb)."; return false; }
        total+=z.usize; if(total>lim.maxTotalBytes){ err="Refused: archive expands to more than the allowed size."; return false; }
    }
    // phase 1: decode everything into memory-verified temp list is too big for large mods, so verify CRC while extracting
    // to a staging folder first, then move. This guarantees all-or-nothing.
    std::error_code ec; fs::path stage=root; stage+=".pgstage"; fs::remove_all(stage,ec); fs::create_directories(stage,ec);
    for(auto& z:e_){
        if(!z.safe) continue;
        std::string nm=z.name; std::replace(nm.begin(),nm.end(),'\\','/');
        fs::path dest=stage/pathFromUtf8(nm);
        if(!isInside(stage,dest)){ fs::remove_all(stage,ec); err="Refused: path escapes the install folder: "+z.name; return false; }
        if(z.isDir){ fs::create_directories(dest,ec); continue; }
        std::vector<uint8_t> data; if(!readEntry(z,data,err)){ fs::remove_all(stage,ec); return false; }
        fs::create_directories(dest.parent_path(),ec);
        std::ofstream o(dest,std::ios::binary|std::ios::trunc); o.write((const char*)data.data(),(std::streamsize)data.size()); o.flush();
        if(!o){ fs::remove_all(stage,ec); err="Write failed for "+z.name; return false; }
    }
    fs::create_directories(root,ec);
    for(auto& z:e_){
        if(!z.safe||z.isDir) continue;
        std::string nm=z.name; std::replace(nm.begin(),nm.end(),'\\','/');
        fs::path src=stage/pathFromUtf8(nm), dst=root/pathFromUtf8(nm);
        if(!isInside(root,dst)){ fs::remove_all(stage,ec); err="Refused: path escapes the install folder: "+z.name; return false; }
        fs::create_directories(dst.parent_path(),ec); fs::remove(dst,ec); fs::rename(src,dst,ec);
        if(ec){ fs::copy_file(src,dst,fs::copy_options::overwrite_existing,ec); if(ec){ err="Could not place "+z.name; fs::remove_all(stage,ec); return false; } }
        written.push_back(nm);
    }
    fs::remove_all(stage,ec); return true;
}
}

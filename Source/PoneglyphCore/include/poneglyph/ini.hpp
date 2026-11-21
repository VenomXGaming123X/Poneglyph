// Poneglyph - tiny INI reader/writer. Case-insensitive section/key lookup,
// order preserving, UTF-8, ';' and '#' comments.
#pragma once
#include <string>
#include <vector>
#include <filesystem>
namespace pg {
class IniFile {
public:
    bool load(const std::filesystem::path& p);
    bool loadFromString(const std::string& text);
    bool save(const std::filesystem::path& p) const;     // atomic
    std::string toString() const;
    std::string get(const std::string& sec,const std::string& key,const std::string& def="") const;
    bool        has(const std::string& sec,const std::string& key) const;
    int         getInt(const std::string& sec,const std::string& key,int def) const;
    bool        getBool(const std::string& sec,const std::string& key,bool def) const;
    void        set(const std::string& sec,const std::string& key,const std::string& val);
    void        setInt(const std::string& sec,const std::string& key,int v);
    void        setBool(const std::string& sec,const std::string& key,bool v);
    bool        remove(const std::string& sec,const std::string& key);
    std::vector<std::string> sections() const;
    std::vector<std::pair<std::string,std::string>> entries(const std::string& sec) const;
    void        clear(){ secs_.clear(); }
private:
    struct Sec{ std::string name; std::vector<std::pair<std::string,std::string>> kv; };
    std::vector<Sec> secs_;
    Sec* find(const std::string& n);
    const Sec* find(const std::string& n) const;
};
}

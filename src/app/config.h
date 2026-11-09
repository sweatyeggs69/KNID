#pragma once
// Persistent user settings: a flat key=value file (settings.ini) in the per-user pref folder
// (SDL_GetPrefPath("KNID", "KNID")). Values are saved as soon as they change.
//
// Key namespaces:
//   rom.*      ROM selection (rom.path, rom.sha1)
//   display.*  video options (owned by the options panel)
//   input.*    controls / remapping (owned by the options panel)
//   audio.*, save.*, bios.*  reserved

#include <filesystem>
#include <map>
#include <string>

namespace knid {

class Config {
public:
    // Loads <pref dir>/settings.ini. If pref_dir is empty, SDL_GetPrefPath("KNID","KNID") is used.
    explicit Config(std::filesystem::path pref_dir = {});

    const std::filesystem::path& pref_dir() const { return pref_dir_; }
    const std::filesystem::path& file() const { return file_; }

    bool has(const std::string& key) const { return values_.count(key) != 0; }
    std::string get_string(const std::string& key, const std::string& def = {}) const;
    int get_int(const std::string& key, int def = 0) const;
    bool get_bool(const std::string& key, bool def = false) const;
    float get_float(const std::string& key, float def = 0.0f) const;

    // Setters write the file immediately if the value changed.
    void set_string(const std::string& key, const std::string& value);
    void set_int(const std::string& key, int value);
    void set_bool(const std::string& key, bool value);
    void set_float(const std::string& key, float value);
    void erase(const std::string& key);

    const std::map<std::string, std::string>& values() const { return values_; }
    bool save() const;
    bool load();

private:
    std::filesystem::path pref_dir_, file_;
    std::map<std::string, std::string> values_;
};

}  // namespace knid

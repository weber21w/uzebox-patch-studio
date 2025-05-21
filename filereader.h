#pragma once

#include <wx/string.h>
#include <wx/vector.h>
#include <map>
#include <regex>
#include "waves.h"    // defines WaveTable, WAVE_SIZE, NUM_WAVES

class FileReader {
public:
    /// Parse a patches.inc/.cpp and extract patches & structs
    static bool read_patches_and_structs(const wxString &fn,
        std::multimap<wxString, wxVector<long>> &patches,
        std::multimap<wxString, wxVector<wxString>> &structs);

    /// Read up to `maxWaves` tables (each WAVE_SIZE bytes) from a `.inc`-style
    /// wavetable file into `waves[0..]`.  Returns how many full tables loaded.
    static size_t read_waves(const wxString &fn,
                             WaveTable waves[],
                             size_t maxWaves = MAX_WAVES);
    /// Write out `numWaves` tables (each WAVE_SIZE bytes) from `waves[]`
    /// into a `.inc`-style file at `fn`.  Returns true on success.
    static bool write_waves(const wxString &fn,
                            WaveTable waves[],
                            size_t numWaves);
private:
    static long string_to_long(const wxString &str);
    static bool read_patch_vals(const wxString &str, wxVector<long> &vals);
    static bool read_struct_vals(const wxString &str, wxVector<wxString> &vals);
    static std::string clean_code(const std::string &code);
    static bool read_patches(const std::string &clean_src,
        std::multimap<wxString, wxVector<long>> &data);
    static bool read_structs(const std::string &clean_src,
        std::multimap<wxString, wxVector<wxString>> &data);

    static const std::map<wxString, long> defines;
    static const std::regex byte_line;
    static const std::regex multiline_comments;
    static const std::regex singleline_comments;
    static const std::regex white_space;
    static const std::regex extra_space;
    static const std::regex patch_declaration;
    static const std::regex struct_declaration;
};

#include <wx/vector.h>
#include <regex>
#include <wx/string.h>
#include <algorithm>    // for std::transform
#include <cctype>       // for std::tolower
#include <iterator>
#include <fstream>
#include <iomanip>
#include <streambuf>
#include <sstream>
#include <map>
#include "filereader.h"


const std::map<wxString, long> FileReader::defines = {
  {"WAVE_SINE", 0},
  {"WAVE_SAWTOOTH", 1},
  {"WAVE_TRIANGLE", 2},
  {"WAVE_SQUARE_25", 3},
  {"WAVE_SQUARE_50", 4},
  {"WAVE_SQUARE_75", 5},
  {"WAVE_FUZZY_SINE1", 6},
  {"WAVE_FUZZY_SINE2", 7},
  {"WAVE_FUZZY_SINE3", 8},
  {"WAVE_FILTERED_SQUARE", 9},

  {"WSIN", 0},
  {"WSAW", 1},
  {"WTRI", 2},
  {"WS25", 3},
  {"WS50", 4},
  {"WS75", 5},
  {"WFS1", 6},
  {"WFS2", 7},
  {"WFS3", 8},
  {"WFSQ", 9},

  {"PC_ENV_SPEED", 0},
  {"PC_NOISE_PARAMS", 1},
  {"PC_WAVE", 2},
  {"PC_NOTE_UP", 3},
  {"PC_NOTE_DOWN", 4},
  {"PC_NOTE_CUT", 5},
  {"PC_NOTE_HOLD", 6},
  {"PC_ENV_VOL", 7},
  {"PC_PITCH", 8},
  {"PC_TREMOLO_LEVEL", 9},
  {"PC_TREMOLO_RATE", 10},
  {"PC_SLIDE", 11},
  {"PC_SLIDE_SPEED", 12},
  {"PC_LOOP_START", 13},
  {"PC_LOOP_END", 14},
  {"PATCH_END", 15},
};

const std::regex FileReader::byte_line(
  R"(^\s*\.byte\s*(.*))",
  std::regex_constants::ECMAScript | std::regex_constants::icase
);
const std::regex FileReader::multiline_comments("/\\*(.|[\r\n])*?\\*/");
const std::regex FileReader::singleline_comments("//.*");
const std::regex FileReader::white_space("[\t\n\r]");
const std::regex FileReader::extra_space(" +");
const std::regex FileReader::patch_declaration(
    "const char ([a-zA-Z_][a-zA-Z_\\d]*)\\[\\] PROGMEM ?= ?");
const std::regex FileReader::struct_declaration(
    "const struct PatchStruct ([a-zA-Z_][a-zA-Z_\\d]*)\\[\\] PROGMEM ?= ?");
static const std::regex music_decl(
    R"(const\s+unsigned\s+char\s+([A-Za-z_][A-Za-z0-9_]*)\s*\[\]\s*=\s*\{)");

long FileReader::string_to_long(const wxString &str) {
  if (defines.find(str) != defines.end())
    return defines.find(str)->second;
  return strtol(str.c_str(), NULL, 0);
}

bool FileReader::read_patch_vals(const wxString &str, wxVector<long> &vals) {
  int depth = 0;
  std::string vstr;
  for (auto c : str) {
    if (c == '{') {
      depth++;
    }
    else if (c == '}') {
      if (!depth) {
        return false;
      }
      else if (!(--depth)) {
        break;
      }
    }
    else if (c != ' ' && depth == 1) {
      vstr += c;
    }
  }

  std::stringstream ss(vstr);
  std::string item;
  vals.clear();
  while (std::getline(ss, item, ',')) {
    if (!item.empty()) {
      vals.push_back(string_to_long(item));
    }
  }

  return !vals.empty();
}

bool FileReader::read_struct_vals(const wxString &str,
    wxVector<wxString> &vals) {
  int depth = 0;
  std::string vstr;
  for (auto c : str) {
    if (c == '{') {
      depth++;
    }
    else if (c == '}') {
      if (!depth) {
        return false;
      }
      else if (!(--depth)) {
        break;
      }
      else {
        vstr += ',';
      }
    }
    else if (c != ' ' && depth == 2) {
      vstr += c;
    }
  }

  std::stringstream ss(vstr);
  std::string item;
  vals.clear();
  while (std::getline(ss, item, ',')) {
    vals.push_back(item);
  }

  return !vals.empty();
}

std::string FileReader::clean_code(const std::string &code) {
  std::string clean_code, t;

  /* Remove comments and unecessary white space */
  std::regex_replace(std::back_inserter(clean_code), code.begin(), code.end(),
      multiline_comments, "");
  clean_code.swap(t);
  clean_code.clear();
  std::regex_replace(std::back_inserter(clean_code), t.begin(), t.end(),
      singleline_comments, "");
  clean_code.swap(t);
  clean_code.clear();
  std::regex_replace(std::back_inserter(clean_code), t.begin(), t.end(),
      white_space, " ");
  clean_code.swap(t);
  clean_code.clear();
  std::regex_replace(std::back_inserter(clean_code), t.begin(), t.end(),
      extra_space, " ");

  return clean_code;
}

bool FileReader::read_patches(const std::string &clean_src,
    std::multimap<wxString, wxVector<long>> &data) {
  std::smatch match;
  auto search_start = clean_src.cbegin();

  data.clear();

  while (std::regex_search(search_start, clean_src.cend(),
        match, patch_declaration)) {
    search_start += match.position() + match.length();

    wxVector<long> vals;
    std::string varea(search_start, clean_src.cend());
    if (!read_patch_vals(varea, vals))
      return false;
    data.emplace(match[1].str(), vals);
  }

  return true;
}

bool FileReader::read_structs(const std::string &clean_src,
    std::multimap<wxString, wxVector<wxString>> &data) {
  std::smatch match;
  auto search_start = clean_src.cbegin();

  data.clear();

  while (std::regex_search(search_start, clean_src.cend(),
        match, struct_declaration)) {
    search_start += match.position() + match.length();

    wxVector<wxString> vals;
    std::string varea(search_start, clean_src.cend());
    if (!read_struct_vals(varea, vals))
      return false;
    data.emplace(match[1].str(), vals);
  }

  return true;
}

bool FileReader::read_patches_and_structs(const wxString &fn,
    std::multimap<wxString, wxVector<long>> &patches,
    std::multimap<wxString, wxVector<wxString>> &structs) {
  std::ifstream f(fn);
  if (!f.is_open())
    return false;
  std::string src((std::istreambuf_iterator<char>(f)),
    std::istreambuf_iterator<char>());
  std::string clean_src = clean_code(src);

  return read_patches(clean_src, patches) && read_structs(clean_src, structs);
}


size_t FileReader::read_waves(const wxString &fn,
                              WaveTable waves[],
                              size_t maxWaves)
{
  // 1) Slurp entire file into a string
  std::ifstream in(fn.mb_str(), std::ios::in | std::ios::binary);
  if (!in.is_open()) return 0;
  std::string src((std::istreambuf_iterator<char>(in)),
                   std::istreambuf_iterator<char>());
  in.close();

  // 2) Strip out all C-style /* … */ blocks
  src = std::regex_replace(src, multiline_comments, "");

  // 3) Split into lines (handling both \r\n and \n)
  std::vector<std::string> lines;
  {
    std::istringstream iss(src);
    std::string line;
    while (std::getline(iss, line)) {
      if (!line.empty() && line.back() == '\r')
        line.pop_back();
      lines.push_back(std::move(line));
    }
  }

  size_t waveIdx = 0;
  std::vector<uint8_t> buffer;
  buffer.reserve(WAVE_SIZE);

  // 4) Scan each line for “.byte” directives
  for (const auto &raw : lines) {
    if (waveIdx >= maxWaves) break;

    // 4a) Chop off any trailing ';' comment
    auto semi = raw.find(';');
    std::string line = (semi == std::string::npos
                        ? raw
                        : raw.substr(0, semi));

    // 4b) Trim leading whitespace
    auto first = line.find_first_not_of(" \t");
    if (first == std::string::npos) continue;
    line.erase(0, first);

    // 4c) Case-insensitive check for ".byte"
    if (line.size() < 5) continue;
    std::string prefix = line.substr(0,5);
    std::transform(prefix.begin(), prefix.end(), prefix.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    if (prefix != ".byte") continue;

    // 4d) Parse the comma-separated literals
    std::string nums = line.substr(5);
    std::istringstream ss(nums);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
      // trim whitespace around tok
      auto a = tok.find_first_not_of(" \t");
      auto b = tok.find_last_not_of(" \t");
      if (a == std::string::npos) continue;
      tok = tok.substr(a, b - a + 1);

      // interpret as signed 8-bit, then shift to unsigned 0–255
      long  rawVal      = std::stol(tok, nullptr, 0);
      int8_t signedSample = static_cast<int8_t>(rawVal & 0xFF);
      uint8_t u           = static_cast<uint8_t>(int(signedSample) + 128);

      buffer.push_back(u);

      // once we have WAVE_SIZE samples, commit one wave
      if (buffer.size() == WAVE_SIZE) {
        std::copy(buffer.begin(), buffer.end(),
                  waves[waveIdx].begin());
        buffer.clear();
        ++waveIdx;
        if (waveIdx >= maxWaves) break;
      }
    }
  }

  return waveIdx;
}

bool FileReader::write_waves(const wxString &fn,
                             WaveTable waves[],
                             size_t numWaves)
{
  std::ofstream out(fn.mb_str(), std::ios::out | std::ios::binary);
  if (!out.is_open()) return false;

  // Header comment
  out << "/* Created by Uzebox Patch Studio: wavetable export */\n\n";

  for (size_t w = 0; w < numWaves; ++w) {
    // Comment for each wave
    out << "; Wave #" << w << "\n";

    // We break each wave into lines of 16 bytes
    for (size_t i = 0; i < WAVE_SIZE; i += 16) {
      out << "  .byte ";

      for (size_t j = 0; j < 16 && (i + j) < WAVE_SIZE; ++j) {
        // Invert your loader’s signed→unsigned shift:
        // waves[w][i+j] is 0…255, but the loader expects two’s-complement bytes.
        uint8_t u      = waves[w][i + j];
        int8_t  s      = static_cast<int8_t>(int(u) - 128);
        uint8_t rawVal = static_cast<uint8_t>(s);

        // Emit as hex literal 0xNN
        out << "0x"
            << std::uppercase
            << std::hex
            << std::setw(2)
            << std::setfill('0')
            << int(rawVal);

        // comma if not last in line
        if (j + 1 < 16 && (i + j + 1) < WAVE_SIZE)
          out << ",";
      }

      out << "\n";
    }

    out << "\n";
  }

  out.close();
  return true;
}

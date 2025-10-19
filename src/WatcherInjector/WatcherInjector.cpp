// WatcherInjector – DLL-Worker

#include <windows.h>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <cwctype>
#include <algorithm>
#include <system_error>

namespace fs = std::filesystem;

static volatile bool g_running = false;

// --- Hilfsfunktionen ---

// UTF-16 -> UTF-8
static std::string narrow(const std::wstring& ws) {
    if (ws.empty()) return std::string();
    int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), nullptr, 0, nullptr, nullptr);
    std::string out;
    out.resize((size_t)len);
    if (len > 0) {
        WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), out.data(), len, nullptr, nullptr);
    }
    return out;
}

static std::wstring wtrim(const std::wstring& s) {
    size_t a = 0, b = s.size();
    while (a < b && iswspace(s[a])) ++a;
    while (b > a && iswspace(s[b - 1])) --b;
    return s.substr(a, b - a);
}

static std::wstring wlower(std::wstring s) {
    for (auto& c : s) c = (wchar_t)towlower(c);
    return s;
}

static void ensure_dir(const fs::path& p) {
    std::error_code ec;
    fs::create_directories(p, ec);
}

// --- Konfiguration / INI ---

struct Field {
    std::wstring label;
    size_t offset = 0;
    int size = 1;
    uint32_t mask = 0;
    int value_offset = 0;
};

struct Cfg {
    std::wstring base;   // BASE Pfad
    std::wstring rom;    // ROM Name
    std::wstring nvram;  // NVRAM-Datei
    std::vector<Field> fields; // Felder für cp/pc/cb/bp etc.
};

// Einfaches Lesen einer Textdatei in wstring
static bool load_text(const fs::path& file, std::wstring& out) {
    std::wifstream f(file);
    if (!f.is_open()) return false;
    f.imbue(std::locale(std::locale::classic(), new std::codecvt_utf8_utf16<wchar_t>));
    std::wstringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

// Parse "field=" Zeile in unser Field-Format
// Unterstützt:
// - field=label=current_player,offset=...,size=...,mask=...,value_offset=...
// - field=current_player,offset=...,size=...,mask=...,value_offset=... (legacy)
static bool parse_field_line(const std::wstring& line, Field& out) {
    // Erwartet: "field=...."
    auto pos = line.find(L'=');
    if (pos == std::wstring::npos) return false;
    std::wstring rhs = wtrim(line.substr(pos + 1));

    // Token per Komma splitten
    std::vector<std::wstring> tokens;
    {
        std::wstring cur;
        std::wstringstream ss(rhs);
        while (std::getline(ss, cur, L',')) {
            tokens.push_back(wtrim(cur));
        }
    }

    std::wstring label;
    size_t offset = 0;
    int size = 1;
    uint32_t mask = 0;
    int value_offset = 0;

    // Zwei Formate:
    // 1) label=... als Key-Value
    // 2) Erster Token ist direkt der Label-Name (legacy)
    bool have_kv_label = false;
    for (const auto& t : tokens) {
        auto eq = t.find(L'=');
        if (eq != std::wstring::npos) {
            std::wstring k = wlower(wtrim(t.substr(0, eq)));
            std::wstring v = wtrim(t.substr(eq + 1));
            if (k == L"label") {
                label = v;
                have_kv_label = true;
            } else if (k == L"offset") {
                offset = (size_t)_wtoi(v.c_str());
            } else if (k == L"size") {
                size = _wtoi(v.c_str());
                if (size <= 0) size = 1;
            } else if (k == L"mask") {
                mask = (uint32_t)_wtoi(v.c_str());
            } else if (k == L"value_offset") {
                value_offset = _wtoi(v.c_str());
            } else if (k.empty() && !v.empty()) {
                // ignorieren
            }
        }
    }

    if (!have_kv_label && !tokens.empty()) {
        // Legacy: erster Token ist Label pur
        auto eq0 = tokens[0].find(L'=');
        if (eq0 == std::wstring::npos) {
            label = tokens[0];
        }
    }

    if (label.empty()) return false;
    out.label = wtrim(label);
    out.offset = offset;
    out.size = size;
    out.mask = mask;
    out.value_offset = value_offset;
    return true;
}

// INI laden. Sucht standardmäßig:
// - BASE\watchtower_hook.ini (bevorzugt)
// - BASE\watcher_hook.ini (legacy)
// Fallback: gleiche Dateien neben der DLL (BASE\bin)
static bool load_ini(Cfg& cfg) {
    // DLL-Pfad ermitteln
    wchar_t modPathW[MAX_PATH];
    HMODULE hMod = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCWSTR)&load_ini, &hMod)) {
        hMod = GetModuleHandleW(nullptr);
    }
    GetModuleFileNameW(hMod, modPathW, MAX_PATH);
    fs::path modPath(modPathW);
    fs::path binDir = modPath.parent_path(); // ...\BASE\bin
    fs::path baseDir = binDir.parent_path(); // ...\BASE

    // Kandidatenliste
    std::vector<fs::path> candidates = {
        baseDir / L"watchtower_hook.ini",
        baseDir / L"watcher_hook.ini",
        binDir / L"watchtower_hook.ini",
        binDir / L"watcher_hook.ini",
    };

    fs::path iniPath;
    for (const auto& p : candidates) {
        if (fs::exists(p)) { iniPath = p; break; }
    }
    if (iniPath.empty()) return false;

    std::wstring text;
    if (!load_text(iniPath, text)) return false;

    std::wstringstream ss(text);
    std::wstring line;
    std::vector<Field> fields;
    std::wstring base, rom, nv;

    while (std::getline(ss, line)) {
        line = wtrim(line);
        if (line.empty()) continue;
        if (line[0] == L'#' || line[0] == L';') continue;

        auto eq = line.find(L'=');
        if (eq == std::wstring::npos) continue;

        std::wstring key = wlower(wtrim(line.substr(0, eq)));
        std::wstring val = wtrim(line.substr(eq + 1));

        if (key == L"base") {
            base = val;
        } else if (key == L"rom") {
            rom = val;
        } else if (key == L"nvram") {
            nv = val;
        } else if (key == L"field") {
            Field f{};
            if (parse_field_line(line, f)) {
                fields.push_back(f);
            }
        }
    }

    if (base.empty() || rom.empty() || nv.empty() || fields.empty()) {
        // Minimalanforderungen nicht erfüllt
        return false;
    }

    cfg.base = base;
    cfg.rom = rom;
    cfg.nvram = nv;
    cfg.fields = std::move(fields);
    return true;
}

// Datei in Pufferspeicher lesen
static bool read_bytes(const std::wstring& path, std::vector<uint8_t>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    f.seekg(0, std::ios::end);
    std::streamoff sz = f.tellg();
    if (sz <= 0) return false;
    f.seekg(0, std::ios::beg);
    out.resize((size_t)sz);
    f.read(reinterpret_cast<char*>(out.data()), sz);
    return true;
}

// Wert-Extraktion (Big-Endian-Aufbau wie im Original-Snippet),
// inkl. mask/value_offset
static int extract_value(const std::vector<uint8_t>& buf, const Field& f) {
    if (f.offset + (size_t)f.size > buf.size()) return -1;
    uint64_t v = 0;
    if (f.size == 1) {
        v = buf[f.offset];
    } else {
        for (int i = f.size - 1; i >= 0; --i)
            v = (v << 8) | buf[f.offset + (size_t)i];
    }
    if (f.mask) v &= f.mask;
    int vi = (int)v;
    vi += f.value_offset;
    return vi;
}

// --- Ausgabe: NUR noch live.session.json ---

// JSON ausschließlich in session_stats schreiben (kein Root-Mirror, neuer Name: live.session.json)
static void write_json(const std::wstring& base, const std::wstring

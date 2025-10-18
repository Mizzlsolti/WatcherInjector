// WatcherInjector DLL
// - INI: BASE\bin\watcher_hook.ini (gleicher Ordner wie die DLL)
// - Liest NVRAM und schreibt BASE\session_stats\live_control.json

#include <windows.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <fstream>
#include <sstream>
#include <codecvt>
#include <cwctype>
#include <filesystem>

extern "C" IMAGE_DOS_HEADER __ImageBase;
namespace fs = std::filesystem;

static std::atomic<bool> g_running{false};

struct Field {
    std::wstring label;
    size_t offset = 0;
    int size = 1;
    unsigned int mask = 0;
    int value_offset = 0;
};

struct Cfg {
    std::wstring base;
    std::wstring rom;
    std::wstring nvram;
    std::vector<Field> fields;
};

static inline std::wstring trim(const std::wstring& s) {
    size_t i = 0, j = s.size();
    while (i < j && iswspace(s[i])) ++i;
    while (j > i && iswspace(s[j - 1])) --j;
    return s.substr(i, j - i);
}
static std::string narrow(const std::wstring& w) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n > 0 ? n : 0, '\0');
    if (n > 0) WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

static bool find_ini_path(fs::path& outIni) {
    // INI liegt im selben Ordner wie die DLL: BASE\bin\watcher_hook.ini
    wchar_t modPathW[MAX_PATH]{0};
    GetModuleFileNameW((HMODULE)&__ImageBase, modPathW, MAX_PATH);
    fs::path mod(modPathW);
    fs::path binDir = mod.parent_path();
    fs::path ini = binDir / L"watcher_hook.ini";
    std::error_code ec;
    if (fs::exists(ini, ec)) {
        outIni = ini;
        return true;
    }
    return false;
}

static bool parse_field_line(const std::wstring& line, Field& out) {
    // Akzeptiert:
    // field=label=...,offset=...,size=...,mask=...,value_offset=...
    // sowie legacy: field=current_player,offset=...
    size_t eq = line.find(L'=');
    if (eq == std::wstring::npos) return false;
    std::wstring rest = line.substr(eq + 1);
    std::wstringstream ss(rest);
    std::wstring token;
    Field f;
    bool haveAny = false;

    while (std::getline(ss, token, L',')) {
        token = trim(token);
        if (token.empty()) continue;
        haveAny = true;

        size_t p = token.find(L'=');
        if (p == std::wstring::npos) {
            if (f.label.empty()) {
                f.label = token; // legacy: erstes token als label
            }
            continue;
        }
        std::wstring k = trim(token.substr(0, p));
        std::wstring v = trim(token.substr(p + 1));
        for (auto& c : k) c = (wchar_t)towlower(c);

        if (k == L"label") f.label = v;
        else if (k == L"offset") { try { f.offset = (size_t)std::stoul(v, nullptr, 0); } catch (...) {} }
        else if (k == L"size") { try { f.size = std::stoi(v, nullptr, 0); } catch (...) {} }
        else if (k == L"mask") { try { f.mask = (unsigned int)std::stoul(v, nullptr, 0); } catch (...) {} }
        else if (k == L"value_offset") { try { f.value_offset = std::stoi(v, nullptr, 0); } catch (...) {} }
    }
    if (!haveAny || f.label.empty()) return false;
    out = f;
    return true;
}

static bool load_ini(Cfg& cfg) {
    fs::path ini;
    if (!find_ini_path(ini)) return false;

    std::wifstream in(ini);
    if (!in.is_open()) return false;
    in.imbue(std::locale(std::locale::empty(), new std::codecvt_utf8<wchar_t>));

    std::wstring line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == L'#' || line[0] == L';') continue;
        size_t p = line.find(L'=');
        if (p == std::wstring::npos) continue;
        std::wstring key = trim(line.substr(0, p));
        std::wstring val = trim(line.substr(p + 1));
        for (auto& c : key) c = (wchar_t)towlower(c);

        if (key == L"base") cfg.base = val;
        else if (key == L"rom") cfg.rom = val;
        else if (key == L"nvram") cfg.nvram = val;
        else if (key == L"field") {
            Field f;
            if (parse_field_line(line, f)) cfg.fields.push_back(f);
        }
    }
    return !cfg.base.empty() && !cfg.nvram.empty();
}

static bool read_bytes(const std::wstring& path, std::vector<uint8_t>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    f.seekg(0, std::ios::end);
    std::streamoff len = f.tellg();
    if (len <= 0) return false;
    f.seekg(0, std::ios::beg);
    out.resize((size_t)len);
    f.read(reinterpret_cast<char*>(out.data()), len);
    return true;
}

static int extract_value(const std::vector<uint8_t>& buf, const Field& f) {
    if (f.offset + (size_t)f.size > buf.size()) return 0;
    unsigned int v = 0;
    if (f.size == 1) v = buf[f.offset];
    else {
        for (int i = f.size - 1; i >= 0; --i)
            v = (v << 8) | buf[f.offset + (size_t)i];
    }
    if (f.mask) v &= f.mask;
    int vi = (int)v;
    vi += f.value_offset;
    return vi;
}

static void ensure_dir(const fs::path& p) {
    std::error_code ec;
    fs::create_directories(p, ec);
}

static void write_json(const std::wstring& base, const std::wstring& rom, int cp, int pc, int cb, int bp) {
    fs::path out = fs::path(base) / L"session_stats" / L"live_control.json";
    ensure_dir(out.parent_path());
    std::ostringstream ss;
    ss << "{";
    ss << "\"rom\":\"" << narrow(rom) << "\"";
    if (cp >= 0) ss << ",\"cp\":" << cp;
    if (pc >= 0) ss << ",\"pc\":" << pc;
    if (cb >= 0) ss << ",\"cb\":" << cb;
    if (bp >= 0) ss << ",\"bp\":" << bp;
    ss << ",\"ts\":" << GetTickCount64();
    ss << "}\n";
    std::ofstream f(out, std::ios::binary | std::ios::trunc);
    auto s = ss.str();
    f.write(s.data(), (std::streamsize)s.size());
}

static DWORD WINAPI Worker(LPVOID) {
    Cfg cfg;
    if (!load_ini(cfg)) {
        Sleep(1500);
        return 0;
    }
    g_running = true;
    FILETIME prevWriteTime{};
    while (g_running) {
        WIN32_FILE_ATTRIBUTE_DATA fad{};
        if (!GetFileAttributesExW(cfg.nvram.c_str(), GetFileExInfoStandard, &fad)) {
            Sleep(200);
            continue;
        }
        bool changed = (memcmp(&fad.ftLastWriteTime, &prevWriteTime, sizeof(FILETIME)) != 0);
        if (changed) prevWriteTime = fad.ftLastWriteTime;

        std::vector<uint8_t> buf;
        if (read_bytes(cfg.nvram, buf)) {
            int cp=-1, pc=-1, cb=-1, bp=-1;
            for (const auto& f : cfg.fields) {
                int v = extract_value(buf, f);
                std::wstring l = f.label;
                for (auto& c: l) c = (wchar_t)towlower(c);
                if (l == L"current_player") cp = v;
                else if (l == L"player_count") pc = v;
                else if (l == L"current_ball") cb = v;
                else if (l == L"balls played" || (l.find(L"balls")!=std::wstring::npos && l.find(L"played")!=std::wstring::npos)) bp = v;
            }
            write_json(cfg.base, cfg.rom, cp, pc, cb, bp);
        }
        Sleep(200);
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE h, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        HANDLE th = CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr);
        if (th) CloseHandle(th);
    } else if (reason == DLL_PROCESS_DETACH) {
        g_running = false;
        Sleep(250);
    }
    return TRUE;
}

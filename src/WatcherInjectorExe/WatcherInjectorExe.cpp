// Minimal Injector EXE:
// - Finds Visual Pinball window "Visual Pinball - ["
// - Injects WatcherInjector{32,64}.dll placed next to the EXE
// - Fallback for local builds: also accepts "WatcherInjector.dll"

#include <windows.h>
#include <string>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

static BOOL CALLBACK EnumCb(HWND h, LPARAM p) {
    if (!IsWindowVisible(h)) return TRUE;
    wchar_t title[512]{0};
    GetWindowTextW(h, title, 511);
    if (wcslen(title) > 0 && wcsstr(title, L"Visual Pinball - [") == title) {
        *reinterpret_cast<HWND*>(p) = h;
        return FALSE;
    }
    return TRUE;
}
static DWORD FindVPXProcess() {
    HWND found = nullptr;
    EnumWindows(EnumCb, reinterpret_cast<LPARAM>(&found));
    if (!found) return 0;
    DWORD pid = 0;
    GetWindowThreadProcessId(found, &pid);
    return pid;
}
static std::wstring ModuleDir() {
    wchar_t path[MAX_PATH]{0};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring p = path;
    size_t pos = p.find_last_of(L"\\/");
    return (pos == std::wstring::npos) ? L"." : p.substr(0, pos);
}

static std::wstring ResolveDllPath() {
    std::wstring dir = ModuleDir();
    // Preferred packaged names
    std::wstring primary = (sizeof(void*) == 8)
        ? (dir + L"\\WatcherInjector64.dll")
        : (dir + L"\\WatcherInjector32.dll");

    std::error_code ec;
    if (fs::exists(primary, ec)) {
        return primary;
    }
    // Local build fallback
    std::wstring local = dir + L"\\WatcherInjector.dll";
    if (fs::exists(local, ec)) {
        return local;
    }
    return primary; // return preferred anyway (error will show later)
}

int wmain() {
    DWORD pid = FindVPXProcess();
    if (!pid) {
        std::wcerr << L"[WatcherInjector] VPX process not found.\n";
        return 1;
    }

    std::wstring dllPath = ResolveDllPath();

    HANDLE hProc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                               PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
                               FALSE, pid);
    if (!hProc) {
        std::wcerr << L"OpenProcess failed: " << GetLastError() << L"\n";
        return 2;
    }

    SIZE_T bytes = (dllPath.size() + 1) * sizeof(wchar_t);
    LPVOID remote = VirtualAllocEx(hProc, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) {
        std::wcerr << L"VirtualAllocEx failed: " << GetLastError() << L"\n";
        CloseHandle(hProc);
        return 3;
    }

    if (!WriteProcessMemory(hProc, remote, dllPath.c_str(), bytes, nullptr)) {
        std::wcerr << L"WriteProcessMemory failed: " << GetLastError() << L"\n";
        VirtualFreeEx(hProc, remote, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return 4;
    }

    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    FARPROC loadLib = GetProcAddress(k32, "LoadLibraryW");
    HANDLE th = CreateRemoteThread(hProc, nullptr, 0,
        (LPTHREAD_START_ROUTINE)loadLib, remote, 0, nullptr);
    if (!th) {
        std::wcerr << L"CreateRemoteThread failed: " << GetLastError() << L"\n";
        VirtualFreeEx(hProc, remote, 0, MEM_RELEASE);
        CloseHandle(hProc);
        return 5;
    }

    WaitForSingleObject(th, 5000);
    VirtualFreeEx(hProc, remote, 0, MEM_RELEASE);
    CloseHandle(th);
    CloseHandle(hProc);
    std::wcout << L"[WatcherInjector] Injection attempted for PID " << pid << L"\n";
    return 0;
}

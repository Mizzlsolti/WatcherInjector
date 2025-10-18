# WatcherInjector (DLL + Injector EXE for Achievement Watcher)

This repository builds the two components used by Achievement Watcher:
- WatcherInjector{32,64}.dll: DLL that reads a simple INI (watchtower_hook.ini in BASE), polls NVRAM, and writes live_control.json
- WatcherInjector{32,64}.exe: Injector that finds Visual Pinball X and injects the DLL

Outputs (on releases):
- WatcherInjector64.dll
- WatcherInjector32.dll
- WatcherInjector64.exe
- WatcherInjector32.exe

How Achievement Watcher downloads them automatically
Set in Achievement Watcher config.json:
```json
{
  "HOOK_AUTO_SETUP": true,
  "HOOK_BIN_URL_BASE": "https://github.com/Mizzlsolti/WatcherInjector/releases/download/v0.1.0"
}
```

Local build (Windows, CMake + MSVC)
```powershell
# x64
cmake -S . -B build64 -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build64 --config Release

# x86
cmake -S . -B build32 -A Win32 -DCMAKE_BUILD_TYPE=Release
cmake --build build32 --config Release
```

INI format (written by Achievement Watcher to BASE\watchtower_hook.ini)
```
base=C:\vPinball\Achievements
rom=afm_113b
nvram=C:\vPinball\VisualPinball\VPinMAME\nvram\afm_113b.nv
field=current_player,offset=123,size=1,mask=3,value_offset=1
field=player_count,offset=55,size=1,mask=0,value_offset=0
field=current_ball,offset=56,size=1,mask=0,value_offset=0
field=Balls Played,offset=200,size=1,mask=0,value_offset=0
```

The DLL writes:
`{ "rom":"...", "cp":1, "pc":1, "cb":1, "bp":0, "ts": 1740000000000 }`
to `BASE\session_stats\live_control.json` every ~200ms.

Notes
- The DLL looks for the INI at one directory above the DLL (BASE\watchtower_hook.ini), since binaries live in BASE\bin.
- Anti-virus may flag injection. Builds are unsigned test binaries.

License
MIT

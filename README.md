# WatcherInjector

Components:
- WatcherInjector{32,64}.dll — reads a simple INI, polls NVRAM, writes live_control.json
- WatcherInjector{32,64}.exe — finds Visual Pinball X and injects the DLL

Build (Windows, CMake + MSVC)
```powershell
# x64
cmake -S . -B build64 -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build64 --config Release

# x86
cmake -S . -B build32 -A Win32 -DCMAKE_BUILD_TYPE=Release
cmake --build build32 --config Release
```

Artifacts (Release):
- build64/bin/WatcherInjector64.dll
- build64/bin/WatcherInjector64.exe
- build32/bin/WatcherInjector32.dll
- build32/bin/WatcherInjector32.exe

INI location and format
- The DLL looks for a single file: `BASE\bin\watcher_hook.ini` (same folder as the DLL).
- Example content:
```
base=C:\vPinball\Achievements
rom=afm_113b
nvram=C:\vPinball\VisualPinball\VPinMAME\nvram\afm_113b.nv
field=label=current_player,offset=123,size=1,mask=3,value_offset=1
field=label=player_count,offset=55,size=1,mask=0,value_offset=0
field=label=current_ball,offset=56,size=1,mask=0,value_offset=0
field=label=Balls Played,offset=200,size=1,mask=0,value_offset=0
```

Output
- Every ~200 ms the DLL writes:
  - `BASE\session_stats\live_control.json`
```json
{ "rom":"afm_113b", "cp":1, "pc":2, "cb":1, "bp":0, "ts": 1740000000000 }
```

Notes
- Only `watcher_hook.ini` in `BASE\bin` is supported now.
- Legacy `watchtower_hook.ini` is not used or created anymore.
- The injector EXE prefers `WatcherInjector64.dll` or `WatcherInjector32.dll` placed next to it (recommended: run from `BASE\bin`).

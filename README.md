# WatcherInjector

This repository builds the two components used by Achievement Watcher:
- WatcherInjector{64}.dll: DLL that reads a simple INI (watchtower_hook.ini in BASE), polls NVRAM, and writes live.session.json
- WatcherInjector{64}.exe: Injector that finds Visual Pinball X and injects the DLL

Outputs (on releases):
- WatcherInjector64.dll
- WatcherInjector64.exe

Local build (Windows, CMake + MSVC)
```powershell
# x64
cmake -S . -B build64 -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build64 --config Release
```

INI format (written by Achievement Watcher to BASE\watchtower_hook.ini or legacy BASE\watcher_hook.ini)
```
base=C:\vPinball\Achievements
rom=afm_113b
nvram=C:\vPinball\VisualPinball\VPinMAME\nvram\afm_113b.nv
field=label=current_player,offset=123,size=1,mask=3,value_offset=1
field=label=player_count,offset=55,size=1,mask=0,value_offset=0
field=label=current_ball,offset=56,size=1,mask=0,value_offset=0
field=label=Balls Played,offset=200,size=1,mask=0,value_offset=0
```

Notes
- The DLL looks for the INI one directory above the DLL (BASE\watchtower_hook.ini preferred), since binaries live in BASE\bin.
- Legacy name BASE\watcher_hook.ini is also accepted (and as a fallback the same files next to the DLL in BASE\bin).
- Field lines are accepted in both formats:
  - `field=label=...,offset=...,size=...,mask=...,value_offset=...`
  - `field=current_player,offset=...,size=...,mask=...,value_offset=...` (legacy, label is the first token)
- Anti-virus may flag injection. Builds are unsigned test binaries.

Where the DLL writes JSON
- The DLL writes every ~200ms to:
  - `BASE\session_stats\live.session.json` (single canonical output file)
- JSON example:
```
{ "rom":"afm_113b", "cp":1, "pc":2, "cb":1, "bp":0, "ts": 1740000000000 }
```

Packaging note
- Release artifacts are named with 64 suffixes. The injector EXE prefers `WatcherInjector64.dll` next to it, but will fallback to `WatcherInjector.dll` for local builds.

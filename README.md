# WatcherInjector
Visual Pinball session tracker and overlay. Reads VPinMAME NVRAM maps, attributes per-player highlights, exports live JSON for overlays, and optionally uses a small DLL/Injector for live control signals.



# WatcherInjector (binaries for Achievement Watcher)

This repository hosts the four prebuilt binaries used by the Watchtower Achievements app to provide live control signals (current_player, player_count, current_ball, balls played) via a DLL hook/injector.

Releases contain these files (names must match exactly):
- WatchtowerHook64.dll
- WatchtowerHook32.dll
- WatchtowerInject64.exe
- WatchtowerInject32.exe

How users consume it (automatic download)
1) In the Watchtower config (config.json):
   ```json
   {
     "HOOK_AUTO_SETUP": true,
     "HOOK_BIN_URL_BASE": "https://github.com/Mizzlsolti/WatcherInjector/releases/download/v0.1.0"
   }
   ```
2) On first run, Watchtower will create BASE\\bin and download any missing files from the URL above.
3) When Visual Pinball X starts, the injector will launch and the DLL will write live_control.json used by the app.

Release process
- Create a new GitHub Release (e.g., tag v0.1.0).
- Upload the four files as release assets.
- Update the HOOK_BIN_URL_BASE in your Watchtower defaults or instruct users to set it in their config.

Security/Integrity
- Optionally include checksums (SHA256) as separate assets (e.g., `SHA256SUMS.txt`) so users or the app can verify downloads.

License
- MIT for the repository contents. (If the DLL/EXE include third-party components, make sure to comply with their licenses in the release notes.)

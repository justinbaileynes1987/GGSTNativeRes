# Changelog

## 1.1.0 (2026-10-08)

- **Microsoft Store / Xbox app / PC Game Pass version supported.** That version's executable
  (`RED-WinGDK-Shipping.exe`) doesn't load `xapofx1_5.dll`, so it gets its own build that loads as
  `dsound.dll`: `GGSTNativeRes-1.1.0-xbox.zip`. Confirmed on that version by a player at 3840×2160.
  The Steam download is now `GGSTNativeRes-1.1.0-steam.zip`.
- **Fixed: Borderless and Windowed still got one exclusive-Fullscreen switch at boot.** Fix 1 changed
  the window mode the game *stores*, but the game requests the resolution with a copy of the mode
  held in a CPU register, which was still Fullscreen. The resolution was right, so Fullscreen players
  (including all of 1.0.0's testing) never saw it. Now both are patched. Found from the Microsoft
  Store tester's diagnostic log; reproduced and verified on Steam in Borderless and Windowed.
- `Mode=-1` reads the Microsoft Store version's settings folder (`Saved\Config\WinGDK`).
- If the game folder isn't writable, the log goes to `%LOCALAPPDATA%\GGST\Saved\GGSTNativeRes.log`.
  The log's first line now says which build loaded and in which executable.
- If both DLLs are installed, only one of them runs.
- New tool: `tools/xbox-check.ps1` reports the Microsoft Store version's files and loaded DLLs.

## 1.0.0 (2026-10-03)

First release.

- Stops Guilty Gear -Strive- from switching to 1920×1080 at boot, by fixing both causes:
  - **Fix 1**, the game's own boot-time default (1920×1080, Fullscreen), applied before your save
    is loaded. It now uses your resolution and window mode.
  - **Fix 2**, Unreal Engine 4 capping exclusive Fullscreen at the monitor's EDID "native"
    resolution, which some monitors report as 1920×1080. The cap now uses your desktop size.
- `GGSTNativeRes.ini`: target width/height (default: desktop resolution), window mode (default: your
  in-game setting), log on/off, and a switch for each fix.
- Loads as a proxy for `xapofx1_5.dll`; no function hooks; each fix is skipped safely if a game
  update changes the code it expects.
- Tested on Windows with the 2026-09-24 game build, alone and together with StriveLabs v2-47.
- Evidence for how it works: [VERIFICATION.md](VERIFICATION.md).

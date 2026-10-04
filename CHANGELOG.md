# Changelog

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

# Research tools

Used to find the boot-time resolution code. Rerun these if a Strive update breaks the patterns
(the mod's log will say `Unexpected match count - game updated? Not patching.`).

Requires Python with `pefile` and `capstone`.

1. Start Strive and wait for the title screen (SteamStub has unpacked `.text` by then).
2. `python dump.py GGST-dump.exe` — read-only copy of the running exe's in-memory image,
   with section headers fixed up so pefile/capstone/Ghidra can load it. **Never commit or share
   the dump** — it's the game's code.
3. `python find1080.py` — lists every place in `.text` that uses 1920 and 1080 together.
4. `python ctx.py <rva> ...` — disassembles the function containing each hit.
   `python fn.py <start> <end>` — full disassembly of a range with raw bytes (for writing patterns).

`watch-ggst.ps1` launches Strive through Steam and logs window size, window style and the primary
display mode every N ms — that's how we measured the 1080p switch and verified the fix.
(`-Enforce` was an experiment that resizes the window externally; it doesn't work, see below.)

## What we found (game build of 2026-09-24)

The screen-settings apply routine (RVA `0xd40090` in that build):

```
if (fingerprint(resolution list) != saved fingerprint):   // true at boot, before SYSTEM.sav loads
    resolutionIndex = index of 1920x1080 (or largest mode if 1080p unsupported)
    windowMode      = 0   // EWindowMode: 0 Fullscreen, 1 WindowedFullscreen, 2 Windowed
GameUserSettings->SetScreenResolution(list[resolutionIndex])
GameUserSettings->SetFullscreenMode(windowMode)
FSystemResolution::RequestResolutionChange(w, h, windowMode)
```

The same reset fires whenever the monitor's mode list changes (new monitor, driver update).

## Things that don't work (tested)

- Editing / read-only `GameUserSettings.ini` — the engine reads it fine; the game overrides it.
- `-ResX/-ResY` launch options — same override.
- Externally resizing the window — the game does a real exclusive-fullscreen mode change.
- Editing `SYSTEM.sav` — encrypted.
- Setting borderless in-game — boot default is still exclusive 1080p.

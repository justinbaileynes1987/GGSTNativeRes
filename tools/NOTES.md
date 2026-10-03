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

The "list" is the RHI's available display modes (12-byte entries: width, height, refresh rate, i.e.
`FScreenResolutionRHI`). The fingerprint is `sum(width + height) + count`. The save stores an *index*
into this list (byte at `+0x66b9e0` of the settings object), the window mode (`+0x66b9e1`) and the
fingerprint (`+0x66b9dc`).

## Second cause: UE4 EDID "native resolution" clamp (exclusive Fullscreen only)

Found with the diag build (`build.bat diag`), which hooks `FSystemResolution::RequestResolutionChange`
(RVA `0x2d27a60`) and logs callers:

```
RequestResolutionChange(1920, 1080, mode 0) from exe+0x296a9bc   <- UGameUserSettings::PreloadResolutionSettings
RequestResolutionChange(2560, 1440, mode 0) from exe+0xd4020d    <- the routine above (after fix 1)
```

`PreloadResolutionSettings` reads the ini correctly (2560x1440, mode 0), then calls
`UGameEngine::ConditionallyOverrideSettings` (RVA `0x2930300`) → `DetermineGameWindowResolution`
(RVA `0x2934170`). In Fullscreen, that caps the resolution at the primary monitor's `NativeWidth/Height`
from `FMonitorInfo`, which UE4 reads from the first detailed timing descriptor of the EDID in the
registry. The dev machine's ASUS XG27AQDMG (1440p 240 Hz OLED) lists 1920x1080 there. Running with
`-ForceRes` confirmed it. Fix 2 turns the `jne` at RVA `0x29342d6` into `jmp`, so Fullscreen uses the
desktop size as the cap, like the other modes do.

To check a machine's EDIDs, decode `HKLM\SYSTEM\CurrentControlSet\Enum\DISPLAY\*\*\Device Parameters\EDID`:
first DTD width = `e[56] | (e[58] >> 4) << 8`, height = `e[59] | (e[61] >> 4) << 8`.

## SteamStub note

`.text` is decrypted progressively at startup: patterns early in the exe can show up before
later ones (RequestResolutionChange appeared ~100 ms after the boot-default code). The patch thread
keeps scanning until every pattern it needs has appeared. Measured margin on the dev machine:
patches land ~1 s before the engine's preload runs.

## Loader DLL name

Originally `sensapi.dll`, but StriveLabs ships its own `sensapi.dll` (plus `dwmapi.dll` and
`xinput1_3.dll`) into the same folder. Now `xapofx1_5.dll`: statically imported by the exe, not a
KnownDLL, one export (`CreateFX`), and not used by any known mod loader.

## Things that don't work (tested)

- Editing / read-only `GameUserSettings.ini` — the engine reads it fine; the game overrides it.
- `-ResX/-ResY` launch options — same override.
- Externally resizing the window — the game does a real exclusive-fullscreen mode change.
- Editing `SYSTEM.sav` — encrypted.
- Setting borderless in-game — boot default is still exclusive 1080p.
- `-ForceRes` launch option — fixes the UE4 EDID clamp (cause 2) but not the game's boot default (cause 1).

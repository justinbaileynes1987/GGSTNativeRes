# GGSTNativeRes

A tiny mod for **Guilty Gear -Strive-** (Steam, PC) that stops the game from switching your monitor
to **1920×1080 every time it starts**.

## The problem

Strive ignores your settings for the first few seconds after launch:

1. The window opens at your saved resolution.
2. About a second later, the game switches to **1920×1080**. In exclusive fullscreen your monitor
   changes mode, the screen flashes, and windows on your other monitors get shuffled around.
3. Several seconds later it loads your settings from `SYSTEM.sav` and switches **again** to your real
   resolution and window mode.

None of the usual fixes work. The engine already reads `GameUserSettings.ini` correctly, so editing
it (or making it read-only) can't help; `-ResX/-ResY` launch options and picking Borderless in the
game's options don't stop it either; and `SYSTEM.sav` can't be edited (it has no standard Unreal
save header and its contents look random, i.e. it's encrypted or otherwise obfuscated).

With this mod the game starts in your real resolution and window mode, and nothing switches.

## Install

1. Download `GGSTNativeRes-<version>.zip` from the [Releases](../../releases) page. Only download
   it from there, and check its SHA-256 hash against the one listed on the release.
2. Put `xapofx1_5.dll` from the zip in the folder that contains `GGST-Win64-Shipping.exe`:
   ```
   ...\steamapps\common\GUILTY GEAR STRIVE\RED\Binaries\Win64\
   ```
   (In Steam, right-click the game, then **Manage → Browse local files** to find it.)
3. Launch the game normally.

**Uninstall:** delete `xapofx1_5.dll`, `GGSTNativeRes.ini` and `GGSTNativeRes.log` from that folder.

**Linux / Steam Deck (Proton), untested:** Proton won't load the DLL unless told to. Add this to the
game's Steam launch options:

```
WINEDLLOVERRIDES="xapofx1_5=n,b" %command%
```

If you also use StriveLabs on Proton, combine the two: `WINEDLLOVERRIDES="sensapi=n,b;xapofx1_5=n,b" %command%`

## Settings

On first launch the mod creates `GGSTNativeRes.ini` next to the DLL:

```ini
[Settings]
Width=0     ; 0 = your monitor's current desktop resolution
Height=0
Mode=-1     ; -1 = same as your in-game setting, 0 = Fullscreen, 1 = Borderless, 2 = Windowed
Log=1       ; write GGSTNativeRes.log
FixBootDefault=1    ; fix 1 below (1 = on, 0 = off)
FixFullscreenCap=1  ; fix 2 below (1 = on, 0 = off)
```

The defaults are right for most people: your desktop resolution, and whichever window mode you
picked in the game's options. The two `Fix...` switches let you turn each fix off individually,
for example to compare against the stock game (see [VERIFICATION.md](VERIFICATION.md)).

## How it works

There are actually **two** separate causes of the 1080p switch, and the mod fixes both. Every
claim in this section is backed by evidence you can check yourself in
[VERIFICATION.md](VERIFICATION.md).

**1. Strive's own boot-time default.** The game stores your display setting as an *index* into
the list of display modes your monitor supports, together with a fingerprint of that list. Every
time the settings are applied, the game checks the fingerprint. If it doesn't match, for example
because the monitor changed and the saved index might now point at the wrong mode, the game resets
to a default: **1920×1080** (or the largest mode, if the monitor can't do 1080p), window mode
**Fullscreen**. At boot this check runs *before* your save has loaded, so it always fails and you
always get the default for the first few seconds. The mod replaces the default's width, height and
window mode with yours. (Reading the code, the same reset should also happen whenever your
monitor's mode list changes, e.g. after a driver update; that part hasn't been tested.)

**2. An Unreal Engine 4 quirk with some monitors.** Before the game's own code runs, the engine
reads `GameUserSettings.ini` and applies it. In exclusive Fullscreen only, it caps the resolution
at what it thinks your monitor's *native* resolution is, and it gets that from the **first
detailed timing descriptor** in the monitor's EDID (the identity data every monitor reports).
Some monitors put a 1920×1080 mode in that slot even though they're 1440p or 4K. The ASUS
XG27AQDMG this mod was developed on lists 1920×1080 @ 240 Hz there, and its real 2560×1440 @ 240 Hz
mode only appears in an extension block. So UE4 decides it's a 1080p monitor. Run
[`tools/check_edid.py`](tools/check_edid.py) to see what UE4 reads from yours. The mod makes
exclusive Fullscreen use your desktop size as the cap instead, which is what Borderless and
Windowed already do. (UE4's `-ForceRes` launch option also gets around this one, but it doesn't
help with cause 1.)

*Side effect of fix 2:* at boot, exclusive Fullscreen can no longer start at a resolution *above*
your desktop resolution (e.g. a 4K TV used with a 1080p desktop). The game's own settings are
applied a few seconds later as normal, so you'd just see one switch at boot, like without the mod.
If that's your setup, set `FixFullscreenCap=0`.

What the mod does and doesn't do:

- **No function hooks or detours.** It changes 5 constants and 1 jump instruction in the game's
  memory at startup, then stops running.
- **Touches nothing else.** Input, netcode, gameplay, saves and game files are all untouched.
- **Fails safe.** It searches for the exact code it expects and applies each fix only if its patterns
  are found exactly once, in the expected layout. If a game update changes that code, that fix is
  skipped and the game behaves as stock. `GGSTNativeRes.log` says what was and wasn't patched.
  (These checks confirm the code looks the same, not that it still means the same thing. An update
  that kept these exact bytes but changed their purpose would be extremely unlikely, but it isn't
  impossible.)
- **Only runs inside Strive.** It does nothing if loaded by any other program.

It loads as a proxy for `xapofx1_5.dll`, a DirectX audio-effects DLL the game imports. The game
loads DLLs from its own folder first, and the mod passes the DLL's single function, `CreateFX`,
through to the real copy in `System32`.

Strive has no client-side anti-cheat that we could find: no EasyAntiCheat, BattlEye or kernel
driver in the install folder or loaded in the game. What its servers check is unknown.

### Compatibility with other mods

Mod loaders work by placing a DLL with a specific name in the game folder, so two mods that use the
same name overwrite each other. Names used by popular Strive mods:

| Mod | DLL names |
|---|---|
| StriveLabs | `dwmapi.dll`, `xinput1_3.dll`, `sensapi.dll` |
| UE4SS-based mods | `dwmapi.dll` (sometimes `xinput1_3.dll`) |
| GGST-Enhancer | `UMPDC.dll` |
| **GGSTNativeRes** | **`xapofx1_5.dll`** |

GGSTNativeRes doesn't hook anything, so it shouldn't interfere with mods that do.

**Unverum** (the mod manager, [TekkaGB/Unverum](https://github.com/TekkaGB/Unverum), checked against
its source, not tested): it installs `.pak` mods into `Content\Paks\~mods`, and its own UE4SS
(`dwmapi.dll`, `ue4ss.dll`) into `Binaries\Win64` when a mod needs it. Each time it applies mods it
deletes `opengl32.dll`, `patternsleuth_bind.dll`, `ue4ss.dll`, `UE4SS-settings.ini`, `dwmapi.dll`,
`Win64\Mods` and `LogicMods`. None of those are GGSTNativeRes files, so it leaves this mod alone.
Beware of look-alike "Unverum Mod Manager 2026" repos; the original is TekkaGB's.

**Tested together:** StriveLabs v2-47 (UE4SS) on Windows. Both load, StriveLabs starts normally, and
the resolution fixes still apply before the engine's boot-time resolution code runs.

## Building

Requires Visual Studio 2022 or newer with the **Desktop development with C++** workload.

```
build.bat          release build   -> build\xapofx1_5.dll
build.bat diag     diagnostic build -> build\diag\xapofx1_5.dll
```

`release.ps1` builds the release and packages `dist\GGSTNativeRes-<version>.zip` plus
`dist\SHA256SUMS.txt`.

**Reproducible builds:** the build uses `/Brepro`, so the same source and compiler always produce a
byte-identical DLL. To check that a release DLL was built from this source, check out the release's
tag, run `build.bat` with the same MSVC toolset (listed in the release notes), and compare the
SHA-256 of `build\xapofx1_5.dll` with the published one. A different compiler version will produce
a different (but equivalent) binary.

The diagnostic build also logs every resolution change the game requests (with the calling
address) and every display-mode change, on one timeline. It's meant for development and adds a
function hook, so it's not for release.

`tools/` contains the scripts used to find the code and verify the fix, and notes for redoing the
analysis after a game update. See [tools/NOTES.md](tools/NOTES.md).

## Disclaimer

Unofficial and not affiliated with or endorsed by Arc System Works. Use at your own risk.

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

None of the usual fixes work. The game overrides `GameUserSettings.ini` (read-only or not),
`-ResX/-ResY` launch options and the in-game display mode at boot, and `SYSTEM.sav` is encrypted.

With this mod the game starts in your real resolution and window mode, and nothing switches.

## Install

1. Download `xapofx1_5.dll` from the [Releases](../../releases) page.
2. Put it in the folder that contains `GGST-Win64-Shipping.exe`:
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
```

The defaults are right for most people: your desktop resolution, and whichever window mode you
picked in the game's options.

## How it works

There are actually **two** separate causes of the 1080p switch, and the mod fixes both.

**1. Strive's own boot-time default.** Your display settings are saved in `SYSTEM.sav` as an
*index* into the list of display modes your monitor supports, together with a fingerprint of that
list. Every time the settings are applied, the game checks the fingerprint. If it doesn't match,
for example because the monitor changed and the saved index might now point at the wrong mode,
the game resets to a safe default: **1920×1080**, window mode **Fullscreen**. The bug is that at
boot this check runs *before* the save has loaded, so it always fails and you always get the
default for the first few seconds. (The same reset happens whenever your monitor's mode list
changes.) The mod replaces the default's width, height and window mode with yours.

**2. An Unreal Engine 4 quirk with some monitors.** Before the game's own code runs, the engine
reads `GameUserSettings.ini` and applies it. In exclusive Fullscreen it first caps the resolution
at what it thinks your monitor's *native* resolution is, and it gets that from the first "detailed
timing" entry in the monitor's EDID (the identity data every monitor reports). Many newer monitors,
especially high-refresh OLEDs and HDMI 2.1 models, list a **1920×1080** compatibility mode there and
put their real resolution in an extension block. So UE4 decides your 1440p or 4K monitor is a 1080p
monitor. The mod makes exclusive Fullscreen use your desktop size as the cap instead, which is what
Borderless and Windowed already do. (UE4's `-ForceRes` launch option also gets around this one, but
it doesn't help with cause 1.)

What the mod does and doesn't do:

- **No function hooks or detours.** It changes 5 constants and 1 jump instruction in the game's
  memory at startup, then stops running.
- **Touches nothing else.** Input, netcode, gameplay, saves and game files are all untouched.
- **Fails safe.** It searches for the exact code it expects and applies each fix only if its patterns
  are found exactly once, in the expected layout. If a game update changes that code, that fix is
  skipped and the game behaves as stock. `GGSTNativeRes.log` says what was and wasn't patched.
- **Only runs inside Strive.** It does nothing if loaded by any other program.

It loads as a proxy for `xapofx1_5.dll`, a DirectX audio-effects DLL the game imports. The game
loads DLLs from its own folder first, and the mod passes the DLL's single function, `CreateFX`,
through to the real copy in `System32`.

Strive has no anti-cheat (no EasyAntiCheat, BattlEye or kernel driver).

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

## Building

Requires Visual Studio 2022 or newer with the **Desktop development with C++** workload.

```
build.bat          release build   -> build\xapofx1_5.dll
build.bat diag     diagnostic build -> build\diag\xapofx1_5.dll
```

The diagnostic build also logs every resolution change the game requests (with the calling
address) and every display-mode change, on one timeline. It's meant for development and adds a
function hook, so it's not for release.

`tools/` contains the scripts used to find the code and verify the fix, and notes for redoing the
analysis after a game update. See [tools/NOTES.md](tools/NOTES.md).

## Disclaimer

Unofficial and not affiliated with or endorsed by Arc System Works. Use at your own risk.

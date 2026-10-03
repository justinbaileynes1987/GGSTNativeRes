# GGSTNativeRes

A tiny mod for **Guilty Gear -Strive-** (Steam, PC) that stops the game from switching your monitor
to **1920×1080 exclusive fullscreen every time it starts**.

## The problem

Strive ignores your settings for the first few seconds after launch:

1. The window opens at your saved resolution.
2. About a second later, the game switches to **exclusive fullscreen 1920×1080**. Your monitor changes mode,
   the screen flashes, and windows on your other monitors get shuffled around.
3. Several seconds later it loads your settings from `SYSTEM.sav` and switches **again** to your real
   resolution and window mode.

None of the usual fixes work. The game overrides `GameUserSettings.ini` (read-only or not),
`-ResX/-ResY` launch options and the in-game display mode at boot, and `SYSTEM.sav` is encrypted.

With this mod the game starts in your real resolution and window mode, and nothing switches.

## Install

1. Download `sensapi.dll` from the [Releases](../../releases) page.
2. Put it in the folder that contains `GGST-Win64-Shipping.exe`:
   ```
   ...\steamapps\common\GUILTY GEAR STRIVE\RED\Binaries\Win64\
   ```
   (In Steam, right-click the game, then **Manage → Browse local files** to find it.)
3. Launch the game normally.

**Uninstall:** delete `sensapi.dll`, `GGSTNativeRes.ini` and `GGSTNativeRes.log` from that folder.

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

At startup, before your save is loaded, Strive's screen-settings code notices that its stored
"list of supported resolutions" doesn't match your monitor and resets to a hardcoded default:
the **1920×1080** entry, window mode **0 (Fullscreen)**. The same reset also happens whenever your
monitor's mode list changes, for example after a new monitor or a driver update.

The mod replaces those three default values (width, height, window mode) in the game's memory with
your settings. That's the whole mod:

- **No function hooks or detours.** It changes 5 constants inside one function, then stops running.
- **Touches nothing else.** Input, netcode, gameplay, saves and game files are all untouched.
- **Fails safe.** It searches for the exact code it expects and patches only if every pattern is found
  exactly once, in the expected layout. If a game update changes that code, it does nothing and the
  game behaves as if the mod weren't installed. `GGSTNativeRes.log` says which case happened.
- **Only runs inside Strive.** It does nothing if loaded by any other program.

It loads as a proxy for `sensapi.dll`, a small Windows networking-status DLL the game imports. The
game loads DLLs from its own folder first, and the mod passes all 3 of `sensapi.dll`'s functions
through to the real copy in `System32`.

Strive has no anti-cheat (no EasyAntiCheat, BattlEye or kernel driver).

### Compatibility with other mods

`sensapi.dll` was chosen so it doesn't collide with the DLL names other Strive mods use, such as
`dwmapi.dll` (UE4SS-based mods) and `UMPDC.dll` (GGST-Enhancer). It doesn't hook anything, so it
shouldn't interfere with mods that do.

## Building

Requires Visual Studio 2022 or newer with the **Desktop development with C++** workload.

```
build.bat
```

Output: `build\sensapi.dll`.

`tools/` contains the scripts used to find the code and verify the fix, and notes for redoing the
analysis after a game update. See [tools/NOTES.md](tools/NOTES.md).

## Disclaimer

Unofficial and not affiliated with or endorsed by Arc System Works. Use at your own risk.

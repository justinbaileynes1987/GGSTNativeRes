# Verification: evidence for every claim in the README

This document lists each claim the [README](README.md) makes, what kind of evidence backs it, and
how to check it yourself. It was also used as an audit of our own work, and the README was corrected
where a claim didn't hold up. Those corrections are listed [at the end](#audit-findings-and-corrections).

Game build checked: `GGST-Win64-Shipping.exe` dated 2026-09-24 (engine string `++UE4+Release-4.25Plus`).
Test machine: Windows 11, four monitors, primary = ASUS XG27AQDMG at 2560×1440 @ 240 Hz.

### Evidence labels

| Label | Meaning |
|---|---|
| **TESTED** | Measured in a controlled experiment; the procedure is below so you can repeat it. |
| **CODE** | Shown directly in the game's machine code; [`tools/verify_code.py`](tools/verify_code.py) prints it from your own copy of the game. |
| **DATA** | Read from Windows (registry, display APIs); [`tools/check_edid.py`](tools/check_edid.py) prints it for your own monitors. |
| **INFERENCE** | Follows from the evidence above but wasn't directly observed. Stated as such. |

**About function names.** The shipping exe has no debug symbols, so names like
`DetermineGameWindowResolution` are *identifications*: each function was matched to the
Unreal Engine 4.25 source by the strings it uses, its structure and its constants. The string
references are shown below so you can judge the match yourself. Nothing in the argument depends on
the names. The behaviour is shown directly in the code and the experiments.

---

## How to reproduce

Requirements: Windows, the Steam version of the game, Python 3 with `pip install pefile capstone`,
and Visual Studio 2022+ (C++ workload) to build the mod.

| Tool | What it does | Needs the game running? |
|---|---|---|
| `tools/check_edid.py` | For each active monitor: which one is primary, which registry EDID Windows has for it, what UE4 will treat as its "native" resolution, and every timing the EDID actually lists (base block, CTA-861 and DisplayID extensions). | No |
| `tools/dump.py` | Copies the game's decrypted code out of memory (read-only). The exe on disk is encrypted by Steam's DRM, so this is the only way to read it. | Yes |
| `tools/verify_code.py` | Finds the relevant functions in a dump **by the strings they reference or by byte pattern** (no hardcoded addresses) and prints annotated disassembly. | No (uses the dump) |
| `build.bat diag` | Builds the diagnostic version of the mod. It logs every `RequestResolutionChange` call (resolution, window mode, calling address) and every display-mode change, on one timeline. | — |
| `tools/experiment.ps1` | Sets the mod's `FixBootDefault` / `FixFullscreenCap` switches, launches the game, prints the diagnostic timeline, closes the game. | Launches it |
| `tools/watch-ggst.ps1` | Launches the game and logs window size and display mode from outside the process. Needs no mod at all. | Launches it |

For a stock dump, set both `Fix...` switches to `0` (or remove the DLL), start the game, wait for
the title screen, then run `python tools/dump.py GGST-dump.exe` and `python tools/verify_code.py GGST-dump.exe`.

> Note: exclusive fullscreen only takes over the display while the game window **has focus**. If you
> launch from a terminal and the window doesn't get focus, the resolution *requests* still happen
> (and are logged), but the monitor may not actually change mode. The request log is the reliable
> signal; the `DISPLAY` lines depend on focus.

---

## 1. The symptom

> *"About a second later, the game switches to 1920×1080 ... several seconds later it loads your
> settings and switches again."*

**TESTED** with `watch-ggst.ps1` and no mod installed (in-game setting: Fullscreen 2560×1440):

```
  4.2s  window 2560x1440                       <- engine creates the window at the ini resolution
  5.5s  window 1920x1080                       <- the game shrinks it
 12.1s  window 2560x1440                       <- save loaded, real settings applied
```

With the game focused, the display mode itself switched (`display=1920x1080@240`, then back).
The diagnostic build shows the requests behind this (experiment A, below).

## 2. "None of the usual fixes work"

| Claim | Evidence |
|---|---|
| The engine already reads `GameUserSettings.ini` correctly, so editing it can't help | **TESTED + CODE.** The ini already contained `ResolutionSizeX=2560`, `ResolutionSizeY=1440`, and the window was created at 2560×1440 (§1). The engine's preload function reads exactly these keys (§4.4). In experiment F it requested exactly the ini's values. The 1080p comes from two later steps, not from the ini. |
| Making it read-only can't help | **INFERENCE**, not tested. Read-only only stops the game *writing* the file, and the values it *reads* were already correct. |
| `-ResX=2560 -ResY=1440` doesn't help | **TESTED.** Same 1080p switch at the same moment. **CODE:** `-ResX/-ResY` are parsed *before* the Fullscreen cap in `DetermineGameWindowResolution` (§4.4), so they get capped too; and they don't affect the game's own default at all (§3). |
| Choosing Borderless in-game doesn't help | **TESTED.** With Borderless saved, boot still did an exclusive-fullscreen switch to 1920×1080 (the game's default mode is Fullscreen regardless of your setting, §3). |
| `SYSTEM.sav` can't be edited | **DATA.** 69,856 bytes, entropy 7.90 bits/byte (8.0 = random), no `GVAS` header (the standard Unreal save header), no zlib/gzip signature. Contains no readable setting names. This proves it isn't readable as-is; it doesn't prove *which* encryption or obfuscation is used. |

## 3. Cause 1: the game's own boot-time default

### 3.1 The code (CODE)

`verify_code.py` section 1 locates this function by the byte pattern
`41 81 3C 8E 80 07 00 00 75 ?? 41 81 7C 8E 04 38 04 00 00 74` (exactly one match). Key lines
(addresses are offsets in the 2026-09-24 build):

```
0xd400f0..fe   loop: edx += entry.width + entry.height, 12-byte entries   <- fingerprint
0xd40100:      lea r8d, [r9 + rdx]                     ; fingerprint = sum(w+h) + count
0xd40104:      cmp [rdi + 0x66b9dc], r8d               ; compare with stored fingerprint
0xd4010b:      je  (skip the reset)
0xd40118:      cmp [r14+rcx*4], 0x780 / jb             ; largest mode >= 1920 x 1080 ?
0xd40122:      cmp [r14+rcx*4+4], 0x438 / jb           ;   (if not: use the largest mode)
0xd40137:      cmp [r14+rcx*4], 0x780                  ; find the index of 1920x1080
0xd40141:      cmp [r14+rcx*4+4], 0x438
0xd40156:      mov [rdi + 0x66b9dc], r8d               ; store new fingerprint
0xd4015f:      mov byte [rdi + 0x66b9e0], dl           ; resolution index = that entry
0xd40165:      mov byte [rdi + 0x66b9e1], 0            ; window mode = 0
...
0xd4018e:      movsx rax, byte [rdi + 0x66b9e0]        ; look up width/height by index
0xd401f2:      call 0x296f720                          ; GameUserSettings->SetScreenResolution(w,h)
0xd401fc:      call 0x296eb70                          ; GameUserSettings->SetFullscreenMode(mode)
0xd40208:      call 0x2d27a60                          ; FSystemResolution::RequestResolutionChange(w,h,mode)
```

What this establishes:
- The setting is stored as an **index** (`+0x66b9e0`) plus a **fingerprint** (`+0x66b9dc`) of the
  mode list. The fingerprint is `sum(width + height) + count`.
- On a mismatch, the default is the **1920×1080** entry (or the largest mode if the monitor can't
  do 1080p), with **window mode 0**.
- Window mode 0 = Fullscreen: the value is passed straight to `SetFullscreenMode` and
  `RequestResolutionChange`, and logged requests show `mode 0` = exclusive fullscreen,
  `mode 1` = borderless (experiment F).
- The list entries are 12 bytes; the first two fields are width and height (compared with
  1920/1080). That the third is the refresh rate is an **INFERENCE** (it matches UE's
  `FScreenResolutionRHI` layout).

### 3.2 It runs before the save loads (TESTED + INFERENCE)

In every stock run, this function (`exe+0xd4020d` in the logs) issues two requests: **1920×1080**
about 4 s after launch, then **your saved resolution** about 10 s later. On the second call it
produced your saved setting rather than the 1920×1080 default, so the fingerprint check passed.
That means the stored fingerprint and index were your saved ones by then, i.e. loaded from your
save between the two calls. On the first call they weren't there yet, so it reset. *Which file* they
come from (`SYSTEM.sav` vs `SYSTEM2.sav`) is **INFERENCE**: the save files are unreadable (§2).

The README's note that the same reset should happen after a monitor/driver change is **INFERENCE**
from the code (any change to the mode list changes the fingerprint). It hasn't been tested.

## 4. Cause 2: Unreal Engine's EDID-based Fullscreen cap

This is a chain of five links. Each one is shown separately.

### 4.1 What your monitor's EDID says (DATA)

`python tools/check_edid.py` on the test machine:

```
\\.\DISPLAY1  primary=True  desktop mode=2560x1440@240
    monitor device ID: MONITOR\AUS27F2\{4d36e96e-...}\0004   -> UE4 model key: AUS27F2
    registry EDID [5&2e8aea03&0&UID4356] name=XG27AQDMG  extension blocks=2
      UE4 'native' (1st DTD): 1920x1080 @ 239.99 Hz
        base block DTD 1                 1920x1080 @ 239.99 Hz
        ext 2 (DisplayID type I)         2560x1440 @ 239.97 Hz

\\.\DISPLAY2  primary=False  desktop mode=2560x1440@59
    monitor device ID: MONITOR\AUS27B1\...   -> UE4 model key: AUS27B1
    registry EDID [...UID4353] name=ROG PG278QR  extension blocks=1
      UE4 'native' (1st DTD): 2560x1440 @ 59.95 Hz
```

So the **primary** display is the XG27AQDMG, and the first detailed timing descriptor (DTD) in its
EDID is **1920×1080 @ 240 Hz**. Its 2560×1440 @ 240 Hz mode appears only in extension block 2, a
DisplayID block. The PG278QR (not primary) lists 2560×1440 first, so it wouldn't trigger the cap.

You can check this by hand. The registry value
`HKLM\SYSTEM\CurrentControlSet\Enum\DISPLAY\AUS27F2\5&2e8aea03&0&UID4356\Device Parameters\EDID`
is 384 bytes, EDID version 1.4. Bytes 54–71 (the first DTD) are:

```
ea ec 80 a0 70 38 87 40 30 20 35 00 4b 4a 21 00 00 1a
```

- width  = byte 56 | (byte 58 >> 4) << 8 = `0x80 | 0x7 << 8` = `0x780` = **1920**
- height = byte 59 | (byte 61 >> 4) << 8 = `0x38 | 0x4 << 8` = `0x438` = **1080**

(The EDID 1.4 standard calls the first DTD the "preferred timing mode". Why this monitor prefers
1080p there is up to ASUS; it may depend on firmware or the connection used. We only claim what the
data shows.)

### 4.2 The game reads exactly those bytes (CODE)

`verify_code.py` section 4 finds the only function that references the string `"EDID"`
(`0x16d4d70` in this build). It is a `RegEnumValue` loop: 512-character name buffer, 1024-byte data
buffer at `rbp+0x120`, name compared to `"EDID"`. Then:

| Instruction | Offset into EDID buffer | EDID byte | Meaning |
|---|---|---|---|
| `movzx eax, byte [rbp+0x158]` | 0x158 − 0x120 = 0x38 | **56** | width, low 8 bits |
| `movzx ecx, byte [rbp+0x15a]` → `and ecx, 0xF0` → `shl ecx, 4` → `or ecx, eax` | 0x3A | **58** | width, high 4 bits |
| `movzx eax, byte [rbp+0x15b]` | 0x3B | **59** | height, low 8 bits |
| `movzx ecx, byte [rbp+0x15d]` → `and ecx, 0xF0` → `shl ecx, 4` → `or ecx, eax` | 0x3D | **61** | height, high 4 bits |
| `movzx edx, byte [rbp+0x135]`, `byte [rbp+0x136]` | 0x15, 0x16 | 21, 22 | screen size in cm (for DPI) |

No other part of the EDID is read: not the other DTDs, and not the extension blocks. Earlier in
the same function: `CM_Get_Device_ID` with a 200-character buffer (`MAX_DEVICE_ID_LEN`), a
substring of the device ID starting at character 8 (`MONITOR\AUS27F2\...` → `AUS27F2`), and
`SetupDiOpenDevRegKey(..., KEY_READ = 0x20019)` on the matching monitor device. This matches UE
4.25's `GetSizeForDevID` / `GetMonitorSizeFromEDID` in `WindowsApplication.cpp`.

### 4.3 The result becomes the monitor's NativeWidth/NativeHeight (CODE)

The only caller of that function (`0x16d3e2c`, inside `GetMonitorInfo`) builds an `FMonitorInfo`
on the stack at `rsp+0x40` and passes:

| Argument | Address | Struct offset | Field |
|---|---|---|---|
| `rcx` | `rsp+0x40` | +0x00 | `Name`, the model key, e.g. `AUS27F2` |
| `rdx` | `rsp+0x60` | **+0x20** | `NativeWidth` (written by the EDID reader) |
| `r8`  | `rsp+0x64` | **+0x24** | `NativeHeight` |
| `r9`  | `rbp-0x74` | +0x4C | `DPI` |

After the call: `test byte [rbp+0x454], 4` (`DISPLAY_DEVICE_PRIMARY_DEVICE`) then
`seta byte [rbp-0x78]`. That's struct offset **+0x48** (since `rbp-0x74` = +0x4C), the
`bIsPrimary` flag. The struct is 0x50 bytes, matching UE 4.25's `FMonitorInfo`.

### 4.4 Fullscreen caps the resolution at that value (CODE)

`verify_code.py` section 3 finds the function referencing `"ForceRes"`. It also references
`"Res="`, `"ResX="`, `"ResY="` and `"Portrait"`, matching `UGameEngine::DetermineGameWindowResolution`.

```
0x29342d0: mov ebx, [rbp-0x35]            ; max = desktop size (PrimaryDisplayWidth/Height) ...
0x29342d3: test r15d, r15d                ; ... unless window mode == 0 (Fullscreen):
0x29342d6: jne  (skip)                    ;   <- fix 2 turns this jne (75) into jmp (EB)
0x29342ef: mov edi, [rcx+0x20]            ;   max = MonitorInfo[...].NativeWidth   (+0x20)
0x29342f2: mov ebx, [rcx+0x24]            ;   max = MonitorInfo[...].NativeHeight  (+0x24)
0x2934300: cmp byte [rcx+0x48], r13b      ;   ... of the entry with bIsPrimary     (+0x48)
0x2934306: add rcx, 0x50                  ;   (entries are 0x50 bytes)
0x293431f: lea rdx, "ForceRes"            ; if -ForceRes: skip the cap entirely
0x293433b: if (resX <= 0 || resX > maxX || resY <= 0 || resY > maxY) resX, resY = maxX, maxY
```

The caller chain is shown by `verify_code.py` section 2. The function referencing
`"ResolutionSizeX"` also references `"GameUserSettingsClassName"`, `"Version"` (checked `== 5`),
`"bUseDesktopResolution"`, `"FullscreenMode"`, `"ResolutionSizeY"` and `"bUseHDRDisplayOutput"`,
matching UE 4.25's `UGameUserSettings::PreloadResolutionSettings`. It calls the override chain
(`0x2930300` → `0x2934170`) and then `RequestResolutionChange` from `exe+0x296a9bc`, the
address seen in the logs below.

### 4.5 Putting it together (TESTED)

Your ini says 2560×1440, mode 0 → the preload reads that → in Fullscreen the cap is
`NativeWidth/Height` of the primary monitor → which is the first EDID DTD → which is 1920×1080 →
2560 > 1920, so the request becomes 1920×1080. The experiments below confirm each step.

## 5. Experiments: each fix is necessary, both together are sufficient (TESTED)

Diagnostic build, `tools/experiment.ps1`, StriveLabs also installed, in-game setting Fullscreen
2560×1440, mod's `Mode=0`. `0x296a9bc` = engine preload (cause 2), `0xd4020d` = the game's
settings routine (cause 1 on its first call, your saved settings on its second).

| Run | `FixBootDefault` | `FixFullscreenCap` | Extra | Engine preload | Game, 1st call | Game, after save | Display |
|---|---|---|---|---|---|---|---|
| A | 0 | 0 | — | **1920×1080** mode 0 | **1920×1080** mode 0 | 2560×1440 | no switch (window unfocused) |
| B | 1 | 0 | — | **1920×1080** mode 0 | 2560×1440 | 2560×1440 | 1080p for 1.6 s |
| C | 0 | 1 | — | 2560×1440 | **1920×1080** mode 0 | 2560×1440 | 1080p for ~12 s |
| D | 1 | 1 | — | 2560×1440 | 2560×1440 | 2560×1440 | **never changes** |
| E | 0 | 0 | `-ForceRes` | 2560×1440 | **1920×1080** mode 0 | 2560×1440 | 1080p for ~8 s |
| F | 0 | 0 | ini `FullscreenMode=1` | 2560×1440 **mode 1** | **1920×1080** mode 0 | 2560×1440 | 1080p for ~8 s |

What each run shows:
- **A vs D:** together the two fixes remove both 1080p requests.
- **B, C:** each fix alone leaves the other cause in place, so both are necessary.
- **E:** `-ForceRes` removes cause 2 only, matching the `"ForceRes"` branch in §4.4.
- **F:** with Borderless in the ini, the engine's request is *not* capped. The cap only applies to
  Fullscreen (mode 0), matching `test r15d, r15d` in §4.4.

Run C also logged a third caller, `RequestResolutionChange(1920, 1080)` from `exe+0x248cad5`. That
function re-issues the engine's *current* stored request (the `ResX`/`ResY`/`WindowMode` globals,
then sets the flag after them), matching UE4's `FSystemResolution::ForceRefresh`. It only repeated
1080p because the game had just requested it; it never appears when both fixes are on (run D). It
isn't a third cause.

Raw log of run D:
```
Patched boot default: 1920x1080 Fullscreen -> 2560x1440 mode 0
Patched fullscreen native-size cap: uses desktop size instead of monitor EDID
RequestResolutionChange(2560, 1440, mode 0) from exe+0x296a9bc
RequestResolutionChange(2560, 1440, mode 0) from exe+0xd4020d
RequestResolutionChange(2560, 1440, mode 0) from exe+0xd4020d
```

## 6. Claims about the mod itself

| README claim | Evidence |
|---|---|
| No function hooks or detours; 5 constants and 1 jump | **CODE** ([`src/main.cpp`](src/main.cpp)): Fix 1 writes four `int32` values (width and height in two compare instructions) and one byte (window mode); Fix 2 writes one byte (`75` → `EB`). That's all the release build writes to the game's memory. The hook in the diagnostic build is behind `#ifdef GGSTNR_DIAG`. |
| ...then stops running | **CODE.** The patch thread exits after patching. Nothing else is created. |
| Fails safe | **CODE.** Fix 1 requires each of its three patterns to match exactly once, in order, within 0x80 bytes. Fix 2 requires one match whose jump byte is `75`. Otherwise the fix is skipped and logged. These checks verify the code *looks* the same; see the README caveat. |
| Only runs inside Strive | **CODE.** `DllMain` checks the host exe is `GGST-Win64-Shipping.exe`. |
| Loads as a proxy for `xapofx1_5.dll` and forwards `CreateFX` | **CODE + TESTED.** The exe statically imports `xapofx1_5.dll`, which isn't a Windows KnownDLL, so the game folder is searched first. With the mod installed, the game's module list showed both `...\RED\Binaries\Win64\XAPOFX1_5.dll` (the mod) and `C:\WINDOWS\system32\xapofx1_5.dll` (the real one, loaded by the mod). |
| Patches land before the engine's boot code | **TESTED.** Three runs with StriveLabs installed: patches applied 925–998 ms before the engine preload. Most of the time before patching is spent waiting for Steam's DRM to decrypt the code (see `tools/NOTES.md`). |
| `Mode=-1` = same as your in-game setting | **TESTED.** After choosing Borderless in-game, `GameUserSettings.ini` had `FullscreenMode=1`, and the mod logged `mode 1 (Borderless)`. |
| No client-side anti-cheat that we could find | **DATA.** No EasyAntiCheat, BattlEye or other anti-cheat files, and no `.sys` drivers, in the install folder; none in the game's loaded module list. (The exe contains the text "AntiCheat" once; we didn't trace what uses it, but no anti-cheat module is shipped or loaded.) Server-side checks are unknown. |
| Compatibility table | **DATA.** StriveLabs: file names in its v2-47 zip. UE4SS: its install docs. GGST-Enhancer: its README. **TESTED** together with StriveLabs v2-47: both load, StriveLabs' frame bar and hitboxes work, both fixes apply. |
| Linux / Proton line | **Untested.** It says so in the README. |

## 7. Audit findings and corrections

Found while writing this document, and fixed:

| # | Finding | Fix |
|---|---|---|
| 1 | README said many newer monitors (OLED, HDMI 2.1) put 1080p in the first EDID slot. That's a generalization from one monitor. | Reworded to what the data shows: this monitor does it; `check_edid.py` lets anyone check theirs. |
| 2 | README said the game "overrides" `GameUserSettings.ini`, "read-only or not". Wrong framing: the engine reads it correctly; read-only was never tested. | Reworded (§2). |
| 3 | README said `SYSTEM.sav` is "encrypted". The data shows it's unreadable, not specifically encrypted. | Reworded. |
| 4 | README said Strive "has no anti-cheat". We only checked for client-side anti-cheat. | Reworded. |
| 5 | Fix 2's side effect (no above-desktop Fullscreen at boot) wasn't documented. | Documented, plus a `FixFullscreenCap=0` switch. |
| 6 | **Bug:** `Mode=-1` was read with `GetPrivateProfileInt`, which Microsoft documents as returning 0 for negative values. It happened to work on our system, but that behaviour is undocumented. | Settings are now parsed from the string. |
| 7 | **Bug:** if a game update broke one of the patterns the scan waits for, the patch thread kept scanning at raised priority for 30 s during startup. | Gives up 2 s after the first pattern appears, or after 10 s overall, and backs off to slow polling after 3 s. |
| 8 | **Bug (tool):** `check_edid.py` read past the end of the timing list in CTA-861 blocks and printed a bogus `1077x132 @ 1319 Hz` mode. | Stops at the end-of-list marker. |
| 9 | Earlier in development, the README claimed `sensapi.dll` didn't conflict with other mods. StriveLabs ships its own `sensapi.dll`. | Switched to `xapofx1_5.dll` (in an earlier commit). |

Still **not** verified:
- That the same reset happens after a monitor or driver change (§3.2): code inference only.
- Which save file holds the index and fingerprint: the saves can't be read.
- How many other monitors put 1080p in the first EDID slot.
- Linux/Proton.

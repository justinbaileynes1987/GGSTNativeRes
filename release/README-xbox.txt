GGSTNativeRes 1.1.0
Boot-time resolution fix for Guilty Gear -Strive-
(Microsoft Store / Xbox app / PC Game Pass version)

Stops the game from switching your monitor to 1920x1080 for the first few
seconds after launch. The game starts in your real resolution and window mode.

This zip is for the Microsoft Store / Xbox app version only. For the Steam
version, download GGSTNativeRes-1.1.0-steam.zip instead.

INSTALL
  Extract this zip into the game's install folder, e.g.
    C:\XboxGames\GUILTY GEAR STRIVE\
  so that dsound.dll ends up in
    C:\XboxGames\GUILTY GEAR STRIVE\Content\RED\Binaries\WinGDK\
  Launch the game normally.

UNINSTALL
  Delete dsound.dll, GGSTNativeRes.ini and GGSTNativeRes.log from the WinGDK
  folder (and GGSTNativeRes.log from %LOCALAPPDATA%\GGST\Saved if it's there).

SETTINGS
  GGSTNativeRes.ini is created next to the DLL on first launch. The defaults
  (desktop resolution, your in-game window mode) are right for most people.

CHECK THAT IT WORKED
  GGSTNativeRes.log should end with two "Patched ..." lines. It's next to
  dsound.dll, or in %LOCALAPPDATA%\GGST\Saved if the game folder isn't writable.

Source, documentation and how it works:
  https://github.com/justinbaileynes1987/GGSTNativeRes
Only download this mod from the GitHub Releases page above or its official
GameBanana page (https://gamebanana.com/mods/723773), and check the SHA-256
hash listed on the GitHub release.

MIT License - see LICENSE.txt. Not affiliated with Arc System Works.

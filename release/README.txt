GGSTNativeRes 1.0.0
Boot-time resolution fix for Guilty Gear -Strive- (Steam, PC)

Stops the game from switching your monitor to 1920x1080 for the first few
seconds after launch. The game starts in your real resolution and window mode.

INSTALL
  Copy xapofx1_5.dll into the folder that contains GGST-Win64-Shipping.exe:
    ...\steamapps\common\GUILTY GEAR STRIVE\RED\Binaries\Win64\
  (In Steam: right-click the game > Manage > Browse local files.)
  Launch the game normally.

UNINSTALL
  Delete xapofx1_5.dll, GGSTNativeRes.ini and GGSTNativeRes.log from that folder.

LINUX / STEAM DECK (Proton) - untested
  Add to the game's Steam launch options:
    WINEDLLOVERRIDES="xapofx1_5=n,b" %command%

SETTINGS
  GGSTNativeRes.ini is created next to the DLL on first launch. The defaults
  (desktop resolution, your in-game window mode) are right for most people.

CHECK THAT IT WORKED
  GGSTNativeRes.log (next to the DLL) should end with two "Patched ..." lines.

Source, documentation and how it works:
  https://github.com/justinbaileynes1987/GGSTNativeRes
Only download this mod from the GitHub Releases page above or its official
GameBanana page (https://gamebanana.com/mods/723773), and check the SHA-256
hash listed on the GitHub release.

MIT License - see LICENSE.txt. Not affiliated with Arc System Works.

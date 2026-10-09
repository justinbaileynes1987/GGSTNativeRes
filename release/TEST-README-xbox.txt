GGSTNativeRes 1.1.0-test.1 - TEST BUILD for the Microsoft Store / Xbox app version
of Guilty Gear -Strive-

This is a test build. Besides applying the fix, it writes a detailed log (every
resolution change the game requests) so we can confirm it works on your version.

INSTALL
  Extract this zip into the game's install folder, e.g.
    C:\XboxGames\GUILTY GEAR STRIVE\
  so that dsound.dll ends up in
    C:\XboxGames\GUILTY GEAR STRIVE\Content\RED\Binaries\WinGDK\
  (Same place StriveLabs' Xbox files go. It doesn't conflict with StriveLabs.)

TEST
  1. Start the game and wait until the title screen.
  2. Close the game.
  3. Send us GGSTNativeRes.log. It's next to dsound.dll in the WinGDK folder,
     or, if the game couldn't write there, in
       %LOCALAPPDATA%\GGST\Saved\GGSTNativeRes.log
     (paste %LOCALAPPDATA%\GGST\Saved into the File Explorer address bar).
  Also tell us whether you still saw the switch to 1080p at launch.

UNINSTALL
  Delete dsound.dll, GGSTNativeRes.ini and GGSTNativeRes.log from the WinGDK
  folder (and GGSTNativeRes.log from %LOCALAPPDATA%\GGST\Saved if it's there).

https://github.com/justinbaileynes1987/GGSTNativeRes

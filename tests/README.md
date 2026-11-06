# Discord connection tests

Run from a Visual Studio C++ developer PowerShell prompt (x86 recommended):

```powershell
powershell -ExecutionPolicy Bypass -File tests/run.ps1
```

For a standalone Clang/MinGW toolchain, pass its compiler path:

```powershell
powershell -ExecutionPolicy Bypass -File tests/run.ps1 -Cxx C:/toolchain/bin/i686-w64-mingw32-clang++.exe
```

These tests use the real vendored Discord C++ wrappers with a fake SDK C entry
point. No Discord installation, account, running game, or network is needed.
They check optional initialization flags, retry timing, late startup, repeated
disconnect/reconnect, current activity and timestamp preservation, callback
failures, safe SDK destruction, and bounded activity requests.

## In-game verification

1. Fully quit Discord and launch the game. Discord should stay closed and the
   game should run normally.
2. Start Discord and sign in. Rich Presence should appear after the next retry
   (normally within a few seconds once Discord is ready).
3. Fully quit or kill Discord while playing. The game should continue normally
   and Discord should stay closed.
4. Change game activity, restart Discord, and confirm the new activity appears
   with the original game session start time. Repeat the quit/restart cycle.
5. Also launch the game with Discord already running and confirm presence works.

Closing Discord's window may leave it running in the system tray; fully quit it
to test a disconnected client. Discord's activity privacy settings must allow
Rich Presence to be displayed.

## v1.0.5

- Adds glyph compatibility for game build CL 583916 while retaining CL 574912 and CL 583774 support.
- Fixes connected keyboards being counted as gamepads during native prompt selection.
- Supports the exact CL 583916 executable produced by the optional XInput workaround.
- Includes `Set-XInputWorkaround.ps1` for players whose controller input fails through GameInput. This separate, opt-in patch selects the game's built-in XInput backend and backs up the original executable.
- Adds device-selection diagnostics and patch/restore regression checks.

**Keep Steam Input enabled for PlayStation controllers.** The optional workaround is controller-only: keyboard/mouse gameplay may be unavailable. It supports only CL 583916 and refuses other executable hashes. Run it with `-Restore` to undo; uninstalling the glyph DLL alone does not undo the workaround.

Extract the ZIP. Copy `build/XInput1_4.dll` and `ShrekControllerGlyphs.ini` beside `Shrek.exe`, or run `Install.ps1`. Remove the previous glyph installation first. Apply the optional input workaround only if needed; see the README for commands and backup details.

The ZIP includes a compiled DLL; building from source is not required. No game executable, backup, debug symbols, or game artwork is distributed.

Validation: automated glyph/controller tests, DLL forwarding, installer preservation checks, and XInput patch/restore and changed-build rejection tests passed. The author confirmed both controller input and PlayStation glyphs working in-game on CL 583916 with the XInput workaround and Steam Input enabled.

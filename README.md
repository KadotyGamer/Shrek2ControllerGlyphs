# Shrek 2 Remastered — Automatic Controller Glyphs

A Windows mod for **DreamWorks Shrek 2 Remastered (2026)** that shows PlayStation
button prompts while using Steam Input. Xbox controllers keep the game's original
prompts. Version **1.0.0**.

The game ships PlayStation artwork, but Steam Input can expose a PlayStation
controller as an Xbox-compatible device. This mod asks Steam for the underlying
controller type and selects the corresponding artwork during prompt rendering.

## Features

- Recognizes Steam's PS5, PS4 and PS3 controller types, including DualSense Edge.
- Uses 26 bundled PlayStation textures for 27 key prompts: face buttons, shoulders,
  triggers, D-pad, sticks, stick clicks, Options and Create.
- Refreshes controller identity every 200 ms.
- Preserves keyboard and mouse prompts and forwards XInput calls to Windows.
- Supports automatic selection or a manual PlayStation/Xbox override.
- Adds files beside the executable; does not modify original assets or saves.

## Compatibility and testing

Targets the **Windows Steam release, CL 574912**. This is not a mod for the original
2004 PC game or for console versions.

Supported `Shrek.exe` SHA-256:

```text
00B67356582A4F3D900DB855224BA9379F9980DCCCFF3C89C3558CF0EEFB11CB
```

The author confirmed PlayStation prompts working in-game with a **DualSense Edge
and Steam Input enabled**. The diagnostic log confirmed Steam type 13 and the
GameInput prompt hook. Other physical controller models, mixed-controller co-op,
and physical hot-swapping have not yet been verified in-game.

Automated tests cover 3,220 key/controller combinations, simulated Steam slot
changes and disconnections, native fallback behavior, DLL loading, ordinal imports,
and forwarding to Windows XInput. Installer tests check install/uninstall,
refusal to overwrite existing files, and preservation of the original executable.

The installer checks the executable hash. Runtime hooks also check the build,
function signature and vtable entries. A different game update may require a mod
update; unsupported runtime builds keep normal XInput forwarding without glyph hooks.

## Installation

If you downloaded source, [build the DLL](#building-from-source) first.

1. Close the game.
2. Copy `build/XInput1_4.dll` and `ShrekControllerGlyphs.ini` into the game folder,
   beside `Shrek.exe`.
3. **Enable Steam Input** for this game in Steam's controller properties.
4. Launch through Steam and press a controller button.

Do not overwrite an existing `XInput1_4.dll` from another mod. Proxy chaining is
not implemented. No ASI loader or other mod framework is required.

Alternatively, the included PowerShell installer verifies compatibility and refuses
to overwrite an existing installation:

```powershell
.\Install.ps1
# For a Steam library in another location:
.\Install.ps1 -GameDirectory 'D:\SteamLibrary\steamapps\common\Shrek2Remastered'
```

Windows may require administrator access to write to a protected game folder.

## Configuration

Edit `ShrekControllerGlyphs.ini` in the game folder and restart:

```ini
[Glyphs]
Mode=Auto
```

| Mode | Behavior |
| --- | --- |
| `Auto` | Detect controller family through Steam Input. |
| `PlayStation` | Force PlayStation gamepad prompts. |
| `Xbox` | Use the original Xbox gamepad prompts. |

`ShrekControllerGlyphs.log` records startup, detected Steam types, slot changes,
and the first PlayStation prompt selected. Type `13` is PS5, `5` is PS4, and
`12` is PS3.

## Limitations

- XInput detection is per slot. The native GameInput fallback only overrides when
  exactly one native gamepad and one Steam-mapped controller are detected; it
  avoids guessing identities in mixed native-controller sessions.
- Other translation software may expose only a virtual Xbox controller. If Steam
  cannot identify the original hardware, use `Mode=PlayStation`.
- Back/View uses the bundled Create/share icon. Custom Steam mappings to touchpad
  clicks or back paddles do not get specialized prompts.
- This changes glyphs only. It does not add adaptive triggers, advanced haptics,
  native DualSense input handling, or controller remapping.

## Uninstallation

Close the game and remove the added `XInput1_4.dll` and `ShrekControllerGlyphs.ini`.
You can also remove this mod's log and installation manifest if present.

If you used the installer:

```powershell
.\Install.ps1 -Uninstall
# Add -GameDirectory if you installed into a different Steam library.
```

The uninstaller verifies the installed DLL before removing it. An edited config
and the diagnostic log are retained. Leave the game's other DLLs alone.

## Building from source

Requires Windows x64, Visual Studio C++ build tools and a Windows SDK.

```powershell
.\build.cmd
```

The script locates a Visual Studio installation with the x64 C++ toolchain, builds
`build/XInput1_4.dll`, compiles the test harness, and runs the tests. The static
MSVC runtime is used; there are no third-party library dependencies.

Optional installer test, with the supported game installed at the default Steam path:

```powershell
.\test-install.ps1
```

This creates an isolated fixture under `build`; it does not install into the actual
game folder. Build outputs, game files, runtime logs, and local analysis dumps are
excluded from Git.

## How it works

The proxy provides the game's XInput ordinal imports and forwards them to the real
DLL loaded by an absolute System32 path. A Steam callback import hook initializes
controller detection on the game's callback thread. Verified prompt vtable entries
select PlayStation textures while preserving original input handling.

Controller identification uses Valve's documented
[GetControllerForGamepadIndex](https://partner.steamgames.com/doc/api/isteaminput#GetControllerForGamepadIndex)
and [GetInputTypeForHandle](https://partner.steamgames.com/doc/api/isteaminput#GetInputTypeForHandle)
APIs. No game executable, debug symbols, or glyph artwork is distributed here.

## Reporting issues

Include your game build, controller model, USB/Bluetooth connection, Steam Input
setting, whether another controller translator or proxy DLL is installed, and
relevant lines from `ShrekControllerGlyphs.log`. Do not upload game files or saves.

## Credits and license

Project direction and in-game testing: **Andrew Adkins (@drew73197)**.
The mod code, tests, and documentation were generated using OpenAI Codex and
tested locally. The PlayStation artwork comes from the user's installed game.

Mod source is available under the [MIT license](LICENSE). That license applies
to this project's code and documentation, not to the game or its assets.
This is an unofficial community mod, not affiliated with the game's publishers
or with Sony, Microsoft, or Valve.

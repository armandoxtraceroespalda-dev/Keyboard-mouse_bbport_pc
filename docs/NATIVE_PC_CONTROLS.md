# Source integration: PC controls and native game menus

This integrates the retained features of version 1.5.27 into source archive revision
`b4dcc823b1c759bdf621704bab449a3dc54e0cad`. It preserves the original engine/renderer.

## Resulting features

- Launcher GPU selection and selected-GPU reporting; reorganized Controls, Tweaks & Cheats,
  Gameplay Tweaks and Show FPS in Display & FPS.
- Controller / Keyboard and mouse selection in the launcher and vanilla Controls. Both
  binding configurations remain available; displayed bindings/prompts follow the selection.
- Default Dark Souls III PC keyboard/mouse profile, configurable key/mouse chords, sprint
  direction shaping, separate interaction/item use and Escape as Options.
- Standard launcher assignment rows for attacks and next/previous items; in-game categorized
  Key Bindings under vanilla Controls and confirmed restoration of DS3/controller defaults.
- Native six-slot next/previous quick-item selection on wheel down/up, with gameplay gating
  and HUD updates retained. Shift + wheel retains weapon switching.
- Mouse menu navigation: left-click selection, right-click back, title WASD navigation,
  and in-world inventory use alongside character movement. Menu mouse remains enabled;
  mouse camera follows the selected input mode. Residual pointer focus is 34ms.
- Shared vanilla camera axes, sensitivity and auto-adjust; FOV/distance/height controls under
  Environment / Camera options. Redundant PC Controls/Enhancements sections are absent.
- Keyboard text prompts for menu and gameplay actions; action-specific binding descriptions.
- Hunter's Dream appearance editing and Altar of Despair rebirth with the Yharnam Stone.
- Added UI strings in English, Spanish, Russian, Japanese, French, German, Italian and
  Brazilian Portuguese. Game text follows game language; launcher text follows launcher UI.

## Integration points

| Source | Responsibility |
| --- | --- |
| `launcher/bbport_launcher_win.py` | Ordinary import/install of PC launcher modules |
| `launcher/bbport_integrated_*.py` | UI, settings, profiles, localization and local asset preparation |
| `src/runtime_pc_ui_api.h` | Typed/versioned host/module interface; no host RVAs |
| `src/runtime_pc_ui.c` | Load matching module beside bb-probe and dispatch source calls |
| `src/probe.c` | Register module before GPU setup; initialize guest hooks after relocation/patching |
| `src/runtime_pad.c` | Separate native keyboard/mouse input from physical controller sampling |
| `gpu/shim/window.cpp` | Events, capture and input reload without private-class offsets |
| `gpu/shim/bbgpu.cpp`, `bloodborne_cam.cpp` | Gate/invert accumulated and direct mouse motion |
| `gpu/shim/bbport_overlay.cpp` | Preserve PC settings when renderer settings are saved |
| `native/CMakeLists.txt` | Build matching module and converter from source |
| `build.sh`, `packaging/windows/package.sh` | Include native components and runtime dependencies |

There is no `bb-probe-ui.exe`, PE/COFF patcher or frozen-launcher bytecode patcher in this
source integration. Older managed `BB_PROBE=.../bb-probe-ui.exe` overrides are discarded.
The source-built DLL uses API version 1; do not mix it with the old binary-patched DLL.
The loader rejects missing/incompatible modules when native features are requested.

The module deliberately uses its own settings namespace in its DLL, separate from the
renderer. `runtime_pc_ui_api.h` is the boundary; keep its version/size guards. Event API
publication uses C11 acquire/release atomics because the window thread is already running.

## Build on Windows

Use the existing [MSYS2 CLANG64 instructions](../packaging/windows/README.md), including
all packages listed there (zlib/Python/CMake are required for resource generation).
This GitHub-style ZIP omits the contents of six Git submodules. A complete build requires
a Git checkout of the same project/revision and:

```bash
git submodule update --init --recursive
bash build.sh
bash packaging/windows/package.sh
```

If your source is only an extracted ZIP with no `.git`, obtain a full checkout of the same
repository/revision first; `git submodule update` cannot reconstruct missing Git metadata.
Copy the replacement files into that checkout before building. Existing game data is not
part of the source package and is not needed to compile.

`build.sh` produces `out/bb-probe.exe`, `out/bb-native-ui.dll`, `out/bb-native-assets.exe`
and the original support executables. Packaging creates the launcher and installs the two
native components beside bb-probe in `bin`. Rebuild/package the complete set together.
Do not manually replace only the launcher executable in an old binary-patched installation.

For source development, the launcher/asset preparation resolves `bin` and `out` locations.
The profile is an importable Python constant; no separate preset file is required at runtime.

For an isolated native-module build with a Windows toolchain and zlib:

```bash
cmake -S native -B out/pc-native -G Ninja
cmake --build out/pc-native --target bb-native-ui bb-native-assets bb-pc-host-check bb-pc-api-link-test
```

The optional Windows `bb-pc-api-link-test.exe` can load the matching DLL without a game:
from its output folder run `BB_NATIVE_MENUS=1 ./bb-pc-api-link-test.exe` in MSYS2. Keep the
module beside the test and run with normal native-options/mirror flags unset.

## Validation

Python/source setup checks:

```bash
python tools/test_pc_controls.py
```

Linux x64 unit/resource checks (g++ and zlib development headers required):

```bash
python tools/test_pc_controls.py --native-tests
# Optional private original resources; these are never packaged:
python tools/test_pc_controls.py --native-tests --game-eboot /path/to/eboot.bin --game-menu /path/to/optionsetting.gfx
```

The supplied module/converter and C interface were compiled for Windows x64. Launcher,
input, persistence, categories, camera, menu pointer, eight-language catalog and resource
checks run separately. The original forward item leaf is compared against compiled policy
in 10,000 cases when a verified user-supplied 1.09 executable is provided.

A complete renderer/launcher distribution was not built in this Linux environment: the
uploaded ZIP has empty submodules and lacks the full MSYS2 dependency environment. Actual
Windows gameplay validation is still required, including submenu visibility, cursor timing,
wheel behavior and translation layout. Do not treat automated checks as a gameplay test.

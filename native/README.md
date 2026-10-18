# Native PC controls and game UI

This contains the retained PC control/UI features from integration 1.5.27, now connected
through source rather than modifications to a compiled host executable.

- `pc_ui/`: native input, chords, mouse menu navigation, game option pages, camera,
  text prompts, appearance editor and rebirth. It builds as `bb-native-ui.dll`.
- `pc_assets/`: serialization of the user's own menu/movie/message/talk resources into
  a private cache. It builds as `bb-native-assets.exe`. No game assets are distributed.
- `tests/`: launcher, game-language and resource-view tests.
- `../src/runtime_pc_ui_api.h`: versioned C interface. C and C++ layouts are checked.

The original engine/renderer is preserved. The DLL is a separate link unit so its private
`BbSettings` structures cannot alias the renderer's settings. Host integration supplies
function pointers and explicit call sites; it does not scan COFF symbols, modify PE
sections/imports, or assume private SDL window class offsets. Native hooks inside the
supported 1.09 guest image remain signature-checked.

Windows CMake builds the module and converter as dependencies of `bb-probe`. `build.sh`
copies them to `out`; Windows packaging includes them in `bin` and collects their DLL
runtime dependencies. See [the integration guide](../docs/NATIVE_PC_CONTROLS.md).

Localization lives in `launcher/bbport_integrated_localization.py`. After editing it:

```bash
python native/pc_ui/pages/generate_language_text.py
```

The resource converter's C++ string table is generated from
`pc_assets/pc_option_messages.tsv` by CMake. Do not hand-edit its generated `.inc` file.

## Provenance and licensing

The host project is Supermedo/bloodborne_pc (source archive revision
`b4dcc823b1c759bdf621704bab449a3dc54e0cad`). Native UI portions are adapted from
[droogie/bbhost](https://github.com/droogie/bbhost) at
`7c790536c2c27ad7bb5115e3b1a12d7ffd7a972e` and
`db5457cbd447f4ef129b58df0a78e0a8819c7033`, including isolated adapters published in
[DarkIzuku/bloodborne_pc](https://github.com/DarkIzuku/bloodborne_pc), preview revision
`8ecba9a4abdd54747e31d7fee54982e422b5f8e2`. No replacement renderer or donor engine is
included. Individual file headers identify GPL-2.0-or-later/GPL-3.0-or-later provenance;
`licenses/` contains the applicable texts and notices. Integration with GPL3 components
is distributed under GPL-3.0-or-later. The original project's license remains in place.

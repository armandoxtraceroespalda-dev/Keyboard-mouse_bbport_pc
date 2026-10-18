Native PC UI source module

Active documentation: ../README.md and ../../docs/NATIVE_PC_CONTROLS.md.

Build with the project CMake targets. menu_bridge.cpp exposes BbPcGetApi and
receives source callbacks through src/runtime_pc_ui_api.h. There is no host
binary patcher or private-class offset map. Guest 1.09 hooks remain checked.

The settings namespace is DLL-local, isolated from the original renderer.
Keep this as a module when integrating; do not simply link both incompatible
BbSettings layouts into the same executable.

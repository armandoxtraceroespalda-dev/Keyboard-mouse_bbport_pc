Serialization-only native PC assets tool, generator 16.

Build target: bb-native-assets. CMake uses installed zlib and generates the
message string table from pc_option_messages.tsv. Original user resources
are read-only inputs; output belongs in a separate local cache.

Tests use locally supplied original game resources, never distributed here.
See ../README.md and ../../docs/NATIVE_PC_CONTROLS.md.

cmake_minimum_required(VERSION 3.24)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(bbpc_native LANGUAGES C CXX)
    if(NOT WIN32)
        message(FATAL_ERROR "Use a Windows/MinGW toolchain for the PC UI module")
    endif()
endif()
# SPDX-License-Identifier: GPL-3.0-or-later
# Native UI is a host-ABI module; the renderer and guest runtime remain unchanged.
find_package(ZLIB REQUIRED)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(PC_MESSAGES "${CMAKE_CURRENT_BINARY_DIR}/pc_option_messages.inc")
add_custom_command(OUTPUT "${PC_MESSAGES}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/pc_assets/generate_messages.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/pc_assets/pc_option_messages.tsv" "${PC_MESSAGES}"
    DEPENDS pc_assets/generate_messages.py pc_assets/pc_option_messages.tsv
    VERBATIM)
set_source_files_properties("${PC_MESSAGES}" PROPERTIES HEADER_FILE_ONLY TRUE)

set(PC_UI pc_ui)
add_library(bb-native-ui SHARED
    ${PC_UI}/menu_bridge.cpp ${PC_UI}/menu_pointer.cpp
    ${PC_UI}/native_mirror.cpp ${PC_UI}/engine_hooks.cpp ${PC_UI}/menu_memory.cpp
    ${PC_UI}/pages/option_menu.cpp ${PC_UI}/pages/engine_state.cpp
    ${PC_UI}/pages/camera.cpp ${PC_UI}/pages/rebirth.cpp ${PC_UI}/pages/bindings.cpp
    ${PC_UI}/pages/settings.cpp ${PC_UI}/pages/input_runtime.cpp
    ${PC_UI}/pages/camera_rotation.cpp ${PC_UI}/pages/key_prompts.cpp
    ${PC_UI}/pages/item_cycle.cpp)
target_compile_features(bb-native-ui PRIVATE cxx_std_20)
target_compile_definitions(bb-native-ui PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(bb-native-ui PRIVATE -Wno-deprecated-declarations)
target_link_libraries(bb-native-ui PRIVATE user32 kernel32)
set_target_properties(bb-native-ui PROPERTIES PREFIX "" INTERPROCEDURAL_OPTIMIZATION OFF
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
add_executable(bb-native-assets pc_assets/menu_assets.cpp pc_assets/rebirth_script.cpp
    pc_assets/esd.cpp pc_assets/sha256.cpp "${PC_MESSAGES}")
target_include_directories(bb-native-assets PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(bb-native-assets PRIVATE cxx_std_20)
target_compile_definitions(bb-native-assets PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(bb-native-assets PRIVATE ZLIB::ZLIB shell32)
set_target_properties(bb-native-assets PROPERTIES INTERPROCEDURAL_OPTIMIZATION OFF
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

# Optional standalone syntax/ABI check for the C side of the source integration.
add_library(bb-pc-host-check OBJECT EXCLUDE_FROM_ALL ../src/runtime_pc_ui.c tests/api_layout.c)
target_compile_features(bb-pc-host-check PRIVATE c_std_11)
target_compile_options(bb-pc-host-check PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(bb-pc-host-check PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)

add_executable(bb-pc-api-link-test EXCLUDE_FROM_ALL ../src/runtime_pc_ui.c tests/host_link_smoke.c)
target_compile_features(bb-pc-api-link-test PRIVATE c_std_11)
target_compile_options(bb-pc-api-link-test PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_compile_definitions(bb-pc-api-link-test PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
set_target_properties(bb-pc-api-link-test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
add_dependencies(bb-pc-api-link-test bb-native-ui)

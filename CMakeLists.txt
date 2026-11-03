cmake_minimum_required(VERSION 3.18)
project(Sandveil C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(RAYLIB REQUIRED IMPORTED_TARGET raylib)

add_executable(sandveil MACOSX_BUNDLE
    src/main.c
    src/world.c
    src/game.c
)
target_include_directories(sandveil PRIVATE src)
target_link_libraries(sandveil PRIVATE PkgConfig::RAYLIB m)
target_compile_options(sandveil PRIVATE -Wall -Wextra -Wpedantic)
set_target_properties(sandveil PROPERTIES
    MACOSX_BUNDLE_BUNDLE_NAME "SANDVEIL"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.sandveil.prototype"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1.0"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/platform/Info.plist.in"
)

include(CTest)
if(BUILD_TESTING)
    add_executable(game_logic_tests tests/test_game.c src/game.c)
    target_include_directories(game_logic_tests PRIVATE src)
    target_link_libraries(game_logic_tests PRIVATE m)
    target_compile_options(game_logic_tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME game_logic_tests COMMAND game_logic_tests)

    add_executable(psx_renderer_tests tests/test_psx_renderer.c src/psx/renderer.c)
    target_include_directories(psx_renderer_tests PRIVATE src)
    target_compile_options(psx_renderer_tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME psx_renderer_tests COMMAND psx_renderer_tests)
endif()

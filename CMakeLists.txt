# Native SDL2 Joust for Linux handhelds and desktops (aarch64, x86_64),
# Windows (MinGW) and the web (Emscripten). See JOUST_PORT_PLAN.md.
cmake_minimum_required(VERSION 3.13)
project(joust VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
# The sim must stay bit-exact across architectures: no -ffast-math.
add_compile_options(-Wall -Wextra -Wno-unused-parameter)

option(JOUST_SANITIZE "Build with ASan/UBSan" OFF)
if(JOUST_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

add_library(sdl2 INTERFACE)
if(EMSCRIPTEN)
  # Emscripten's own SDL2 port
  target_compile_options(sdl2 INTERFACE -sUSE_SDL=2)
  target_link_options(sdl2 INTERFACE -sUSE_SDL=2)
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
  target_link_libraries(sdl2 INTERFACE PkgConfig::SDL2)
endif()

# Pure C, no SDL: ROM and asset loading, framebuffer drawing (the sim joins here).
add_library(joust_core STATIC
  src/assets/inflate.c
  src/assets/png.c
  src/assets/zip.c
  src/assets/rom.c
  src/assets/assets.c
  src/assets/images.c
  src/assets/savefile.c
  src/gfx/fb.c
  src/gfx/blit.c
  src/gfx/scale.c
  src/gfx/enhance.c
  src/gfx/debugfont.c
  src/gfx/williams.c
  src/sim/prng.c
  src/sim/game.c
  src/sim/tasks.c
  src/sim/checksum.c
  src/sim/lava.c
  src/sim/collide.c
  src/sim/scores.c
  src/sim/flow.c
  src/sim/display.c
  src/sim/rider.c
  src/sim/platform.c
  src/sim/brains.c
  src/sim/wave.c
  src/sim/eggs.c
  src/sim/spawn.c
  src/sim/score.c
  src/sim/death.c
  src/sim/ptero.c
  src/sim/points.c
  src/sim/text.c
  src/emu/m6809.c
  src/emu/pia6821.c
  src/emu/board.c
  src/emu/m6800.c
  src/emu/sound.c
)
target_include_directories(joust_core PUBLIC src)
target_link_libraries(joust_core PUBLIC m)
# Development builds also find assets/ in the source tree; releases ship it next to the program.
option(JOUST_DEV_ASSETS "Fall back to the source tree's assets/ at run time" ON)
if(JOUST_DEV_ASSETS)
  target_compile_definitions(joust_core PRIVATE JOUST_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
endif()

add_executable(joust
  src/main.c
  src/platform/video.c
  src/platform/input.c
  src/platform/audio.c
)
target_link_libraries(joust PRIVATE joust_core sdl2 m)

# The user's ROM set (never committed; see .gitignore): roms/joust.zip or roms/joust/.
set(rom_default "${CMAKE_SOURCE_DIR}/roms/joust.zip")
if(NOT EXISTS "${rom_default}" AND EXISTS "${CMAKE_SOURCE_DIR}/roms/joust")
  set(rom_default "${CMAKE_SOURCE_DIR}/roms/joust")
endif()
set(JOUST_ROM "${rom_default}" CACHE FILEPATH "The MAME joust ROM set: joust.zip or a folder")

# Web build: emcmake cmake -S . -B build-web && cmake --build build-web --target serve
# writes <build>/web/index.{html,js,wasm} and serves it on localhost. The ROM
# set is never bundled; JOUST_WEB_ROM can add the user's own for local play
# only (do not publish such a build).
if(EMSCRIPTEN)
  set(JOUST_WEB_ROM "" CACHE FILEPATH "Bundle this joust.zip for LOCAL testing only")
  set(JOUST_WEB_PORT 8000 CACHE STRING "Port for the serve target")
  set(web_preload)
  if(JOUST_WEB_ROM)
    list(APPEND web_preload "SHELL:--preload-file '${JOUST_WEB_ROM}@/joust.zip'")
    message(WARNING "Web build bundles ${JOUST_WEB_ROM}: for local use only, do not publish it")
  endif()
  if(EXISTS "${CMAKE_SOURCE_DIR}/assets")
    list(APPEND web_preload "SHELL:--preload-file '${CMAKE_SOURCE_DIR}/assets@/assets'")
  endif()
  set(shell "${CMAKE_SOURCE_DIR}/src/web/shell.html")
  set_target_properties(joust PROPERTIES
    OUTPUT_NAME index SUFFIX .html
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/web"
    LINK_DEPENDS "${shell}")
  target_link_options(joust PRIVATE -sALLOW_MEMORY_GROWTH=1 "SHELL:--shell-file '${shell}'" ${web_preload})

  find_program(PYTHON3_EXECUTABLE NAMES python3 python)
  add_custom_target(serve
    COMMAND ${CMAKE_COMMAND} -E echo "Joust: http://localhost:${JOUST_WEB_PORT}/"
    COMMAND ${PYTHON3_EXECUTABLE} -m http.server ${JOUST_WEB_PORT} --bind 127.0.0.1
      --directory "${CMAKE_BINARY_DIR}/web"
    DEPENDS joust USES_TERMINAL)
  return()  # no packages or tests for the web build
endif()

# Release archives: cmake --build build --target package_linux / package_portmaster /
# package_windows (configure release builds with -DJOUST_DEV_ASSETS=OFF)
foreach(kind linux portmaster windows)
  add_custom_target(package_${kind}
    COMMAND ${CMAKE_COMMAND} -DKIND=${kind} -DSRC=${CMAKE_SOURCE_DIR} -DBIN=${CMAKE_BINARY_DIR}
      -DGAME=$<TARGET_FILE:joust> -DARCH=${CMAKE_SYSTEM_PROCESSOR} -DVERSION=${PROJECT_VERSION}
      -P ${CMAKE_SOURCE_DIR}/src/packaging/package.cmake
    DEPENDS joust
    COMMENT "Packaging ${kind} release")
endforeach()

enable_testing()
add_executable(test_enhance src/tests/test_enhance.c)
target_link_libraries(test_enhance PRIVATE joust_core)
add_test(NAME enhance COMMAND test_enhance)
add_executable(test_capture src/tests/test_capture.c)
target_link_libraries(test_capture PRIVATE joust_core)
add_executable(test_images src/tests/test_images.c)
target_link_libraries(test_images PRIVATE joust_core)
add_executable(test_prng src/tests/test_prng.c)
target_link_libraries(test_prng PRIVATE joust_core)
add_executable(test_emu src/tests/test_emu.c)
target_link_libraries(test_emu PRIVATE joust_core)

if(EXISTS "${JOUST_ROM}")
  add_executable(test_rom src/tests/test_rom.c)
  target_link_libraries(test_rom PRIVATE joust_core)
  add_test(NAME rom_load COMMAND test_rom "${JOUST_ROM}" "${CMAKE_BINARY_DIR}/rom_test")
  set_tests_properties(rom_load PROPERTIES LABELS rom)
  add_executable(test_game src/tests/test_game.c)
  target_link_libraries(test_game PRIVATE joust_core)
  add_test(NAME game COMMAND test_game "${JOUST_ROM}")
  add_test(NAME game_runner COMMAND test_game "${JOUST_ROM}" runner)
  add_test(NAME game_two COMMAND test_game "${JOUST_ROM}" two)
  set_tests_properties(game game_runner game_two PROPERTIES LABELS rom TIMEOUT 600)
  add_executable(test_routines src/tests/test_routines.c)
  target_link_libraries(test_routines PRIVATE joust_core)
  add_test(NAME routines COMMAND test_routines "${JOUST_ROM}")
  set_tests_properties(routines PROPERTIES LABELS rom)
  message(STATUS "ROM set found at ${JOUST_ROM}: ROM tests enabled")
else()
  message(STATUS "No ROM set at ${JOUST_ROM}: ROM tests disabled (put joust.zip in roms/)")
endif()

# MAME as the reference: its view of the CPU's address space must match ours.
find_program(MAME_EXECUTABLE mame)
if(MAME_EXECUTABLE AND EXISTS "${JOUST_ROM}")
  get_filename_component(rom_parent "${JOUST_ROM}" DIRECTORY)
  add_test(NAME mame_rom_map
    COMMAND ${CMAKE_COMMAND} -DMAME=${MAME_EXECUTABLE} -DROMPATH=${rom_parent} -DGAME=$<TARGET_FILE:joust>
      -DROM=${JOUST_ROM} -DWORKDIR=${CMAKE_BINARY_DIR}/mame
      -DPROBE=${CMAKE_SOURCE_DIR}/src/tests/mame/rom_map_probe.lua
      -P ${CMAKE_SOURCE_DIR}/src/tests/mame/rom_map.cmake)
  set_tests_properties(mame_rom_map PROPERTIES LABELS mame TIMEOUT 300)
  add_test(NAME mame_capture
    COMMAND ${CMAKE_COMMAND} -DMAME=${MAME_EXECUTABLE} -DROMPATH=${rom_parent} -DWORKDIR=${CMAKE_BINARY_DIR}/mame
      -DPROBE=${CMAKE_SOURCE_DIR}/src/tests/mame/capture_probe.lua -DCHECK=$<TARGET_FILE:test_capture>
      -P ${CMAKE_SOURCE_DIR}/src/tests/mame/capture.cmake)
  add_test(NAME mame_images
    COMMAND ${CMAKE_COMMAND} -DMAME=${MAME_EXECUTABLE} -DROMPATH=${rom_parent} -DWORKDIR=${CMAKE_BINARY_DIR}/mame
      -DGAME=$<TARGET_FILE:joust> -DROM=${JOUST_ROM}
      -DPROBE=${CMAKE_SOURCE_DIR}/src/tests/mame/image_probe.lua -DCHECK=$<TARGET_FILE:test_images>
      -P ${CMAKE_SOURCE_DIR}/src/tests/mame/images.cmake)
  add_test(NAME mame_prng
    COMMAND ${CMAKE_COMMAND} -DMAME=${MAME_EXECUTABLE} -DROMPATH=${rom_parent} -DWORKDIR=${CMAKE_BINARY_DIR}/mame
      -DPROBE=${CMAKE_SOURCE_DIR}/src/tests/mame/prng_probe.lua -DCHECK=$<TARGET_FILE:test_prng>
      -P ${CMAKE_SOURCE_DIR}/src/tests/mame/prng.cmake)
  add_test(NAME mame_emu
    COMMAND ${CMAKE_COMMAND} -DWORKDIR=${CMAKE_BINARY_DIR}/mame -DROM=${JOUST_ROM} -DCHECK=$<TARGET_FILE:test_emu>
      -P ${CMAKE_SOURCE_DIR}/src/tests/mame/emu.cmake)
  set_tests_properties(mame_capture mame_images mame_prng mame_emu PROPERTIES LABELS mame TIMEOUT 300 RESOURCE_LOCK mame_workdir)
  set_tests_properties(mame_emu PROPERTIES DEPENDS "mame_prng;mame_capture")
  set_tests_properties(mame_rom_map PROPERTIES RESOURCE_LOCK mame_workdir)
  message(STATUS "MAME reference tests enabled (${MAME_EXECUTABLE})")
else()
  message(STATUS "MAME reference tests disabled (need mame and ${JOUST_ROM})")
endif()

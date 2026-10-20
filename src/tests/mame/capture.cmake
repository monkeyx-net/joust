# Video RAM and palette decoding against MAME's own frames.
#   cmake -DMAME=... -DROMPATH=... -DWORKDIR=... -DPROBE=capture_probe.lua -DCHECK=test_capture -P capture.cmake
# A scripted run (coin, start) is captured at the wave-1 intro and two
# early-game frames (buzzards, lava, the player). They were picked because
# nothing moves during their scan-out, so the end-of-frame RAM dump and the
# snapshot hold the same picture; frames where a sprite moves mid-scan
# differ by a few hundred pixels and are not useful here.
foreach(v MAME ROMPATH WORKDIR PROBE CHECK)
  if(NOT DEFINED ${v})
    message(FATAL_ERROR "capture.cmake: ${v} not set")
  endif()
endforeach()
include("${CMAKE_CURRENT_LIST_DIR}/mame_common.cmake")

set(frames 1500 1650 1800)
set(out "${WORKDIR}/capture")
file(REMOVE_RECURSE "${out}" "${WORKDIR}/snap")
file(MAKE_DIRECTORY "${out}")
set(ENV{PROBE_OUT} "${out}")
string(REPLACE ";" " " frame_list "${frames}")
set(ENV{PROBE_FRAMES} "${frame_list}")
set(ENV{PROBE_LAST} 1810)
set(ENV{PROBE_SCRIPT} "1200:coin 1215: 1300:start 1315:")
execute_process(COMMAND "${MAME}" ${mame_args} -seconds_to_run 120 -autoboot_script "${PROBE}"
                RESULT_VARIABLE rc OUTPUT_QUIET ERROR_QUIET)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "MAME exited with ${rc}")
endif()

set(i 0)
foreach(f ${frames})
  set(snap "${WORKDIR}/snap/joust/000${i}.png")
  execute_process(COMMAND "${CHECK}" "${out}/frame_${f}.txt" "${snap}" RESULT_VARIABLE rc)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "frame ${f}: the port's decoding differs from MAME's snapshot")
  endif()
  math(EXPR i "${i} + 1")
endforeach()

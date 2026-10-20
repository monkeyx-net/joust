# The image catalogue through MAME's blitter against the port's.
#   cmake -DMAME=... -DROMPATH=... -DWORKDIR=... -DGAME=joust -DROM=<set>
#         -DPROBE=image_probe.lua -DCHECK=test_images -P images.cmake
foreach(v MAME ROMPATH WORKDIR GAME ROM PROBE CHECK)
  if(NOT DEFINED ${v})
    message(FATAL_ERROR "images.cmake: ${v} not set")
  endif()
endforeach()
include("${CMAKE_CURRENT_LIST_DIR}/mame_common.cmake")

set(list "${WORKDIR}/images.txt")
set(out "${WORKDIR}/image_probe.txt")
file(REMOVE "${out}")
execute_process(COMMAND "${GAME}" --list-images OUTPUT_FILE "${list}" RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "joust --list-images exited with ${rc}")
endif()
set(ENV{PROBE_OUT} "${out}")
set(ENV{PROBE_IMAGES} "${list}")
set(ENV{PROBE_AT} 600)
execute_process(COMMAND "${MAME}" ${mame_args} -seconds_to_run 60 -autoboot_script "${PROBE}"
                RESULT_VARIABLE rc OUTPUT_QUIET ERROR_QUIET)
if(NOT rc EQUAL 0 OR NOT EXISTS "${out}")
  message(FATAL_ERROR "MAME exited with ${rc} and no probe output")
endif()
execute_process(COMMAND "${CHECK}" "${ROM}" "${out}" RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "the port's blitter differs from MAME's")
endif()

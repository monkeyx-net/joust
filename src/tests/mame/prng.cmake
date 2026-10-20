# The random number generator against MAME, transition by transition.
#   cmake -DMAME=... -DROMPATH=... -DWORKDIR=... -DPROBE=prng_probe.lua -DCHECK=test_prng -P prng.cmake
# A scripted game (coin, start) runs long enough for the game's own steps,
# not only the interrupt's stirs.
foreach(v MAME ROMPATH WORKDIR PROBE CHECK)
  if(NOT DEFINED ${v})
    message(FATAL_ERROR "prng.cmake: ${v} not set")
  endif()
endforeach()
include("${CMAKE_CURRENT_LIST_DIR}/mame_common.cmake")

set(out "${WORKDIR}/prng.txt")
file(REMOVE "${out}")
set(ENV{PROBE_OUT} "${out}")
set(ENV{PROBE_COUNT} 20000)
set(ENV{PROBE_SCRIPT} "1200:coin 1215: 1300:start 1315:")
execute_process(COMMAND "${MAME}" ${mame_args} -seconds_to_run 300 -autoboot_script "${PROBE}"
                RESULT_VARIABLE rc OUTPUT_QUIET ERROR_QUIET)
if(NOT rc EQUAL 0 OR NOT EXISTS "${out}")
  message(FATAL_ERROR "MAME exited with ${rc} and no probe output")
endif()
execute_process(COMMAND "${CHECK}" "${out}" RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "sim/prng.c differs from MAME")
endif()

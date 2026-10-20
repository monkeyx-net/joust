# The emulated board against MAME: runs after mame_prng and mame_capture and
# uses what they wrote (the random-state trace, the primed CMOS, snapshots).
#   cmake -DWORKDIR=... -DROM=... -DCHECK=test_emu -P emu.cmake
foreach(v WORKDIR ROM CHECK)
  if(NOT DEFINED ${v})
    message(FATAL_ERROR "emu.cmake: ${v} not set")
  endif()
endforeach()
foreach(f prng.txt nvram_primed/joust/nvram snap/joust/0000.png snap/joust/0001.png snap/joust/0002.png)
  if(NOT EXISTS "${WORKDIR}/${f}")
    message(FATAL_ERROR "${WORKDIR}/${f} missing: run mame_prng and mame_capture first")
  endif()
endforeach()
# snapshots 0000-0002 are mame_capture's frames 1500, 1650, 1800
execute_process(COMMAND "${CHECK}" "${ROM}" "${WORKDIR}/nvram_primed/joust/nvram" "${WORKDIR}/prng.txt"
                  "1500:${WORKDIR}/snap/joust/0000.png" "1650:${WORKDIR}/snap/joust/0001.png"
                  "1800:${WORKDIR}/snap/joust/0002.png"
                RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "the emulated board differs from MAME")
endif()

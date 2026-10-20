# Shared by the MAME tests: the options and NVRAM priming.
# Needs MAME, ROMPATH (the dir holding joust.zip or joust/) and WORKDIR.
#
# With no NVRAM the arcade boots into FACTORY SETTINGS RESTORED and stays
# there, so one 30 s run writes the CMOS first. MAME saves the CMOS again at
# every exit (coin counters, audits, high scores), which would make each run
# start from a different state; every run therefore gets a fresh copy of the
# primed CMOS.
file(MAKE_DIRECTORY "${WORKDIR}")
set(primed "${WORKDIR}/nvram_primed")
set(common_args joust -rompath "${ROMPATH}" -cfg_directory "${WORKDIR}/cfg"
    -snapshot_directory "${WORKDIR}/snap" -snapview native
    # every frame rendered, so a snapshot shows the frame it is taken in
    -video none -sound none -nothrottle -frameskip 0 -noplugin data -skip_gameinfo)
if(NOT EXISTS "${primed}/joust/nvram")
  execute_process(COMMAND "${MAME}" ${common_args} -nvram_directory "${primed}" -seconds_to_run 30
                  RESULT_VARIABLE prime_rc OUTPUT_QUIET ERROR_QUIET)
  if(NOT prime_rc EQUAL 0 OR NOT EXISTS "${primed}/joust/nvram")
    message(FATAL_ERROR "MAME priming run failed (${prime_rc}); no NVRAM written")
  endif()
endif()
file(REMOVE_RECURSE "${WORKDIR}/nvram")
file(COPY "${primed}/" DESTINATION "${WORKDIR}/nvram")
set(mame_args ${common_args} -nvram_directory "${WORKDIR}/nvram")

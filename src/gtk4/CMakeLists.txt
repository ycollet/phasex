# Real, toolkit-agnostic backend files (session/bank/patch/param data
# layer), linked in for the navbar-backend-wiring pass.
set(phasex_gtk4_backend_SOURCES
  ${CMAKE_SOURCE_DIR}/src/bank.c
  ${CMAKE_SOURCE_DIR}/src/session.c
  ${CMAKE_SOURCE_DIR}/src/patch.c
  ${CMAKE_SOURCE_DIR}/src/param.c
  ${CMAKE_SOURCE_DIR}/src/param_parse.c
  ${CMAKE_SOURCE_DIR}/src/param_strings.c
  ${CMAKE_SOURCE_DIR}/src/param_cb.c
  ${CMAKE_SOURCE_DIR}/src/string_util.c
  ${CMAKE_SOURCE_DIR}/src/debug.c
  ${CMAKE_SOURCE_DIR}/src/wave.c
  ${CMAKE_SOURCE_DIR}/src/filter.c
  ${CMAKE_SOURCE_DIR}/src/engine.c
  ${CMAKE_SOURCE_DIR}/src/buffer.c
  ${CMAKE_SOURCE_DIR}/src/bpm.c
  ${CMAKE_SOURCE_DIR}/src/timekeeping.c
  ${CMAKE_SOURCE_DIR}/src/midi_event.c
  ${CMAKE_SOURCE_DIR}/src/midi_process.c)

# Real JACK audio/MIDI driver files -- see audio_init.c for why these
# (unlike settings.c/midimap.c/phasex.c) are safe to link wholesale:
# none of them touch GTK2.
set(phasex_gtk4_jack_SOURCES
  ${CMAKE_SOURCE_DIR}/src/driver.c
  ${CMAKE_SOURCE_DIR}/src/jack.c
  ${CMAKE_SOURCE_DIR}/src/jack_midi.c
  ${CMAKE_SOURCE_DIR}/src/jack_transport.c)

add_executable(phasex-gtk4-preview
  main.c
  gtkknob.c gtkknob.h
  navbar.c navbar.h
  menubar.c menubar.h
  device_enum.c device_enum.h
  paramgroup.c paramgroup.h
  param_groups_data.c
  gui_debug4.c gui_debug4.h
  backend_init.c backend_init.h
  audio_init.c audio_init.h
  ${phasex_gtk4_backend_SOURCES}
  ${phasex_gtk4_jack_SOURCES})

target_include_directories(phasex-gtk4-preview PRIVATE
  ${CMAKE_BINARY_DIR}
  ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(phasex-gtk4-preview PRIVATE
  PkgConfig::GTK4
  PkgConfig::GLIB
  PkgConfig::ALSA
  PkgConfig::JACK
  PkgConfig::SAMPLERATE
  Threads::Threads)

# Force-included (not #include'd from any shared source) so the GTK2-type
# shim never needs a source-file change -- see gtk2_types_shim.h.
target_compile_options(phasex-gtk4-preview PRIVATE
  -include ${CMAKE_CURRENT_SOURCE_DIR}/gtk2_types_shim.h)

if(TARGET PkgConfig::GMODULE)
  target_link_libraries(phasex-gtk4-preview PRIVATE PkgConfig::GMODULE)
endif()

if(M_LIBRARY)
  target_link_libraries(phasex-gtk4-preview PRIVATE ${M_LIBRARY})
endif()

if(HAVE_LIBRT AND RT_LIBRARY)
  target_link_libraries(phasex-gtk4-preview PRIVATE ${RT_LIBRARY})
endif()

if(APPLE)
  target_compile_definitions(phasex-gtk4-preview PRIVATE _DARWIN_C_SOURCE)
endif()

target_compile_definitions(phasex-gtk4-preview PRIVATE
  _GNU_SOURCE
  _XOPEN_SOURCE=600
  _REENTRANT
  PHASEX_CPU_POWER=${PHASEX_CPU_POWER}
  NUM_PARTS=${PHASEX_NUM_PARTS}
  PHASEX_GTK4_CSS_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
  PHASEX_GTK4_PIXMAP_DIR="${CMAKE_SOURCE_DIR}/pixmaps"
  PHASEX_GTK4_REPO_DIR="${CMAKE_SOURCE_DIR}")

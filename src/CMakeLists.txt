set(phasex_SOURCES
  alsa_pcm.c alsa_pcm.h
  alsa_seq.c alsa_seq.h
  bank.c bank.h
  bpm.c bpm.h
  buffer.c buffer.h
  debug.c debug.h
  driver.c driver.h
  engine.c engine.h
  filter.c filter.h
  gtkknob.c gtkknob.h
  gui_alsa.c gui_alsa.h
  gui_bank.c gui_bank.h
  gui_jack.c gui_jack.h
  gui_layout.c gui_layout.h
  gui_main.c gui_main.h
  gui_menubar.c gui_menubar.h
  gui_midimap.c gui_midimap.h
  gui_navbar.c gui_navbar.h
  gui_param.c gui_param.h
  gui_patch.c gui_patch.h
  gui_session.c gui_session.h
  help.c help.h
  jack.c jack.h
  jack_midi.c jack_midi.h
  jack_transport.c jack_transport.h
  mididefs.h
  midi_event.c midi_event.h
  midimap.c midimap.h
  midi_process.c midi_process.h
  nsm.c nsm.h
  param.c param.h
  param_cb.c param_cb.h
  param_parse.c param_parse.h
  param_strings.c param_strings.h
  patch.c patch.h
  phasex.c phasex.h
  rawmidi.c rawmidi.h
  session.c session.h
  settings.c settings.h
  string_util.c string_util.h
  timekeeping.c timekeeping.h
  wave.c wave.h)

add_executable(phasex ${phasex_SOURCES})

target_include_directories(phasex PRIVATE ${CMAKE_BINARY_DIR})

target_compile_definitions(phasex PRIVATE
  _GNU_SOURCE
  _XOPEN_SOURCE=600
  _REENTRANT
  ARCH_BITS=${PHASEX_ARCH_BITS}
  PHASEX_CPU_POWER=${PHASEX_CPU_POWER}
  NUM_PARTS=${PHASEX_NUM_PARTS})

target_link_libraries(phasex PRIVATE
  PkgConfig::GLIB
  PkgConfig::GTK
  PkgConfig::ALSA
  PkgConfig::JACK
  PkgConfig::SAMPLERATE
  Threads::Threads
  X11::X11)

if(TARGET PkgConfig::GMODULE)
  target_link_libraries(phasex PRIVATE PkgConfig::GMODULE)
endif()

if(M_LIBRARY)
  target_link_libraries(phasex PRIVATE ${M_LIBRARY})
endif()

if(HAVE_LIBRT AND RT_LIBRARY)
  target_link_libraries(phasex PRIVATE ${RT_LIBRARY})
endif()

if(HAVE_LIBLO)
  target_link_libraries(phasex PRIVATE PkgConfig::LIBLO)
endif()

install(TARGETS phasex RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable(phasex-gtk4-preview
  main.c
  gtkknob.c gtkknob.h
  navbar.c navbar.h
  paramgroup.c paramgroup.h
  gui_debug4.c gui_debug4.h)

target_link_libraries(phasex-gtk4-preview PRIVATE PkgConfig::GTK4)

if(M_LIBRARY)
  target_link_libraries(phasex-gtk4-preview PRIVATE ${M_LIBRARY})
endif()

target_compile_definitions(phasex-gtk4-preview PRIVATE
  PHASEX_GTK4_CSS_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
  PHASEX_GTK4_PIXMAP_DIR="${CMAKE_SOURCE_DIR}/pixmaps")

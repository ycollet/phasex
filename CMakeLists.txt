cmake_minimum_required(VERSION 3.13)
project(phasex LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

include(GNUInstallDirs)
include(CheckIncludeFile)
include(CheckSymbolExists)
include(CheckCCompilerFlag)

# ----------------------------------------------------------------------------
# Version: prefer `git describe`, otherwise fall back to the phasex-version
# file that is checked into the tree.
# ----------------------------------------------------------------------------
find_package(Git QUIET)
set(PACKAGE_VERSION "")
if(GIT_FOUND)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --tags
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE PACKAGE_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE GIT_DESCRIBE_RESULT)
  if(NOT GIT_DESCRIBE_RESULT EQUAL 0)
    set(PACKAGE_VERSION "")
  endif()
endif()
if(NOT PACKAGE_VERSION)
  file(READ "${CMAKE_SOURCE_DIR}/phasex-version" PACKAGE_VERSION)
  string(STRIP "${PACKAGE_VERSION}" PACKAGE_VERSION)
endif()
message(STATUS "PHASEX version: ${PACKAGE_VERSION}")

# ----------------------------------------------------------------------------
# Options
# ----------------------------------------------------------------------------
option(PHASEX_WITH_LIBRT "Link against librt if needed for clock_gettime/clock_nanosleep" ON)
option(PHASEX_WITH_NSM "Build with New/Non Session Manager (NSM) support" ON)
option(PHASEX_NATIVE_ARCH "Build with -march=native -mtune=native" OFF)
option(PHASEX_GUI_DEBUG "Dump every GUI widget's position, size, and colors to the console at startup (for comparing GTK2 vs. a future GTK4 port)" OFF)

set(PHASEX_NUM_PARTS "1" CACHE STRING "Number of active synth parts to build with (1-12)")
set(PHASEX_CPU_POWER "2" CACHE STRING "Optimize sound quality for CPU power: 1 (low) - 4 (over-powered)")

if(PHASEX_NUM_PARTS LESS 1)
  set(PHASEX_NUM_PARTS 1)
elseif(PHASEX_NUM_PARTS GREATER 12)
  set(PHASEX_NUM_PARTS 12)
endif()

# ----------------------------------------------------------------------------
# Dependencies
# ----------------------------------------------------------------------------
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(X11 REQUIRED)

pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET glib-2.0>=2.12)
pkg_check_modules(GTK REQUIRED IMPORTED_TARGET gtk+-2.0>=2.4)
pkg_check_modules(ALSA REQUIRED IMPORTED_TARGET alsa>=0.9.0)
pkg_check_modules(JACK REQUIRED IMPORTED_TARGET jack>=0.99.0)
pkg_check_modules(SAMPLERATE REQUIRED IMPORTED_TARGET samplerate>=0.1.2)

# -lgmodule was folded into GTK_LIBRARIES on some platforms; only pull in the
# separate gmodule pkg-config module when it is missing.
string(FIND "${GTK_LIBRARIES}" "gmodule" _gmodule_in_gtk)
if(_gmodule_in_gtk EQUAL -1)
  pkg_check_modules(GMODULE REQUIRED IMPORTED_TARGET gmodule-2.0>=2.12)
endif()

find_library(M_LIBRARY m)

set(HAVE_LIBLO OFF)
if(PHASEX_WITH_NSM)
  pkg_check_modules(LIBLO IMPORTED_TARGET liblo)
  if(LIBLO_FOUND)
    set(HAVE_LIBLO ON)
  else()
    message(WARNING "liblo not found; building without NSM support")
    set(PHASEX_WITH_NSM OFF)
  endif()
endif()

# librt: only needed on systems where clock_gettime()/clock_nanosleep() do not
# already live in libc.
set(HAVE_LIBRT OFF)
set(RT_LIBRARY "")
if(PHASEX_WITH_LIBRT)
  check_symbol_exists(clock_gettime "time.h" PHASEX_HAVE_CLOCK_GETTIME_IN_LIBC)
  if(NOT PHASEX_HAVE_CLOCK_GETTIME_IN_LIBC)
    find_library(RT_LIBRARY rt)
    if(RT_LIBRARY)
      set(HAVE_LIBRT ON)
    endif()
  endif()
endif()

set(CMAKE_REQUIRED_LIBRARIES ${RT_LIBRARY})
check_symbol_exists(clock_gettime "time.h" HAVE_CLOCK_GETTIME)
check_symbol_exists(clock_nanosleep "time.h" HAVE_CLOCK_NANOSLEEP)
unset(CMAKE_REQUIRED_LIBRARIES)

set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})
check_symbol_exists(pthread_mutexattr_setprotocol "pthread.h" HAVE_PTHREAD_MUTEXATTR_SETPROTOCOL)
unset(CMAKE_REQUIRED_LIBRARIES)

set(CMAKE_REQUIRED_INCLUDES ${JACK_INCLUDE_DIRS})
check_include_file(jack/weakjack.h HAVE_JACK_WEAKJACK_H)

string(REPLACE ";" " " _jack_cflags_other "${JACK_CFLAGS_OTHER}")
set(CMAKE_REQUIRED_LIBRARIES ${JACK_LIBRARIES})
set(CMAKE_REQUIRED_FLAGS "${_jack_cflags_other}")
check_symbol_exists(jack_set_port_rename_callback "jack/jack.h" HAVE_JACK_SET_PORT_RENAME_CALLBACK)
check_symbol_exists(jack_set_latency_callback "jack/jack.h" HAVE_JACK_SET_LATENCY_CALLBACK)
unset(CMAKE_REQUIRED_LIBRARIES)
unset(CMAKE_REQUIRED_INCLUDES)
unset(CMAKE_REQUIRED_FLAGS)

# GTK engine directory, mirroring what configure.ac derives from pkg-config.
execute_process(
  COMMAND ${PKG_CONFIG_EXECUTABLE} --variable=libdir gtk+-2.0
  OUTPUT_VARIABLE GTK_LIB_DIR
  OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(
  COMMAND ${PKG_CONFIG_EXECUTABLE} --variable=gtk_binary_version gtk+-2.0
  OUTPUT_VARIABLE GTK_BINARY_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE)
set(GTK_ENGINE_DIR "${GTK_LIB_DIR}/gtk-2.0/${GTK_BINARY_VERSION}/engines")

# ----------------------------------------------------------------------------
# Install locations / path macros baked into config.h
# ----------------------------------------------------------------------------
set(CONFIG_PREFIX "${CMAKE_INSTALL_PREFIX}")
set(PHASEX_DATADIR "${CMAKE_INSTALL_FULL_DATADIR}")
set(PACKAGE_LOCALE_DIR "${PHASEX_DATADIR}/locale")
set(PHASEX_PKGDATADIR "${PHASEX_DATADIR}/phasex")

set(PHASEX_LICENSE_FILE  "${PHASEX_DATADIR}/doc/phasex/LICENSE")
set(ALT_LICENSE_FILE     "${PHASEX_DATADIR}/doc/phasex-${PACKAGE_VERSION}/LICENSE")
set(GPLv3_LICENSE_FILE   "${PHASEX_DATADIR}/doc/phasex/GPL-3.0.txt")
set(ALT_GPLv3_FILE       "${PHASEX_DATADIR}/doc/phasex-${PACKAGE_VERSION}/GPL-3.0.txt")
set(PHASEX_DIR           "${PHASEX_PKGDATADIR}")
set(PIXMAP_DIR           "${PHASEX_PKGDATADIR}/pixmaps")
set(PATCH_DIR            "${PHASEX_PKGDATADIR}/sys-patches")
set(MIDIMAP_DIR          "${PHASEX_PKGDATADIR}/sys-midimaps")
set(SAMPLE_DIR           "${PHASEX_PKGDATADIR}/sys-samples")
set(PARAM_HELPFILE       "${PHASEX_PKGDATADIR}/help/parameters.help")
set(PHASEX_GTK_ENGINE_RC "${PHASEX_PKGDATADIR}/gtkenginerc")
set(GTKRC_DARK           "${PHASEX_DATADIR}/themes/Phasex-Dark/gtk-2.0/gtkrc")
set(GTKRC_LIGHT          "${PHASEX_DATADIR}/themes/Phasex-Light/gtk-2.0/gtkrc")
set(PACKAGE_NAME         "PHASEX")

configure_file(
  "${CMAKE_SOURCE_DIR}/cmake/config.h.cmake.in"
  "${CMAKE_BINARY_DIR}/config.h"
  @ONLY)

# ----------------------------------------------------------------------------
# Compiler flags
# ----------------------------------------------------------------------------
add_compile_options(-pipe -Wall)

if(PHASEX_NATIVE_ARCH)
  check_c_compiler_flag(-march=native HAVE_MARCH_NATIVE)
  if(HAVE_MARCH_NATIVE)
    add_compile_options(-march=native -mtune=native)
  endif()
else()
  message(STATUS "PHASEX_NATIVE_ARCH is off; pass -DPHASEX_NATIVE_ARCH=ON to tune for this machine")
endif()

add_compile_options(
  "$<$<CONFIG:Release>:-O3;-ffast-math;-fomit-frame-pointer;-fsingle-precision-constant>"
  "$<$<CONFIG:RelWithDebInfo>:-O2;-ffast-math;-fsingle-precision-constant>"
  "$<$<CONFIG:Debug>:-Wextra;-Wcast-align;-Wconversion;-Werror=format-security;-ffast-math;-fsingle-precision-constant>")

math(EXPR PHASEX_ARCH_BITS "${CMAKE_SIZEOF_VOID_P} * 8")
if(NOT PHASEX_ARCH_BITS EQUAL 64)
  message(FATAL_ERROR
    "PHASEX requires a 64-bit target (got ${PHASEX_ARCH_BITS}-bit). "
    "The atomic MIDI-timestamp code in timekeeping.c relies on a "
    "pointer-sized (8-byte) atomic swap and no longer has a 32-bit "
    "fallback path.")
endif()

add_subdirectory(src)

# ----------------------------------------------------------------------------
# Data files
# ----------------------------------------------------------------------------
install(FILES
    README LICENSE GPL-3.0.txt AUTHORS ChangeLog TODO
    doc/ROADMAP doc/signal-diagram.txt doc/release-checklist.txt
  DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/phasex)

install(FILES help/parameters.help
  DESTINATION ${CMAKE_INSTALL_DATADIR}/phasex/help)

install(DIRECTORY pixmaps/
  DESTINATION ${CMAKE_INSTALL_DATADIR}/phasex/pixmaps
  FILES_MATCHING PATTERN "*.png" PATTERN "*.svg")
install(FILES pixmaps/phasex-icon-256x256.png
  DESTINATION ${CMAKE_INSTALL_DATADIR}/phasex/pixmaps
  RENAME phasex-icon.png)
install(FILES pixmaps/phasex-icon-256x256.png
  DESTINATION ${CMAKE_INSTALL_DATADIR}/pixmaps
  RENAME phasex-icon.png)

install(FILES themes/gtkenginerc
  DESTINATION ${CMAKE_INSTALL_DATADIR}/phasex)
install(FILES themes/Phasex-Dark
  DESTINATION ${CMAKE_INSTALL_DATADIR}/themes/Phasex-Dark/gtk-2.0
  RENAME gtkrc)
install(FILES themes/Phasex-Light
  DESTINATION ${CMAKE_INSTALL_DATADIR}/themes/Phasex-Light/gtk-2.0
  RENAME gtkrc)

install(DIRECTORY patches/
  DESTINATION ${CMAKE_INSTALL_DATADIR}/phasex/sys-patches
  FILES_MATCHING PATTERN "*.phx")

install(DIRECTORY samples/
  DESTINATION ${CMAKE_INSTALL_DATADIR}/phasex/sys-samples
  FILES_MATCHING PATTERN "*.raw")

install(FILES midimaps/default.map
  DESTINATION ${CMAKE_INSTALL_DATADIR}/phasex/sys-midimaps)

install(FILES misc/patchbank
  DESTINATION ${CMAKE_INSTALL_DATADIR}/phasex)
install(PROGRAMS misc/phasex-convert-patch
  DESTINATION ${CMAKE_INSTALL_BINDIR})

set(ac_datadir "${PHASEX_DATADIR}")
configure_file(
  "${CMAKE_SOURCE_DIR}/misc/phasex.desktop.in"
  "${CMAKE_BINARY_DIR}/phasex.desktop"
  @ONLY)
install(FILES "${CMAKE_BINARY_DIR}/phasex.desktop"
  DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)

configure_file(
  "${CMAKE_SOURCE_DIR}/phasex.spec.in"
  "${CMAKE_BINARY_DIR}/phasex.spec"
  @ONLY)

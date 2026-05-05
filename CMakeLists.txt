cmake_minimum_required(VERSION 3.25)

project(
  AetherisIUI
  VERSION 0.1.0
  DESCRIPTION "Embeddable intent-based UI infrastructure for enterprise systems"
  LANGUAGES C CXX)

include(GNUInstallDirs)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(AetherisCompilerOptions)
include(AetherisTesting)

option(AETHERIS_ENABLE_TESTS "Build Aetheris-IUI tests" ON)
option(AETHERIS_ENABLE_BENCHMARKS "Build Aetheris-IUI benchmarks" OFF)
option(AETHERIS_ENABLE_STATIC_ANALYSIS "Run clang-tidy from CMake targets" OFF)

add_subdirectory(domain)
add_subdirectory(application)
add_subdirectory(infrastructure)
add_subdirectory(interface)
add_subdirectory(tools)

if(AETHERIS_ENABLE_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(AETHERIS_ENABLE_BENCHMARKS)
  add_subdirectory(bench)
endif()

install(
  EXPORT AetherisIUITargets
  NAMESPACE aetheris::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/AetherisIUI")

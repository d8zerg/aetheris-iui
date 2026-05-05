add_library(aetheris_domain STATIC src/domain_anchor.cpp)
add_library(aetheris::domain ALIAS aetheris_domain)

target_include_directories(
  aetheris_domain
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

aetheris_configure_target(aetheris_domain)

set_target_properties(aetheris_domain PROPERTIES EXPORT_NAME domain)

install(
  TARGETS aetheris_domain
  EXPORT AetherisIUITargets
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  INCLUDES
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

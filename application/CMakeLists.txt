add_library(aetheris_application STATIC src/application_anchor.cpp)
add_library(aetheris::application ALIAS aetheris_application)

target_include_directories(
  aetheris_application
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(aetheris_application PUBLIC aetheris::domain)

aetheris_configure_target(aetheris_application)

set_target_properties(aetheris_application PROPERTIES EXPORT_NAME application)

install(
  TARGETS aetheris_application
  EXPORT AetherisIUITargets
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  INCLUDES
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

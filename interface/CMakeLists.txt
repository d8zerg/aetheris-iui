find_package(nlohmann_json REQUIRED)

add_library(aetheris_interface STATIC
  src/c_api.cpp
  src/session_snapshot.cpp)
add_library(aetheris::interface ALIAS aetheris_interface)

target_include_directories(
  aetheris_interface
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(aetheris_interface
  PRIVATE aetheris::application aetheris::infrastructure nlohmann_json::nlohmann_json)
target_compile_definitions(aetheris_interface PRIVATE AETHERIS_IUI_VERSION="${PROJECT_VERSION}")

aetheris_configure_target(aetheris_interface)

set_target_properties(aetheris_interface PROPERTIES EXPORT_NAME interface)

# ── stdio server (IPC bridge for TypeScript integration tests) ──────────────
add_executable(aetheris_stdio_server src/stdio_server.cpp)
target_link_libraries(aetheris_stdio_server PRIVATE aetheris::interface aetheris::application aetheris::infrastructure nlohmann_json::nlohmann_json)
target_compile_definitions(aetheris_stdio_server PRIVATE AETHERIS_IUI_VERSION="${PROJECT_VERSION}")
aetheris_configure_target(aetheris_stdio_server)

install(
  TARGETS aetheris_interface aetheris_stdio_server
  EXPORT AetherisIUITargets
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  INCLUDES
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

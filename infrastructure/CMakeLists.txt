find_package(nlohmann_json REQUIRED)

add_library(
  aetheris_infrastructure STATIC
  src/action_schema_json.cpp
  src/audit_chain.cpp
  src/file_audit_sink.cpp
  src/json_lines_exporter.cpp
  src/llm_intent_classifier.cpp
  src/memory_audit_sink.cpp
  src/noop_telemetry.cpp
  src/schema_slot_extractor.cpp
  src/session_json.cpp
  src/sha256.cpp
  src/system_clock.cpp
  src/uuid_generator.cpp)
add_library(aetheris::infrastructure ALIAS aetheris_infrastructure)

target_include_directories(
  aetheris_infrastructure
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(aetheris_infrastructure PUBLIC aetheris::domain
                                               PRIVATE nlohmann_json::nlohmann_json)

aetheris_configure_target(aetheris_infrastructure)

set_target_properties(aetheris_infrastructure PROPERTIES EXPORT_NAME infrastructure)

install(
  TARGETS aetheris_infrastructure
  EXPORT AetherisIUITargets
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  INCLUDES
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

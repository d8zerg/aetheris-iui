find_package(nlohmann_json REQUIRED)

# ── unified CLI ───────────────────────────────────────────────────────────────
add_executable(aetheris_cli aetheris_cli.cpp)
add_executable(aetheris::cli ALIAS aetheris_cli)
target_link_libraries(aetheris_cli PRIVATE aetheris::application aetheris::infrastructure
                                           nlohmann_json::nlohmann_json)
target_compile_definitions(aetheris_cli PRIVATE AETHERIS_IUI_VERSION="${PROJECT_VERSION}")
aetheris_configure_target(aetheris_cli)
set_target_properties(aetheris_cli PROPERTIES OUTPUT_NAME aetheris)

install(TARGETS aetheris_cli RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

# ── focused tools ─────────────────────────────────────────────────────────────
add_executable(aetheris_schema_lint schema_lint.cpp)
add_executable(aetheris::schema_lint ALIAS aetheris_schema_lint)
target_link_libraries(aetheris_schema_lint PRIVATE aetheris::infrastructure)
aetheris_configure_target(aetheris_schema_lint)
set_target_properties(aetheris_schema_lint PROPERTIES OUTPUT_NAME aetheris-lint)

add_executable(aetheris_audit_verify audit_verify.cpp)
add_executable(aetheris::audit_verify ALIAS aetheris_audit_verify)
target_link_libraries(aetheris_audit_verify PRIVATE aetheris::infrastructure)
aetheris_configure_target(aetheris_audit_verify)
set_target_properties(aetheris_audit_verify PROPERTIES OUTPUT_NAME aetheris-audit)

install(
  TARGETS aetheris_schema_lint aetheris_audit_verify
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

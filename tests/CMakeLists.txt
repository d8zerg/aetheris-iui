add_executable(aetheris_smoke_test smoke/smoke_test.cpp)
target_link_libraries(aetheris_smoke_test PRIVATE aetheris::interface aetheris::infrastructure)
aetheris_configure_target(aetheris_smoke_test)
aetheris_add_ctest(smoke.bootstrap aetheris_smoke_test "smoke")

# ── CLI smoke test ────────────────────────────────────────────────────────────
add_test(NAME smoke.cli COMMAND aetheris_cli --version)
set_tests_properties(smoke.cli PROPERTIES LABELS "smoke")

# ── Sidecar smoke test (eval run on golden dataset) ───────────────────────────
add_test(NAME smoke.eval_harness
  COMMAND aetheris_cli eval run
          "${CMAKE_SOURCE_DIR}/eval/datasets/golden.jsonl")
set_tests_properties(smoke.eval_harness PROPERTIES LABELS "smoke")

# ── Python binding smoke test ─────────────────────────────────────────────────
find_program(PYTHON3_EXEC python3)
if(PYTHON3_EXEC)
  add_test(NAME smoke.python_binding
    COMMAND "${PYTHON3_EXEC}"
            "${CMAKE_SOURCE_DIR}/bindings/python/tests/test_smoke.py"
            --server "$<TARGET_FILE:aetheris_stdio_server>")
  set_tests_properties(smoke.python_binding PROPERTIES LABELS "smoke")
else()
  message(STATUS "python3 not found; smoke.python_binding skipped.")
endif()

# ── Node.js binding smoke test ────────────────────────────────────────────────
find_program(NODE_EXEC node)
if(NODE_EXEC)
  add_test(NAME smoke.node_binding
    COMMAND "${NODE_EXEC}"
            "${CMAKE_SOURCE_DIR}/bindings/node/tests/smoke.test.mjs"
            "$<TARGET_FILE:aetheris_stdio_server>")
  set_tests_properties(smoke.node_binding PROPERTIES LABELS "smoke")
else()
  message(STATUS "node not found; smoke.node_binding skipped.")
endif()

find_package(GTest CONFIG QUIET)
find_package(rapidcheck CONFIG QUIET)

if(GTest_FOUND AND TARGET GTest::gtest_main)
  include(GoogleTest)

  add_executable(
    aetheris_domain_tests
    application/intent_classifier_test.cpp
    application/intent_engine_test.cpp
    domain/action_schema_test.cpp
    domain/audit_test.cpp
    domain/capability_test.cpp
    domain/concepts_test.cpp
    domain/error_test.cpp
    domain/identifier_test.cpp
    domain/knowledge_orchestrator_test.cpp
    domain/non_empty_vector_test.cpp
    domain/quantity_test.cpp
    domain/safety_test.cpp
    domain/schema_registry_test.cpp
    domain/session_test.cpp
    domain/session_lifecycle_test.cpp
    domain/tagged_test.cpp
    domain/validation_pipeline_test.cpp
    infrastructure/action_schema_json_test.cpp
    infrastructure/audit_chain_test.cpp
    infrastructure/default_ports_test.cpp
    infrastructure/json_lines_exporter_test.cpp
    infrastructure/knowledge_sources_test.cpp
    infrastructure/rate_limiter_test.cpp
    infrastructure/session_repository_test.cpp
    infrastructure/session_json_test.cpp)
  target_include_directories(aetheris_domain_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(
    aetheris_domain_tests PRIVATE GTest::gtest_main aetheris::application
                                  aetheris::domain aetheris::infrastructure)
  aetheris_configure_target(aetheris_domain_tests)

  # ABI stability tests (exercise the public C API)
  add_executable(aetheris_abi_tests smoke/abi_stability_test.cpp)
  target_compile_definitions(aetheris_abi_tests PRIVATE AETHERIS_IUI_VERSION="${PROJECT_VERSION}")
  target_link_libraries(aetheris_abi_tests PRIVATE GTest::gtest_main aetheris::interface
                                                    aetheris::application)
  aetheris_configure_target(aetheris_abi_tests)

  gtest_discover_tests(
    aetheris_abi_tests
    PROPERTIES LABELS "smoke"
    DISCOVERY_TIMEOUT 30)

  add_test(
    NAME smoke.abi_stability
    COMMAND aetheris_abi_tests --gtest_filter=AbiStabilityTest.*)
  set_tests_properties(smoke.abi_stability PROPERTIES LABELS "smoke")

  gtest_discover_tests(
    aetheris_domain_tests
    PROPERTIES LABELS "unit"
    DISCOVERY_TIMEOUT 30)

  add_test(
    NAME contract.default_ports
    COMMAND
      aetheris_domain_tests
      --gtest_filter=SystemClockTest.*:UuidGeneratorTest.*:NoopTelemetryTest.*)
  set_tests_properties(contract.default_ports PROPERTIES LABELS "contract")

  add_test(
    NAME contract.action_schema_json
    COMMAND aetheris_domain_tests --gtest_filter=ActionSchemaJsonTest.*)
  set_tests_properties(contract.action_schema_json PROPERTIES LABELS "contract")

  add_test(
    NAME contract.audit_sink
    COMMAND aetheris_domain_tests --gtest_filter=MemoryAuditSinkTest.*)
  set_tests_properties(contract.audit_sink PROPERTIES LABELS "contract")

  add_test(
    NAME contract.rate_limiter
    COMMAND aetheris_domain_tests --gtest_filter=InMemoryRateLimiterTest.*:InMemoryBudgetControllerTest.*)
  set_tests_properties(contract.rate_limiter PROPERTIES LABELS "contract")

  add_test(
    NAME contract.validation_pipeline
    COMMAND aetheris_domain_tests --gtest_filter=ValidationPipelineTest.*:ValidationPipelineAdversarialTest.*)
  set_tests_properties(contract.validation_pipeline PROPERTIES LABELS "contract")

  add_test(
    NAME contract.session_repository
    COMMAND aetheris_domain_tests --gtest_filter=InMemorySessionRepositoryTest.*)
  set_tests_properties(contract.session_repository PROPERTIES LABELS "contract")

  add_test(
    NAME contract.knowledge_source
    COMMAND
      aetheris_domain_tests
      --gtest_filter=SchemaKnowledgeSourceTest.*:GlossaryKnowledgeSourceTest.*:VectorKnowledgeSourceTest.*:InMemoryVectorIndexTest.*:NullEmbeddingsTest.*)
  set_tests_properties(contract.knowledge_source PROPERTIES LABELS "contract")

  add_test(
    NAME integration.session_lifecycle
    COMMAND aetheris_domain_tests --gtest_filter=SessionLifecycleTest.*)
  set_tests_properties(integration.session_lifecycle PROPERTIES LABELS "integration")

  add_test(
    NAME integration.knowledge_degradation
    COMMAND aetheris_domain_tests --gtest_filter=KnowledgeDegradationTest.*)
  set_tests_properties(integration.knowledge_degradation PROPERTIES LABELS "integration")

  add_test(
    NAME contract.llm_backend
    COMMAND aetheris_domain_tests --gtest_filter=StubLlmBackendTest.*)
  set_tests_properties(contract.llm_backend PROPERTIES LABELS "contract")

  add_test(
    NAME contract.slot_extractor
    COMMAND aetheris_domain_tests --gtest_filter=SchemaSlotExtractorTest.*)
  set_tests_properties(contract.slot_extractor PROPERTIES LABELS "contract")

  add_test(
    NAME integration.intent_classifier
    COMMAND aetheris_domain_tests --gtest_filter=LlmIntentClassifierTest.*)
  set_tests_properties(integration.intent_classifier PROPERTIES LABELS "integration")

  add_test(
    NAME integration.intent_engine
    COMMAND aetheris_domain_tests --gtest_filter=IntentEngineTest.*)
  set_tests_properties(integration.intent_engine PROPERTIES LABELS "integration")
else()
  message(STATUS "GTest was not found; only the smoke CTest target will be built.")
endif()

if(rapidcheck_FOUND AND TARGET rapidcheck::rapidcheck)
  add_executable(aetheris_property_tests property/identifier_property_test.cpp)
  target_link_libraries(
    aetheris_property_tests PRIVATE rapidcheck::rapidcheck aetheris::domain)
  aetheris_configure_target(aetheris_property_tests)
  aetheris_add_ctest(property.identifier_round_trip aetheris_property_tests "property")

  add_executable(aetheris_schema_property_tests property/action_schema_property_test.cpp)
  target_link_libraries(
    aetheris_schema_property_tests PRIVATE rapidcheck::rapidcheck aetheris::domain
                                           aetheris::infrastructure)
  aetheris_configure_target(aetheris_schema_property_tests)
  aetheris_add_ctest(property.action_schema_round_trip aetheris_schema_property_tests "property")

  add_executable(aetheris_audit_property_tests property/audit_chain_property_test.cpp)
  target_link_libraries(
    aetheris_audit_property_tests PRIVATE rapidcheck::rapidcheck aetheris::domain
                                          aetheris::infrastructure)
  aetheris_configure_target(aetheris_audit_property_tests)
  aetheris_add_ctest(property.audit_chain aetheris_audit_property_tests "property")

  add_executable(aetheris_capability_property_tests property/capability_property_test.cpp)
  target_include_directories(aetheris_capability_property_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(aetheris_capability_property_tests PRIVATE rapidcheck::rapidcheck
                                                                    aetheris::domain)
  aetheris_configure_target(aetheris_capability_property_tests)
  aetheris_add_ctest(property.capability aetheris_capability_property_tests "property")

  add_executable(aetheris_session_property_tests property/session_property_test.cpp)
  target_link_libraries(aetheris_session_property_tests PRIVATE rapidcheck::rapidcheck
                                                                  aetheris::domain)
  aetheris_configure_target(aetheris_session_property_tests)
  aetheris_add_ctest(property.session aetheris_session_property_tests "property")

  add_executable(aetheris_knowledge_property_tests property/knowledge_property_test.cpp)
  target_link_libraries(aetheris_knowledge_property_tests PRIVATE rapidcheck::rapidcheck
                                                                   aetheris::domain)
  aetheris_configure_target(aetheris_knowledge_property_tests)
  aetheris_add_ctest(property.knowledge aetheris_knowledge_property_tests "property")

  add_executable(aetheris_intent_property_tests property/intent_property_test.cpp)
  target_link_libraries(aetheris_intent_property_tests PRIVATE rapidcheck::rapidcheck
                                                               aetheris::domain
                                                               aetheris::infrastructure)
  aetheris_configure_target(aetheris_intent_property_tests)
  aetheris_add_ctest(property.intent_classifier aetheris_intent_property_tests "property")

else()
  message(STATUS "rapidcheck was not found; property tests are skipped.")
endif()

add_executable(aetheris_fuzz_standalone fuzz/action_schema_fuzz.cpp)
target_compile_definitions(aetheris_fuzz_standalone PRIVATE AETHERIS_FUZZ_STANDALONE)
target_link_libraries(aetheris_fuzz_standalone PRIVATE aetheris::infrastructure)
aetheris_configure_target(aetheris_fuzz_standalone)

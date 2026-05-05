include_guard(GLOBAL)

function(aetheris_add_ctest test_name target_name test_labels)
  add_test(NAME ${test_name} COMMAND ${target_name})
  set_tests_properties(${test_name} PROPERTIES LABELS "${test_labels}")
endfunction()

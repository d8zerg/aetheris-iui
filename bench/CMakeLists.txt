find_package(benchmark CONFIG QUIET)

if(benchmark_FOUND AND TARGET benchmark::benchmark)
  add_executable(aetheris_core_smoke_bench core_smoke_bench.cpp)
  target_link_libraries(
    aetheris_core_smoke_bench PRIVATE benchmark::benchmark aetheris::infrastructure)
  aetheris_configure_target(aetheris_core_smoke_bench)
else()
  message(STATUS "Google Benchmark was not found; benchmark targets are skipped.")
endif()

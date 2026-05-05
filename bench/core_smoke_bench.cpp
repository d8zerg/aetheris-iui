#include <benchmark/benchmark.h>

#include "aetheris/infrastructure/uuid_generator.hpp"

namespace {

void BM_UuidGenerator(benchmark::State& state) {
  aetheris::infrastructure::UuidGenerator generator{42};
  for (auto _ : state) {
    benchmark::DoNotOptimize(generator.next_intent_id());
  }
}

BENCHMARK(BM_UuidGenerator);

} // namespace

BENCHMARK_MAIN();

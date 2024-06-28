#include <benchmark/benchmark.h>
#include <stdio.h>

#include "bench_common.h"
#include "memory_pool.h"

constexpr size_t kBenchmarkIterations = 100000;

static void MemoryPool(bool recreating = false) {
  std::vector<MyObj *> objs;
  for (size_t i = 0; i <= kDefaultBlockCount + 5; i++) {
    std::string name = "Child" + std::to_string(i);
    objs.push_back(pool::New<MyObj>(name, i));
    // printf("%s: %s\n", (recreating ? "Creating" : "Recreating"),
    // name.c_str());
  }

  for (auto &ptr : objs)
    pool::Delete(ptr);
  objs.clear();

  if (!recreating)
    MemoryPool(true);
}

static void ManualMalloc(bool recreating = false) {
  std::vector<MyObj *> objs;
  for (size_t i = 0; i <= kDefaultBlockCount + 5; i++) {
    std::string name = "Child" + std::to_string(i);
    MyObj *newObj = new MyObj(name, i);
    objs.push_back(newObj);
    // printf("%s: %s\n", (recreating ? "Creating" : "Recreating"),
    // name.c_str());
  }

  for (auto &ptr : objs)
    delete ptr;
  objs.clear();

  if (!recreating)
    ManualMalloc(true);
}

static void BM_MemoryPool(benchmark::State &state) {
  for (auto _ : state) {
    MemoryPool();
  }
}

static void BM_ManualMalloc(benchmark::State &state) {
  for (auto _ : state) {
    ManualMalloc();
  }
}

BENCHMARK(BM_MemoryPool)->Iterations(kBenchmarkIterations);
BENCHMARK(BM_ManualMalloc)->Iterations(kBenchmarkIterations);

int main(int argc, char **argv) {
  char arg0_default[] = "benchmark";
  char *args_default = arg0_default;
  if (!argv) {
    argc = 1;
    argv = &args_default;
  }
  benchmark::Initialize(&argc, argv);
  if (benchmark::ReportUnrecognizedArguments(argc, argv))
    return 1;
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();

  return 0;
}

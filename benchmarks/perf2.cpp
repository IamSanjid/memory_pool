#include "bench_common.h"
#include "memory_pool.h"

constexpr size_t kBenchmarkIterations = 100000;

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

int main() {
  for (size_t i = 0; i < kBenchmarkIterations; i++) {
    ManualMalloc();
  }
}

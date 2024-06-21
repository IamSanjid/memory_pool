#include "bench_common.h"
#include "memory_pool.h"

constexpr size_t kBenchmarkIterations = 100000;

static void MemoryPool(bool recreating = false) {
  std::vector<MyObj *> objs;
  for (size_t i = 0; i <= kDefaultBlockCount + 5; i++) {
    std::string name = "Child" + std::to_string(i);
    objs.push_back(pool::New<MyObj>(name, i));
    //printf("%s: %s\n", (recreating ? "Creating" : "Recreating"), name.c_str());
  }

  for (auto &ptr : objs)
    pool::Delete(ptr);
  objs.clear();

  if (!recreating)
    MemoryPool(true);
}

int main() {
  GlobalPoolManager::Init();
  std::ignore = PoolManager<MyObj>::GetInstance();
  for (size_t i = 0; i < kBenchmarkIterations; i++) {
    MemoryPool();
  }
  GlobalPoolManager::DestroyAll();
}

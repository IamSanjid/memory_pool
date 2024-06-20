// To simulate pool is out of pre-allocated space
#define FIXED_POOL_BLOCK_COUNT 2

#include "memory_pool.h"
#include <cstring>
#include <iostream>
#include <thread>

static int test_count = 0;
struct ThObj {
  ThObj(const std::string &p_name, uint32_t p_num) : name(p_name), num(p_num) {
    std::cout << "Creating " << name << "(" << num
              << ") on thread: " << std::this_thread::get_id() << "\n";
    printf("Obj ptr: %p\n", this);
  }
  void Print() {
    std::cout << "Alive " << name << "(" << num
              << ") on thread: " << std::this_thread::get_id() << "\n";
  }
  ~ThObj() {
    std::cout << "Destroying " << name << "(" << num
              << ") on thread: " << std::this_thread::get_id() << "\n";
  }
  std::string name;
  uint32_t num;
};

int test1() {
  std::cout << "\nTest" << ++test_count
            << ": Creating objects and moving it to another thread and "
               "deallocating\n";

  auto *obj1 = pool::New<ThObj>("FromMain", 1);
  auto *obj2 = pool::New<ThObj>("FromMain", 2);

  std::thread t1(
      +[](ThObj *moved_obj1, ThObj *moved_obj2) {
        std::cout << "Thread: " << std::this_thread::get_id() << "\n";

        pool::Delete(moved_obj1);
        pool::Delete(moved_obj2);
      },
      obj1, obj2);

  t1.join();

  std::cout << "\nRecreating\n";
  auto *obj3 = pool::New<ThObj>("Recreate", 1);
  auto *obj4 = pool::New<ThObj>("Recreate", 2);

  obj3->Print();
  obj4->Print();

  return 0;
}

int main() {
#define defer_return(v)                                                        \
  do {                                                                         \
    result = (v);                                                              \
    goto defer;                                                                \
  } while (0)

  int result = 0;
  GlobalPoolManager::Init();

  if (test1() != 0)
    defer_return(1);

defer:
  GlobalPoolManager::GetInstance()->DestroyAll();
  return result;
}

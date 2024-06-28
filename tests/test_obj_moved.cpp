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

struct MyObj {
  MyObj(const std::string &p_name, uint32_t p_num) : name(p_name), num(p_num) {}
  void Print() { printf("Obj ptr: %p\n", this); }
  ~MyObj() {
    std::cout << "Destroying " << name << "(" << num << "): ";
    printf("%p\n", this);
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

  bool is_obj3_reused_space =
      (uintptr_t)obj3 == (uintptr_t)obj1 || (uintptr_t)obj3 == (uintptr_t)obj2;
  bool is_obj4_reused_space =
      (uintptr_t)obj4 == (uintptr_t)obj1 || (uintptr_t)obj4 == (uintptr_t)obj2;

  return (is_obj3_reused_space && is_obj4_reused_space) ? 0 : 1;
}

int test2() {
  std::cout << "\nTest" << ++test_count
            << ": Create 3 pools, free space from previous pool and test if "
               "that pool gets used or not\n";

  std::vector<MyObj *> objs;
  for (size_t i = 0; i < kDefaultBlockCount * 3; i++) {
    MyObj *obj = pool::New<MyObj>("TmpChild", i);
    objs.push_back(obj);
  }

  std::cout << "\n Destroying from 0th pool \n";
  pool::Delete(objs[kDefaultBlockCount - 1]);

  std::cout << "\n Destroying from 3rd pool \n";
  pool::Delete(objs[(kDefaultBlockCount * 3) - 1]);

  std::cout << "\n Destroying from 2nd pool \n";
  pool::Delete(objs[(kDefaultBlockCount * 2) - 1]);

  std::cout << "\n Adding to 3rd pool \n";
  auto c_3rd = pool::New<MyObj>("NewChildTo3rd", 0);
  c_3rd->Print();

  std::cout << "\n Adding to 2nd pool \n";
  auto c_2nd = pool::New<MyObj>("NewChildTo2nd", 0);
  c_2nd->Print();

  std::cout << "\n Adding to 0th pool \n";
  auto c_0th = pool::New<MyObj>("NewChildTo0th", 0);
  c_0th->Print();

  bool is_0th_pool_reused =
      (uintptr_t)objs[kDefaultBlockCount - 1] == (uintptr_t)c_0th ||
      (uintptr_t)objs[kDefaultBlockCount - 1] == (uintptr_t)c_2nd ||
      (uintptr_t)objs[kDefaultBlockCount - 1] == (uintptr_t)c_3rd;
  return is_0th_pool_reused ? 0 : 1;
}

int main() {
#define defer_return(v)                                                        \
  do {                                                                         \
    result = (v);                                                              \
    goto defer;                                                                \
  } while (0)

  int result = 0;

  if (test1() != 0)
    defer_return(1);
  if (test2() != 0)
    defer_return(1);

  printf("\nAll %d Tests passed\n", test_count);
defer:
  if (result != 0) {
    printf("\n%d Tests passed, some other test failed\n", test_count - 1);
  }
  return result;
}

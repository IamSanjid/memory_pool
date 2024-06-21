#include "memory_pool.h"
#include <cstring>
#include <iostream>
#include <thread>

struct MyObj {
  MyObj(const std::string &name, uint32_t num)
      : objName(name), objNum(num), parent(nullptr) {}
  ~MyObj() {
    std::cout << "MyObj " << objName << " destroying\n";
    parent = nullptr;
  }
  void setParent(MyObj *p) {
    parent = p;
    hasParent = true;
  }
  void removeParent() {
    parent = nullptr;
    hasParent = false;
  }
  std::string objName;
  uint32_t objNum;
  MyObj *parent;
  bool hasParent = false;
};

struct MyObj2 {
  MyObj2(const std::string &name, uint32_t num)
      : auth(name, num), names(1, name) {}
  ~MyObj2() { std::cout << "MyObj2 " << auth.objName << " destroying\n"; }
  MyObj auth;
  std::vector<std::string> names;

  void addName(const std::string &name) { names.push_back(name); }

  void printNames() {
    std::cout << "MyObj2:\n";
    for (auto &name : names) {
      std::cout << "  name: " << name << "\n";
    }

    if (auth.hasParent) {
      std::cout << "auth.parent: " << auth.parent->objNum << ": "
                << auth.parent->objName << "\n";
    }
  }
};

static int test_count = 0;
int test_fixed_pool() {
  std::cout << "\nTest" << ++test_count
            << ": Directly using fixed pool to allocate two `MyObj`\n";
  FixedPool *pool = FixedPool::Create(sizeof(MyObj), 10);

  void *space1 = pool->Allocate();
  MyObj *parent = new (space1) MyObj("Parent", 0);

  void *space2 = pool->Allocate();
  MyObj *child = new (space2) MyObj("Child", 1);
  child->setParent(parent);

  std::cout << child->objNum << ": " << child->objName << "\n";
  std::cout << parent->objNum << ": " << parent->objName << "\n";

  child->~MyObj();
  pool->DeAllocate(space2);

  parent->~MyObj();
  pool->DeAllocate(space1);

  delete pool;

  return 0;
}

void create_my_objs(MyObj **ret_parent, MyObj **ret_child) {
  auto manager = PoolManager<MyObj>::GetInstance();
  MyObj *parent = manager->New("Parent", 2);
  MyObj *child = manager->New("Child", 3);

  std::cout << child->objNum << ": " << child->objName << "\n";
  std::cout << parent->objNum << ": " << parent->objName << "\n";

  *ret_parent = parent;
  *ret_child = child;
}

int test_pool_manager() {
  std::cout << "\nTest" << ++test_count
            << ": Using PoolManager to allocate two `MyObj`\n";
  auto manager = PoolManager<MyObj>::GetInstance();
  MyObj *parent, *child;
  create_my_objs(&parent, &child);

  manager->Delete(child);
  manager->Delete(parent);

  return 0;
}

void create_my_objs2(MyObj **ret_parent, MyObj2 **ret_child) {
  auto manager1 = PoolManager<MyObj>::GetInstance();
  auto manager2 = PoolManager<MyObj2>::GetInstance();
  MyObj *parent = manager1->New("Parent", 4);
  MyObj2 *child = manager2->New("Child", 5);

  child->addName("Child1");
  child->addName("Child2");
  child->addName("Child3");

  child->auth.setParent(parent);
  child->printNames();

  std::cout << parent->objNum << ": " << parent->objName << "\n";

  *ret_parent = parent;
  *ret_child = child;
}

int test_pool_manager2() {
  std::cout << "\nTest" << ++test_count
            << ": Using PoolManager to allocate one `MyObj` and one "
               "`MyObj2`\n";
  auto manager1 = PoolManager<MyObj>::GetInstance();
  auto manager2 = PoolManager<MyObj2>::GetInstance();
  MyObj *parent;
  MyObj2 *child;
  create_my_objs2(&parent, &child);

  manager2->Delete(child);
  manager1->Delete(parent);

  return 0;
}

int test_pool_manager3() {
  std::cout << "\nTest" << ++test_count
            << ": Using PoolManager to allocate one `MyObj` and one `MyObj2`\n"
               "Deleting both of those manually and recreating them again.\n";
  auto manager1 = PoolManager<MyObj>::GetInstance();
  auto manager2 = PoolManager<MyObj2>::GetInstance();
  MyObj *parent;
  MyObj2 *child;
  create_my_objs2(&parent, &child);

  manager2->Delete(child);
  manager1->Delete(parent);

  std::cout << "Reallocating after deallocating\n";

  create_my_objs2(&parent, &child);

  return 0;
}

int test_pool_manager4() {
  std::cout << "\nTest" << ++test_count
            << ": Creating one `MyObj` and one `MyObj2` on different threads\n"
               "and deleting them on main thread after those threads exiting\n";
  std::thread t1(
      +[](size_t id) {
        std::cout << "Thread: " << id << "\n";
        auto manager2 = PoolManager<MyObj2>::GetInstance();
        MyObj2 *child = manager2->New("Child", 7);

        child->addName("Child1");
        child->addName("Child2");
        child->addName("Child3");

        child->printNames();
      },
      1);
  std::thread t2(
      +[](size_t id) {
        std::cout << "Thread: " << id << "\n";
        auto manager1 = PoolManager<MyObj>::GetInstance();
        MyObj *parent = manager1->New("Parent", 6);
        std::cout << parent->objNum << ": " << parent->objName << "\n";
      },
      2);
  t1.join();
  t2.join();

  return 0;
}

int test_pool_manager5() {
  std::cout << "\nTest" << ++test_count << ": Creating primitive types\n";

  int *int_ptr = PoolManager<int>::GetInstance()->New(12);
  printf("int_ptr: %p, *int_ptr: %d\n", int_ptr, *int_ptr);

  char *char_ptr = PoolManager<char>::GetInstance()->New('E');
  printf("char_ptr: %p, *char_ptr: %c\n", char_ptr, *char_ptr);

  return 0;
}

int test_pool_manager6() {
  std::cout << "\nTest" << ++test_count
            << ": Exhausting first `FixedPool` of the `PoolManager<MyObj>`\n";

  auto manager = PoolManager<MyObj>::GetInstance();
  std::vector<MyObj *> objs;
  // `FixedPool` is created with 64 blocks by default
  for (size_t i = 0; i <= kDefaultBlockCount + 5; i++) {
    std::string name = "Child" + std::to_string(i);
    objs.push_back(manager->New(name, i));
    std::cout << "Creating: " << name << "\n";
  }

  return 0;
}

int test_pool_manager7() {
  struct ThObj {
    ThObj(const std::string &p_name, uint32_t p_num)
        : name(p_name), num(p_num) {
      std::cout << "Creating " << name << "(" << num
                << ") on thread: " << std::this_thread::get_id() << "\n";
      printf("Obj ptr: %p\n", this);
    }
    ~ThObj() {
      std::cout << "Destroying " << name << "(" << num
                << ") on thread: " << std::this_thread::get_id() << "\n";
    }
    std::string name;
    uint32_t num;
  };
  std::cout << "\nTest" << ++test_count
            << ": Creating an object and moving it to another thread and "
               "deallocating\n";

  std::cout << "MainThread: " << std::this_thread::get_id() << "\n";

  auto main_manager = PoolManager<ThObj>::GetInstance();
  ThObj *obj = main_manager->New("FromMain", 8);

  std::thread t1(
      +[](ThObj *moved_obj) {
        std::cout << "Thread: " << std::this_thread::get_id() << "\n";

        auto second_manager = PoolManager<ThObj>::GetInstance();
        second_manager->Delete(moved_obj);
      },
      obj);

  t1.join();

  return 0;
}

int main(int argc, char **argv) {
#define defer_return(v)                                                        \
  do {                                                                         \
    result = (v);                                                              \
    goto defer;                                                                \
  } while (0)

  int result = 0;
  GlobalPoolManager::Init();

  if (test_fixed_pool() != 0)
    defer_return(1);
  if (test_pool_manager() != 0)
    defer_return(1);
  if (test_pool_manager2() != 0)
    defer_return(1);
  if (test_pool_manager3() != 0)
    defer_return(1);
  if (test_pool_manager4() != 0)
    defer_return(1);
  if (test_pool_manager5() != 0)
    defer_return(1);

  if (argc > 1 && strcmp(argv[1], "-test_all") == 0) {
    if (test_pool_manager6() != 0)
      defer_return(1);
  }

  if (test_pool_manager7() != 0)
    defer_return(1);

  printf("\nAll %d Tests passed\n", test_count);
defer:
  GlobalPoolManager::DestroyAll();
  if (result != 0) {
    printf("\n%d Tests passed, some other test failed\n", test_count - 1);
  }
  return result;
}

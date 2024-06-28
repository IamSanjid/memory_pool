#ifndef __BENCH_COMMON_H_
#define __BENCH_COMMON_H_

#include <stdio.h>
#include <string>
#include <vector>

struct MyObj {
  MyObj(const std::string &name, uint32_t num)
      : objName(name), objNum(num), parent(nullptr) {}
  ~MyObj() {
    // printf("MyObj %s destroying\n", objName.c_str());
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
  ~MyObj2() {
    // printf("MyObj2 %s destroying\n", auth.objName.c_str());
  }
  MyObj auth;
  std::vector<std::string> names;

  void addName(const std::string &name) { names.push_back(name); }

  void printNames() {
    printf("MyObj2:\n");
    for (auto &name : names) {
      printf("  name: %s\n", name.c_str());
    }

    if (auth.hasParent) {
      printf("auth.parent: %d: %s\n", auth.parent->objNum,
             auth.parent->objName.c_str());
    }
  }
};

#endif // __BENCH_COMMON_H_

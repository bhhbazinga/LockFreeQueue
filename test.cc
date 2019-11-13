#include <string>
#include "lockfree_queue.h"

#include <chrono>
#include <cstdio>

#define Log(...)                                                  \
  fprintf(stderr, "[thread-%lu-%s]:", std::this_thread::get_id(), \
          __FUNCTION__);                                          \
  fprintf(stderr, __VA_ARGS__);                                   \
  fprintf(stderr, "\n")

int main(int argc, char const* argv[]) {
  (void)argc;
  (void)argv;

  LockFreeQueue<int> lfqueue;

  std::thread t1([&] {
    for (int i = 0; i < 10000; ++i) {
      lfqueue.Push(i);
    }
    Log("t1 exit");
  });
  std::thread t2([&] {
    for (int i = 0; i < 10000; ++i) {
      lfqueue.Push(i);
    }
    Log("t2 exit");
  });
  std::thread t3([&] {
    for (int i = 0; i < 10000; ++i) {
      auto p = lfqueue.Pop();
      if (nullptr != p) {
        // Log("t3 pop %d", *p);
      }
    }
    Log("t3 exit");
  });
  std::thread t4([&] {
    for (int i = 0; i < 10000; ++i) {
      auto p = lfqueue.Pop();
      if (nullptr != p) {
        // Log("t4 pop %d", *p);
      }
    }
    Log("t4 exit");
  });

  t1.join();
  t2.join();
  t3.join();
  t4.join();
}

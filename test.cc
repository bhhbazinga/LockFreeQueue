#include <string>
#include "lockfree_queue.h"

#include <cstdio>
#define Log(...)                                                  \
  fprintf(stderr, "[thread-%lu-%s]:", std::this_thread::get_id(), \
          __FUNCTION__);                                          \
  fprintf(stderr, __VA_ARGS__);                                   \
  fprintf(stderr, "\n")

class TestElement {
 public:
  TestElement() {
    // Log("TestElement init %p", this);
  }
  TestElement(const TestElement& other) {
    // Log("TestElement copy %p", this);
  }
  TestElement(TestElement&& other) {
    // Log("TestElement move copy %p", this);
  }
  ~TestElement() {
    // Log("TestElement deinit %p", this);
  }

 private:
};

int main(int argc, char const* argv[]) {
  (void)argc;
  (void)argv;

  LockFreeQueue<TestElement> lfqueue;

  std::thread t1([&] {
    for (int i = 0; i < 10000; ++i) {
      lfqueue.Push(TestElement());
    }
    Log("t1 exit");
  });
  std::thread t2([&] {
    for (int i = 0; i < 10000; ++i) {
      lfqueue.Push(TestElement());
    }
    Log("t2 exit");
  });
  std::thread t3([&] {
    for (int i = 0; i < 10000; ++i) {
      lfqueue.Pop();
    }
    Log("t3 exit");
  });
  std::thread t4([&] {
    for (int i = 0; i < 10000; ++i) {
      lfqueue.Pop();
    }
    Log("t4 exit");
  });

  t1.join();
  t2.join();
  t3.join();
  t4.join();
}

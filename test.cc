#include "lockfree_queue.h"

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
      lfqueue.Push(TestElement());
    }
    Log("t3 exit");
  });
  std::thread t4([&] {
    for (int i = 0; i < 10000; ++i) {
      lfqueue.Push(TestElement());
    }
    Log("t4 exit");
  });

  t1.join();
  t2.join();
  t3.join();
  t4.join();
}

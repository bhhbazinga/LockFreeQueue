#include "lockfree_queue.h"

#include <chrono>
#include <cstdio>
#include <iostream>

#include <cstdio>
#define Log(...)                                                  \
  fprintf(stderr, "[thread-%lu-%s]:", std::this_thread::get_id(), \
          __FUNCTION__);                                          \
  fprintf(stderr, __VA_ARGS__);                                   \
  fprintf(stderr, "\n")

const int kMaxElements = 1000000;

int main(int argc, char const* argv[]) {
  (void)argc;
  (void)argv;

  LockFreeQueue<int> q;
  std::atomic<bool> start(false);

  std::thread t1([&] {
    while (!start) {
      std::this_thread::yield();
    }
    for (int i = 0; i < kMaxElements / 2; ++i) {
      q.Enqueue(i);
    }
  });

  std::thread t2([&] {
    while (!start) {
      std::this_thread::yield();
    }
    for (int i = 0; i < kMaxElements / 2; ++i) {
      q.Enqueue(i);
    }
  });

  std::thread t3([&] {
    while (!start) {
      std::this_thread::yield();
    }
    for (int i = 0; i < kMaxElements / 2; ++i) {
      q.Dequeue();
    }
  });

  std::thread t4([&] {
    while (!start) {
      std::this_thread::yield();
    }
    for (int i = 0; i < kMaxElements / 2; ++i) {
      q.Dequeue();
    }
  });

  start.store(true);
  auto t1_ = std::chrono::steady_clock::now();
  t1.join();
  t2.join();
  t3.join();
  t4.join();
  auto t2_ = std::chrono::steady_clock::now();

  std::cout << "t2-t1="
            << std::chrono::duration_cast<std::chrono::milliseconds>(t2_ - t1_)
                   .count()
            << ","
            << "left=" << q.size() << "\n";
  return 0;
}

#include "lockfree_queue.h"

#include <chrono>
#include <cstdio>
#include <iostream>
#include <queue>

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

  std::atomic<int> cnt = 0;
  std::thread t3([&] {
    while (!start) {
      std::this_thread::yield();
    }
    for (; cnt.load(std::memory_order_relaxed) < kMaxElements;) {
      if (nullptr != q.Dequeue()) {
        cnt.fetch_add(1, std::memory_order_relaxed);
      }
    }
  });

  std::thread t4([&] {
    while (!start) {
      std::this_thread::yield();
    }
    for (; cnt.load(std::memory_order_relaxed) < kMaxElements;) {
      if (nullptr != q.Dequeue()) {
        cnt.fetch_add(1, std::memory_order_relaxed);
      }
    }
  });

  start.store(true);
  auto t1_ = std::chrono::steady_clock::now();
  t1.join();
  t2.join();
  t3.join();
  t4.join();
  auto t2_ = std::chrono::steady_clock::now();

  assert(q.size() == 0);
  std::cout << "100K elements enqueue and dequeue, timespan="
            << std::chrono::duration_cast<std::chrono::milliseconds>(t2_ - t1_)
                   .count()
            << "ms"
            << " ,"
            << "number of harzard pointer=" << MAX_THREADS << "\n";

  return 0;
}

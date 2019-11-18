#include <string>
#include "lockfree_queue.h"

#include <chrono>
#include <cstdio>
#include <mutex>
#include <unordered_set>
#include <vector>

#define Log(...)                                                  \
  fprintf(stderr, "[thread-%lu-%s]:", std::this_thread::get_id(), \
          __FUNCTION__);                                          \
  fprintf(stderr, __VA_ARGS__);                                   \
  fprintf(stderr, "\n")

int main(int argc, char const* argv[]) {
  (void)argc;
  (void)argv;

  LockFreeQueue<int> lfqueue;
  std::unordered_set<int*> set;
  std::vector<int> in1;
  std::vector<int> in2;
  std::vector<int> out1;
  std::vector<int> out2;
  std::atomic<bool> start(false);
  std::atomic<int> cur(0);

  std::thread t1([&] {
    while (!start) {
      std::this_thread::yield();
    }

    for (int i = 0; i < 5; ++i) {
      int x = cur.fetch_add(1);
      in1.push_back(x);
      lfqueue.Enqueue(x);
      std::this_thread::yield();
    }
  });

  std::thread t2([&] {
    while (!start) {
      std::this_thread::yield();
    }

    for (int i = 0; i < 5; ++i) {
      int x = cur.fetch_add(1);
      in2.push_back(x);
      lfqueue.Enqueue(x);
      std::this_thread::yield();
    }
  });

  // std::thread t3([&] {
  //   while (!start) {
  //     std::this_thread::yield();
  //   }

  //   for (int i = 0; i < 20; ++i) {
  //     auto p = lfqueue.Dequeue();
  //     if (nullptr != p) {
  //       out1.push_back(*p);
  //     }
  //     std::this_thread::yield();
  //   }
  // });

  // std::thread t4([&] {
  //   while (!start) {
  //     std::this_thread::yield();
  //   }

  //   for (int i = 0; i < 10; ++i) {
  //     auto p = lfqueue.Dequeue();
  //     if (nullptr != p) {
  //       out2.push_back(*p);
  //     }
  //     std::this_thread::yield();
  //   }
  // });

  start.store(true);

  t1.join();
  t2.join();
  // t3.join();
  // t4.join();

  for (int i = 0; i < 10; ++i) {
    auto p = lfqueue.Dequeue();
    if (nullptr != p) {
      out1.push_back(*p);
    }
  }

  // Log("size=%d", lfqueue.size());

  // Log("----------------------------------------in1:");
  // for (auto it = in1.begin(); it < in1.end(); ++it) {
  //   Log("%d", *it);
  // }

  // Log("----------------------------------------in2:");
  // for (auto it = in2.begin(); it < in2.end(); ++it) {
  //   Log("%d", *it);
  // }

  // Log("----------------------------------------out1:");
  // for (auto it = out1.begin(); it < out1.end(); ++it) {
  //   Log("%d", *it);
  // }

  // Log("----------------------------------------out2:");
  // for (auto it = out2.begin(); it < out2.end(); ++it) {
  //   Log("%d", *it);
  // }
}

#include "lockfree_queue.h"

#include <chrono>
#include <cstdio>
#include <iostream>
#include <unordered_map>

int maxElements = 1000000;
LockFreeQueue<int> q;
std::atomic<int> cnt = 0;
std::atomic<bool> start = false;
std::unordered_map<int, int*> elements2timespan;

auto enqueue_func = [](int divide) {
  while (!start) {
    std::this_thread::yield();
  }
  for (int i = 0; i < maxElements / divide; ++i) {
    q.Enqueue(i);
  }
};

auto dequeue_func = [] {
  while (!start) {
    std::this_thread::yield();
  }
  int x;
  for (; cnt.load(std::memory_order_relaxed) < maxElements;) {
    if (q.Dequeue(x)) {
      cnt.fetch_add(1, std::memory_order_relaxed);
    }
  }
};

void TestConcurrentEnqueue() {
  std::thread t1(enqueue_func, 4);
  std::thread t2(enqueue_func, 4);
  std::thread t3(enqueue_func, 4);
  std::thread t4(enqueue_func, 4);

  start = true;
  auto t1_ = std::chrono::steady_clock::now();
  t1.join();
  t2.join();
  t3.join();
  t4.join();
  auto t2_ = std::chrono::steady_clock::now();

  assert(static_cast<int>(q.size()) == maxElements);
  int ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(t2_ - t1_).count();
  elements2timespan[maxElements][0] += ms;
  std::cout << maxElements << " elements enqueue concurrently, timespan=" << ms
            << "ms"
            << "\n";
  start = false;
}

void TestConcurrentDequeue() {
  std::thread t1(dequeue_func);
  std::thread t2(dequeue_func);
  std::thread t3(dequeue_func);
  std::thread t4(dequeue_func);

  cnt = 0;
  start = true;

  auto t1_ = std::chrono::steady_clock::now();
  t1.join();
  t2.join();
  t3.join();
  t4.join();
  auto t2_ = std::chrono::steady_clock::now();

  assert(static_cast<int>(q.size()) == 0 && cnt == maxElements);
  int ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(t2_ - t1_).count();
  elements2timespan[maxElements][1] += ms;
  std::cout << maxElements << " elements dequeue concurrently, timespan=" << ms
            << "ms"
            << "\n";

  cnt = 0;
  start = false;
}

void TestConcurrentEnqueueAndDequeue() {
  std::thread t1(enqueue_func, 2);
  std::thread t2(enqueue_func, 2);
  std::thread t3(dequeue_func);
  std::thread t4(dequeue_func);

  cnt = 0;
  start = true;

  auto t1_ = std::chrono::steady_clock::now();
  t1.join();
  t2.join();
  t3.join();
  t4.join();
  auto t2_ = std::chrono::steady_clock::now();

  assert(static_cast<int>(q.size()) == 0 && cnt == maxElements);
  int ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(t2_ - t1_).count();
  elements2timespan[maxElements][2] += ms;
  std::cout << maxElements
            << " elements enqueue and dequeue concurrently, timespan=" << ms
            << "ms"
            << "\n";

  cnt = 0;
  start = false;
}

std::unordered_map<int, int> element2count1;
std::unordered_map<int, int> element2count2;

auto dequeue_func_with_count = [](std::unordered_map<int, int>& element2count) {
  while (!start) {
    std::this_thread::yield();
  }
  int x;
  for (; cnt.load(std::memory_order_relaxed) < maxElements;) {
    if (q.Dequeue(x)) {
      cnt.fetch_add(1, std::memory_order_relaxed);
      ++element2count[x];
    }
  }
};
void TestCorrectness() {
  for (int i = 0; i < maxElements / 2; ++i) {
    element2count1[i] = 0;
    element2count2[i] = 0;
  }

  maxElements = 1000000;
  std::thread t1(enqueue_func, 2);
  std::thread t2(enqueue_func, 2);
  std::thread t3(dequeue_func_with_count, std::ref(element2count1));
  std::thread t4(dequeue_func_with_count, std::ref(element2count2));
  cnt = 0;
  start = true;

  t1.join();
  t2.join();
  t3.join();
  t4.join();

  assert(static_cast<int>(q.size()) == 0 && cnt == maxElements);
  for (int i = 0; i < maxElements / 2; ++i) {
    assert(element2count1[i] + element2count2[i] == 2);
  }
}

int main(int argc, char const* argv[]) {
  (void)argc;
  (void)argv;

  std::cout << "Benchmark with " << MAX_THREADS << " hazard threads:"
            << "\n";

  int elements[] = {10000, 100000, 1000000};
  int timespan1[] = {0, 0, 0};
  int timespan2[] = {0, 0, 0};
  int timespan3[] = {0, 0, 0};

  elements2timespan[10000] = timespan1;
  elements2timespan[100000] = timespan2;
  elements2timespan[1000000] = timespan3;

  for (int i = 0; i < 10; ++i) {
    for (int j = 0; j < 3; ++j) {
      maxElements = elements[j];
      TestConcurrentEnqueue();
      TestConcurrentDequeue();
      TestConcurrentEnqueueAndDequeue();
      std::cout << "\n";
    }
  }

  for (int i = 0; i < 3; ++i) {
    maxElements = elements[i];
    float avg = static_cast<float>(elements2timespan[maxElements][0]) / 10.0f;
    std::cout << maxElements
              << " elements enqueue concurrently, average timespan=" << avg
              << "ms"
              << "\n";
    avg = static_cast<float>(elements2timespan[maxElements][1]) / 10.0f;
    std::cout << maxElements
              << " elements dequeue concurrently, average timespan=" << avg
              << "ms"
              << "\n";
    avg = static_cast<float>(elements2timespan[maxElements][2]) / 10.0f;
    std::cout << maxElements
              << " elements enqueue and dequeue concurrently, average timespan="
              << avg << "ms"
              << "\n";
    std::cout << "\n";
  }

  TestCorrectness();

  return 0;
}

#include "lockfree_queue.h"

#include <chrono>
#include <iostream>
#include <unordered_map>
#include <vector>

const int kMaxThreads = 8;
static_assert((kMaxThreads & (kMaxThreads - 1)) == 0,
              "Make sure kMaxThreads == 2^n");

int maxElements;
LockFreeQueue<int> q;
std::atomic<int> cnt(0);
std::atomic<bool> start(false);
std::unordered_map<int, int*> elements2timespan;

void onEnqueue(int divide) {
  while (!start) {
    std::this_thread::yield();
  }
  for (int i = 0; i < maxElements / divide; ++i) {
    q.Enqueue(i);
  }
}

void onDequeue() {
  while (!start) {
    std::this_thread::yield();
  }
  int x;
  for (; cnt.load(std::memory_order_relaxed) < maxElements;) {
    if (q.Dequeue(x)) {
      cnt.fetch_add(1, std::memory_order_relaxed);
    }
  }
}

void TestConcurrentEnqueue() {
  std::vector<std::thread> threads;
  for (int i = 0; i < kMaxThreads; ++i) {
    threads.push_back(std::thread(onEnqueue, kMaxThreads));
  }

  start = true;
  auto t1_ = std::chrono::steady_clock::now();
  for (int i = 0; i < kMaxThreads; ++i) {
    threads[i].join();
  }
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
  std::vector<std::thread> threads;
  for (int i = 0; i < kMaxThreads; ++i) {
    threads.push_back(std::thread(onDequeue));
  }

  cnt = 0;
  start = true;
  auto t1_ = std::chrono::steady_clock::now();
  for (int i = 0; i < kMaxThreads; ++i) {
    threads[i].join();
  }
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
  std::vector<std::thread> enqueue_threads;
  for (int i = 0; i < kMaxThreads / 2; ++i) {
    enqueue_threads.push_back(std::thread(onEnqueue, kMaxThreads / 2));
  }

  std::vector<std::thread> dequeue_threads;
  for (int i = 0; i < kMaxThreads / 2; ++i) {
    dequeue_threads.push_back(std::thread(onDequeue));
  }

  cnt = 0;
  start = true;
  auto t1_ = std::chrono::steady_clock::now();
  for (int i = 0; i < kMaxThreads / 2; ++i) {
    enqueue_threads[i].join();
  }
  for (int i = 0; i < kMaxThreads / 2; ++i) {
    dequeue_threads[i].join();
  }
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

auto onDequeue_with_count = [](std::unordered_map<int, int>& element2count) {
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

std::unordered_map<int, int> element2count[kMaxThreads / 2];

void TestCorrectness() {
  maxElements = 1000000;
  assert(maxElements % kMaxThreads == 0);

  for (int i = 0; i < maxElements / kMaxThreads; ++i) {
    for (int j = 0; j < kMaxThreads / 2; ++j) {
      element2count[j][i] = 0;
    }
  }

  std::vector<std::thread> enqueue_threads;
  for (int i = 0; i < kMaxThreads / 2; ++i) {
    enqueue_threads.push_back(std::thread(onEnqueue, kMaxThreads / 2));
  }

  std::vector<std::thread> dequeue_threads;
  for (int i = 0; i < kMaxThreads / 2; ++i) {
    dequeue_threads.push_back(
        std::thread(onDequeue_with_count, std::ref(element2count[i])));
  }

  cnt = 0;
  start = true;
  for (int i = 0; i < kMaxThreads / 2; ++i) {
    enqueue_threads[i].join();
  }
  for (int i = 0; i < kMaxThreads / 2; ++i) {
    dequeue_threads[i].join();
  }

  assert(static_cast<int>(q.size()) == 0 && cnt == maxElements);
  for (int i = 0; i < maxElements / kMaxThreads; ++i) {
    int sum = 0;
    for (int j = 0; j < kMaxThreads / 2; ++j) {
      sum += element2count[j][i];
    }
    assert(sum == kMaxThreads / 2);
  }
}

const int elements1 = 10000;
const int elements2 = 100000;
const int elements3 = 1000000;


int main(int argc, char const* argv[]) {
  (void)argc;
  (void)argv;

  std::cout << "Benchmark with " << kMaxThreads << " threads:"
            << "\n";

  int elements[] = {elements1, elements2, elements3};
  int timespan1[] = {0, 0, 0};
  int timespan2[] = {0, 0, 0};
  int timespan3[] = {0, 0, 0};

  elements2timespan[elements1] = timespan1;
  elements2timespan[elements2] = timespan2;
  elements2timespan[elements3] = timespan3;

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
              << " elements enqueue and dequeue concurrently, average timespan = "
              << avg << "ms"
              << "\n";
    std::cout << "\n";
  }

  // TestCorrectness();
  return 0;
}
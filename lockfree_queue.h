#ifndef LOCKFREE_QUEUE_H
#define LOCKFREE_QUEUE_H

#include "reclaimer.h"

#include <atomic>
#include <cassert>
#include <cstdio>
#include <memory>

#include <cstdio>
#define Log(...)                                                  \
  fprintf(stderr, "[thread-%lu-%s]:", std::this_thread::get_id(), \
          __FUNCTION__);                                          \
  fprintf(stderr, __VA_ARGS__);                                   \
  fprintf(stderr, "\n")

// An estimate count that must be greater or equal than the number of threads
const int kEstimateHazardPointerCount = 8;

template <typename T>
class LockFreeQueue {
 public:
  LockFreeQueue() : head_(new Node), tail_(head_.load()), size_(0) {}

  ~LockFreeQueue() {
    while (nullptr != Pop())
      ;
    delete head_.load();
  }

  void Push(const T& data) {
    std::unique_ptr<T> new_data = std::make_unique<T>(data);
    InternalPush(new_data);
  };

  void Push(T&& data) {
    std::unique_ptr<T> new_data = std::make_unique<T>(std::move(data));
    InternalPush(new_data);
  }

  std::shared_ptr<T> Pop();
  size_t Size() const { return size_; }

 private:
  void InternalPush(std::unique_ptr<T>& new_data);

  struct Node {
    std::atomic<T*> data;
    Node* next;
    Node() : data(nullptr), next(nullptr) {}
    ~Node() {}
  };
  std::atomic<Node*> head_;
  std::atomic<Node*> tail_;
  std::atomic<size_t> size_;
  typename Reclaimer<Node>::HazardPointer
      hazard_pointers_[kEstimateHazardPointerCount];
};

template <typename T>
void LockFreeQueue<T>::InternalPush(std::unique_ptr<T>& new_data) {
  Reclaimer<Node>& reclaimer = Reclaimer<Node>::GetInstance(
      hazard_pointers_, kEstimateHazardPointerCount);
  Node* new_tail = new Node();
  Node* old_tail = tail_.load();
  Node* temp = old_tail;
  T* expected_data = nullptr;
  do {
    do {
      temp = old_tail;
      // Make sure the Hazard pointer we set is tail
      reclaimer.MarkHazard(old_tail);
      old_tail = tail_.load();
    } while (temp != old_tail);
    expected_data = nullptr;
    // Because we set the Hazard pointer, so the old_tail can't be nullptr
  } while (
      !old_tail->data.compare_exchange_strong(expected_data, new_data.get()));

  // At this point, other Push thread can not continue
  old_tail->next = new_tail;
  tail_.store(new_tail);
  // At this point, other Push thread can continue

  reclaimer.MarkHazard(nullptr);
  new_data.release();
}

template <typename T>
std::shared_ptr<T> LockFreeQueue<T>::Pop() {
  Reclaimer<Node>& reclaimer = Reclaimer<Node>::GetInstance(
      hazard_pointers_, kEstimateHazardPointerCount);
  Node* old_head = head_.load();
  Node* temp = old_head;
  do {
    do {
      // Make sure the Hazard pointer we set is head
      temp = old_head;
      reclaimer.MarkHazard(old_head);
      old_head = head_.load();
    } while (temp != old_head);
    // Because we set the Hazard pointer, so the old_head can't be nullptr
    if (nullptr == old_head->next) {
      // Because old_head is dummy node, the queue is empty
      reclaimer.MarkHazard(nullptr);
      return nullptr;
    }
  } while (!head_.compare_exchange_weak(old_head, old_head->next));
  // So this thread is the only thread that can
  // delete old_head or push old_head to gc list
  reclaimer.MarkHazard(nullptr);

  std::shared_ptr<T> res(old_head->data.load());
  if (reclaimer.Hazard(old_head)) {
    reclaimer.ReclaimLater(old_head);
  } else {
    delete old_head;
  }
  reclaimer.ReclaimNoHazardPointer();
  return res;
}

#endif  // LOCKFREE_QUEUE_H

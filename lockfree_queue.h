#ifndef LOCKFREE_QUEUE_H
#define LOCKFREE_QUEUE_H

#include "reclaimer.h"

#include <atomic>
#include <memory>

template <typename T>
class LockFreeQueue {
 public:
  LockFreeQueue()
      : head_(new Node),
        tail_(head_.load(std::memory_order_relaxed)),
        size_(0) {}

  ~LockFreeQueue() {
    while (nullptr != Dequeue())
      ;
    delete head_.load(std::memory_order_relaxed);
  }

  void Enqueue(const T& data) {
    std::unique_ptr<T> new_data = std::make_unique<T>(data);
    InternalEnqueue(new_data);
  };

  void Enqueue(T&& data) {
    std::unique_ptr<T> new_data = std::make_unique<T>(std::move(data));
    InternalEnqueue(new_data);
  }

  std::shared_ptr<T> Dequeue();
  size_t size() const { return size_.load(std::memory_order_relaxed); }

 private:
  void InternalEnqueue(std::unique_ptr<T>& new_data);

  struct Node {
    Node() : data(nullptr), next(nullptr) {}
    ~Node() {}

    std::atomic<T*> data;
    Node* next;
  };

  std::atomic<Node*> head_;
  std::atomic<Node*> tail_;
  std::atomic<size_t> size_;
};

template <typename T>
void LockFreeQueue<T>::InternalEnqueue(std::unique_ptr<T>& new_data) {
  Reclaimer& reclaimer = Reclaimer::GetInstance();
  Node* new_tail = new Node();
  Node* old_tail = tail_.load(std::memory_order_relaxed);
  Node* temp = old_tail;
  T* expected_data = nullptr;
  do {
    do {
      temp = old_tail;
      // Make sure the hazard pointer we set is tail
      reclaimer.MarkHazard(old_tail);
      old_tail = tail_.load(std::memory_order_relaxed);
    } while (temp != old_tail);
    expected_data = nullptr;
    // Because we set the hazard pointer, so the old_tail can't be nullptr
  } while (!old_tail->data.compare_exchange_strong(
      expected_data, new_data.get(), std::memory_order_relaxed,
      std::memory_order_relaxed));

  // At this point, other enqueue thread can not continue
  old_tail->next = new_tail;
  tail_.store(new_tail, std::memory_order_release);
  // At this point, other enqueue thread can continue

  size_.fetch_add(1, std::memory_order_relaxed);
  reclaimer.MarkHazard(nullptr);
  new_data.release();
}

template <typename T>
std::shared_ptr<T> LockFreeQueue<T>::Dequeue() {
  Reclaimer& reclaimer = Reclaimer::GetInstance();
  Node* old_head = head_.load(std::memory_order_relaxed);
  Node* temp = old_head;
  do {
    do {
      // Make sure the hazard pointer we set is head
      temp = old_head;
      reclaimer.MarkHazard(old_head);
      old_head = head_.load(std::memory_order_relaxed);
    } while (temp != old_head);
    // Because we set the hazard pointer, so the old_head can't be nullptr
    if (tail_.load(std::memory_order_acquire) == old_head) {
      // Because old_head is dummy node, the queue is empty
      reclaimer.MarkHazard(nullptr);
      return nullptr;
    }
  } while (!head_.compare_exchange_strong(old_head, old_head->next,
                                          std::memory_order_relaxed,
                                          std::memory_order_relaxed));
  size_.fetch_sub(1, std::memory_order_relaxed);

  // So this thread is the only thread that can
  // delete old_head or enqueue old_head to gc list
  reclaimer.MarkHazard(nullptr);

  std::shared_ptr<T> res(old_head->data.load(std::memory_order_relaxed));
  if (reclaimer.Hazard(old_head)) {
    reclaimer.ReclaimLater(old_head, [old_head] { delete old_head; });
  } else {
    delete old_head;
  }
  reclaimer.ReclaimNoHazardPointer();
  return res;
}

#endif  // LOCKFREE_QUEUE_H

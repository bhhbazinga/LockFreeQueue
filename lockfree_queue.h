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
    T data;
    while (Dequeue(data))
      ;
    delete head_.load(std::memory_order_relaxed);
  }

  void Enqueue(const T& data) {
    T* data_ptr = new T(data);
    InternalEnqueue(data_ptr);
  };

  void Enqueue(T&& data) {
    T* data_ptr = new T(std::move(data));
    InternalEnqueue(data_ptr);
  }

  bool Dequeue(T& data);
  size_t size() const { return size_.load(std::memory_order_relaxed); }

 private:
  void InternalEnqueue(T* data_ptr);

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
//std::unique_ptr<T>& new_data
void LockFreeQueue<T>::InternalEnqueue(T* data_ptr) {
  Reclaimer& reclaimer = Reclaimer::GetInstance();
  Node* new_tail = new Node();
  Node* old_tail = tail_.load(std::memory_order_relaxed);
  Node* temp;
  T* expected_data = nullptr;
  do {
    do {
      temp = old_tail;
      // Make sure the hazard pointer we set is tail
      reclaimer.MarkHazard(old_tail);
      old_tail = tail_.load(std::memory_order_acquire);
    } while (temp != old_tail);
    expected_data = nullptr;
    // Because we set the hazard pointer, so the old_tail can't be delete
  } while (!old_tail->data.compare_exchange_strong(
      expected_data, data_ptr, std::memory_order_relaxed,
      std::memory_order_relaxed));

  // At this point, other enqueue thread can not continue
  old_tail->next = new_tail;
  tail_.store(new_tail, std::memory_order_release);
  // At this point, other enqueue thread can continue

  size_.fetch_add(1, std::memory_order_relaxed);
  reclaimer.MarkHazard(nullptr);
}

template <typename T>
bool LockFreeQueue<T>::Dequeue(T& data) {
  Reclaimer& reclaimer = Reclaimer::GetInstance();
  Node* old_head = head_.load(std::memory_order_relaxed);
  Node* temp;
  do {
    do {
      // Make sure the hazard pointer we set is head
      temp = old_head;
      reclaimer.MarkHazard(old_head);
      old_head = head_.load(std::memory_order_relaxed);
    } while (temp != old_head);
    // Because we set the hazard pointer, so the old_head can't be delete
    if (tail_.load(std::memory_order_acquire) == old_head) {
      // Because old_head is dummy node, the queue is empty
      reclaimer.MarkHazard(nullptr);
      return false;
    }
  } while (!head_.compare_exchange_strong(old_head, old_head->next,
                                          std::memory_order_relaxed,
                                          std::memory_order_relaxed));
  size_.fetch_sub(1, std::memory_order_relaxed);

  // So this thread is the only thread that can
  // delete old_head or enqueue old_head to gc list
  reclaimer.MarkHazard(nullptr);

  T* data_ptr = old_head->data.load(std::memory_order_relaxed);
  data = std::move(*data_ptr);
  delete data_ptr;
  // std::shared_ptr<T> res(old_head->data.load(std::memory_order_relaxed));
  if (reclaimer.Hazard(old_head)) {
    reclaimer.ReclaimLater(old_head,
                           [](void* p) { delete reinterpret_cast<Node*>(p); });
  } else {
    delete old_head;
  }
  reclaimer.ReclaimNoHazardPointer();
  return true;
}

#endif  // LOCKFREE_QUEUE_H

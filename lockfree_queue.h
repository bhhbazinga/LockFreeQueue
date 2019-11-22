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

  LockFreeQueue(const LockFreeQueue&) = delete;
  LockFreeQueue(LockFreeQueue&&) = delete;
  LockFreeQueue& operator=(const LockFreeQueue& other) = delete;
  LockFreeQueue& operator=(LockFreeQueue&& other) = delete;

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
  struct Node;
  bool TryInsertNewTail(Node* old_tail, Node* new_tail) {
    Node* null_ptr = nullptr;
    if (old_tail->next.compare_exchange_strong(null_ptr, new_tail,
                                               std::memory_order_relaxed,
                                               std::memory_order_relaxed)) {
      tail_.store(new_tail, std::memory_order_release);
      size_.fetch_add(1, std::memory_order_relaxed);
      return true;
    } else {
      return false;
    }
  }

  void InternalEnqueue(T* data_ptr);

  struct Node {
    Node() : data(nullptr), next(nullptr) {}
    ~Node() {}

    std::atomic<T*> data;
    std::atomic<Node*> next;
  };

  std::atomic<Node*> head_;
  std::atomic<Node*> tail_;
  std::atomic<size_t> size_;
};

template <typename T>
// std::unique_ptr<T>& new_data
void LockFreeQueue<T>::InternalEnqueue(T* data_ptr) {
  Reclaimer& reclaimer = Reclaimer::GetInstance();
  Node* new_tail = new Node();
  Node* old_tail = tail_.load(std::memory_order_relaxed);
  Node* temp;
  for (;;) {
    do {
      temp = old_tail;
      // Make sure the hazard pointer we set is tail
      reclaimer.MarkHazard(old_tail);
      old_tail = tail_.load(std::memory_order_acquire);
    } while (temp != old_tail);
    // Because we set the hazard pointer, so the old_tail can't be delete
    T* null_ptr = nullptr;
    if (old_tail->data.compare_exchange_strong(null_ptr, data_ptr,
                                               std::memory_order_relaxed,
                                               std::memory_order_relaxed)) {
      if (!TryInsertNewTail(old_tail, new_tail)) {
        // Other help thread already insert tail
        delete new_tail;
      }
      reclaimer.MarkHazard(nullptr);
      return;
    } else {
      // If CAS failed,
      // help another thread to finish enqueueing
      if (TryInsertNewTail(old_tail, new_tail)) {
        // Help success, so allocate new one
        new_tail = new Node();
      }
    }
  }
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
  } while (!head_.compare_exchange_strong(
      old_head, old_head->next.load(std::memory_order_relaxed),
      std::memory_order_relaxed, std::memory_order_relaxed));
  size_.fetch_sub(1, std::memory_order_relaxed);

  // So this thread is the only thread that can
  // delete old_head or push old_head to reclaim list
  reclaimer.MarkHazard(nullptr);

  T* data_ptr = old_head->data.load(std::memory_order_relaxed);
  data = std::move(*data_ptr);
  delete data_ptr;
  reclaimer.ReclaimLater(old_head,
                         [](void* p) { delete reinterpret_cast<Node*>(p); });
  reclaimer.ReclaimNoHazardPointer();
  return true;
}

#endif  // LOCKFREE_QUEUE_H

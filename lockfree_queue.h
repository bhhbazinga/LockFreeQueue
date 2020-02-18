#ifndef LOCKFREE_QUEUE_H
#define LOCKFREE_QUEUE_H

#include <atomic>
#include "HazardPointer/reclaimer.h"

template <typename T>
class LockFreeQueue {
  static_assert(std::is_copy_constructible_v<T>, "T requires copy constructor");

 public:
  LockFreeQueue()
      : head_(new Node),
        tail_(head_.load(std::memory_order_relaxed)),
        size_(0) {}

  ~LockFreeQueue() {
    while (Dequeue())
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
                                               std::memory_order_release,
                                               std::memory_order_relaxed)) {
      tail_.store(new_tail, std::memory_order_release);
      size_.fetch_add(1, std::memory_order_relaxed);
      return true;
    } else {
      return false;
    }
  }

  void InternalEnqueue(T* data_ptr);
  // Dequeue head
  Node* InternalDequeue();
  // Dequeue used in destructor
  bool Dequeue();

  // Invoke this function when the node can be reclaimed
  static void OnDeleteNode(void* ptr) { delete static_cast<Node*>(ptr); }

  struct Node {
    Node() : data(nullptr), next(nullptr) {}
    ~Node() {}

    std::atomic<T*> data;
    std::atomic<Node*> next;
  };

  std::atomic<Node*> head_;
  std::atomic<Node*> tail_;
  std::atomic<size_t> size_;
  HazardPointerList hazard_pointer_list_;
};

template <typename T>
// std::unique_ptr<T>& new_data
void LockFreeQueue<T>::InternalEnqueue(T* data_ptr) {
  Reclaimer& reclaimer = Reclaimer::GetInstance(hazard_pointer_list_);
  Node* new_tail = new Node();
  Node* old_tail = tail_.load(std::memory_order_relaxed);
  Node* temp;
  for (;;) {
    do {
      temp = old_tail;
      // Make sure the hazard pointer we set is tail
      reclaimer.MarkHazard(0, old_tail);
      old_tail = tail_.load(std::memory_order_acquire);
    } while (temp != old_tail);
    // Because we set the hazard pointer, so the old_tail can't be delete
    T* null_ptr = nullptr;
    if (old_tail->data.compare_exchange_strong(null_ptr, data_ptr,
                                               std::memory_order_release,
                                               std::memory_order_relaxed)) {
      if (!TryInsertNewTail(old_tail, new_tail)) {
        // Other help thread already insert tail
        delete new_tail;
      }
      reclaimer.MarkHazard(0, nullptr);
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
typename LockFreeQueue<T>::Node* LockFreeQueue<T>::InternalDequeue() {
  Reclaimer& reclaimer = Reclaimer::GetInstance(hazard_pointer_list_);
  Node* old_head = head_.load(std::memory_order_relaxed);
  Node* temp;
  do {
    do {
      // Make sure the hazard pointer we set is head
      temp = old_head;
      reclaimer.MarkHazard(0, old_head);
      old_head = head_.load(std::memory_order_relaxed);
    } while (temp != old_head);
    // Because we set the hazard pointer, so the old_head can't be delete
    if (tail_.load(std::memory_order_acquire) == old_head) {
      // Because old_head is dummy node, the queue is empty
      reclaimer.MarkHazard(0, nullptr);
      return nullptr;
    }
  } while (!head_.compare_exchange_weak(
      old_head, old_head->next.load(std::memory_order_acquire),
      std::memory_order_release, std::memory_order_relaxed));
  size_.fetch_sub(1, std::memory_order_relaxed);

  // So this thread is the only thread that can
  // delete old_head or push old_head to reclaim list
  reclaimer.MarkHazard(0, nullptr);
  return old_head;
}

template <typename T>
bool LockFreeQueue<T>::Dequeue(T& data) {
  Node* old_head = InternalDequeue();
  if (!old_head) return false;

  T* data_ptr = old_head->data.load(std::memory_order_acquire);
  data = std::move(*data_ptr);
  delete data_ptr;

  Reclaimer& reclaimer = Reclaimer::GetInstance(hazard_pointer_list_);
  reclaimer.ReclaimLater(old_head, LockFreeQueue<T>::OnDeleteNode);
  reclaimer.ReclaimNoHazardPointer();
  return true;
}

template <typename T>
bool LockFreeQueue<T>::Dequeue() {
  Node* old_head = InternalDequeue();
  if (!old_head) return false;

  T* data_ptr = old_head->data.load(std::memory_order_acquire);
  delete data_ptr;

  Reclaimer& reclaimer = Reclaimer::GetInstance(hazard_pointer_list_);
  reclaimer.ReclaimLater(old_head, LockFreeQueue<T>::OnDeleteNode);
  reclaimer.ReclaimNoHazardPointer();
  return true;
}

#endif  // LOCKFREE_QUEUE_H

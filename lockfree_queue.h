#ifndef LOCKFREE_QUEUE_H
#define LOCKFREE_QUEUE_H

#include <atomic>
#include "HazardPointer/reclaimer.h"

template <typename T>
class QueueReclaimer;

template <typename T>
class LockFreeQueue {
  static_assert(std::is_copy_constructible_v<T>, "T requires copy constructor");
  friend QueueReclaimer<T>;

 public:
  LockFreeQueue()
      : head_(new Node),
        tail_(head_.load(std::memory_order_relaxed)),
        size_(0) {}

  ~LockFreeQueue() {
    while (Dequeue())
      ;
    delete head_.load(std::memory_order_consume);
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
  size_t size() const { return size_.load(std::memory_order_consume); }

 private:
  struct Node;
  bool TryInsertNewTail(Node* old_tail, Node* new_tail) {
    Node* null_ptr = nullptr;
    if (old_tail->next.compare_exchange_strong(null_ptr, new_tail,
                                               std::memory_order_release,
                                               std::memory_order_relaxed)) {
      tail_.store(new_tail, std::memory_order_release);
      size_.fetch_add(1, std::memory_order_release);
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
  static Reclaimer::HazardPointerList global_hp_list_;
};

template <typename T>
Reclaimer::HazardPointerList LockFreeQueue<T>::global_hp_list_;

template <typename T>
class QueueReclaimer : public Reclaimer {
  friend LockFreeQueue<T>;

 private:
  QueueReclaimer(HazardPointerList& hp_list) : Reclaimer(hp_list) {}
  ~QueueReclaimer() override = default;

  static QueueReclaimer<T>& GetInstance() {
    thread_local static QueueReclaimer reclaimer(
        LockFreeQueue<T>::global_hp_list_);
    return reclaimer;
  }
};

template <typename T>
void LockFreeQueue<T>::InternalEnqueue(T* data_ptr) {
  QueueReclaimer<T>& reclaimer = QueueReclaimer<T>::GetInstance();
  Node* new_tail = new Node();
  Node* old_tail = tail_.load(std::memory_order_consume);
  Node* temp;
  for (;;) {
    HazardPointer hp;
    do {
      hp.UnMark();
      temp = old_tail;
      // Make sure the hazard pointer we set is tail
      hp = HazardPointer(&reclaimer, old_tail);
      old_tail = tail_.load(std::memory_order_consume);
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
  QueueReclaimer<T>& reclaimer = QueueReclaimer<T>::GetInstance();
  Node* old_head = head_.load(std::memory_order_consume);
  Node* temp;
  HazardPointer hp;
  do {
    do {
      hp.UnMark();
      // Make sure the hazard pointer we set is head
      temp = old_head;
      hp = HazardPointer(&reclaimer, old_head);
      old_head = head_.load(std::memory_order_consume);
    } while (temp != old_head);
    // Because we set the hazard pointer, so the old_head can't be delete
    if (tail_.load(std::memory_order_consume) == old_head) {
      // Because old_head is dummy node, the queue is empty
      return nullptr;
    }
  } while (!head_.compare_exchange_weak(
      old_head, old_head->next.load(std::memory_order_acquire),
      std::memory_order_release, std::memory_order_relaxed));
  size_.fetch_sub(1, std::memory_order_release);

  return old_head;
}

template <typename T>
bool LockFreeQueue<T>::Dequeue(T& data) {
  Node* old_head = InternalDequeue();
  if (!old_head) return false;

  T* data_ptr = old_head->data.load(std::memory_order_consume);
  data = std::move(*data_ptr);
  delete data_ptr;

  QueueReclaimer<T>& reclaimer = QueueReclaimer<T>::GetInstance();
  reclaimer.ReclaimLater(old_head, LockFreeQueue<T>::OnDeleteNode);
  reclaimer.ReclaimNoHazardPointer();
  return true;
}

template <typename T>
bool LockFreeQueue<T>::Dequeue() {
  Node* old_head = InternalDequeue();
  if (!old_head) return false;

  T* data_ptr = old_head->data.load(std::memory_order_consume);
  delete data_ptr;

  QueueReclaimer<T>& reclaimer = QueueReclaimer<T>::GetInstance();
  reclaimer.ReclaimLater(old_head, LockFreeQueue<T>::OnDeleteNode);
  reclaimer.ReclaimNoHazardPointer();
  return true;
}

#endif  // LOCKFREE_QUEUE_H

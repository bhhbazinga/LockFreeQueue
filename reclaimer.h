#ifndef RECLAIMER_H
#define RECLAIMER_H

#include <atomic>
#include <cassert>
#include <cstdio>
#include <functional>
#include <thread>
#include <unordered_set>

#define Log(...)                \
  fprintf(stderr, __VA_ARGS__); \
  fprintf(stderr, "\n")

struct HazardPointer {
  HazardPointer() : ptr(nullptr), next(nullptr) {}
  ~HazardPointer() {}

  HazardPointer(const HazardPointer& other) = delete;
  HazardPointer(HazardPointer&& other) = delete;
  HazardPointer& operator=(const HazardPointer& other) = delete;
  HazardPointer& operator=(HazardPointer&& other) = delete;

  std::atomic_flag flag;
  // We must use atomic pointer to ensure that the modification of pointer
  // is visible to other threads
  std::atomic<void*> ptr;
  HazardPointer* next;
};

struct HazardPointerList {
  HazardPointerList() : head(new HazardPointer()) {}
  ~HazardPointerList() {
    // HazardPointerList destruct when program exit
    HazardPointer* p = head.load(std::memory_order_acquire);
    while (p) {
      HazardPointer* temp = p;
      p = p->next;
      delete temp;
    }
  }

  std::atomic<HazardPointer*> head;
};

static HazardPointerList g_hazard_pointer_list;

class Reclaimer {
 public:
  static Reclaimer& GetInstance() {
    // Each thread has its own reclaimer
    thread_local static Reclaimer reclaimer;
    return reclaimer;
  }

  Reclaimer(const Reclaimer&) = delete;
  Reclaimer(Reclaimer&&) = delete;
  Reclaimer& operator=(const Reclaimer&) = delete;
  Reclaimer& operator=(Reclaimer&&) = delete;

  // Mark ptr as an hazard pointer
  // If ptr == nullptr then mask last ptr(that is hazard) as no hazard
  void MarkHazard(void* const ptr) {
    // TODO:Try to optimize memory order
    hazard_pointer_->ptr.store(ptr, std::memory_order_seq_cst);
  }

  // Check if the ptr is hazard
  bool Hazard(void* const ptr) {
    std::atomic<HazardPointer*>& head = g_hazard_pointer_list.head;
    HazardPointer* p = head.load(std::memory_order_relaxed);
    do {
      if (p->ptr.load(std::memory_order_seq_cst) == ptr) {
        return true;
      }
      p = p->next;
    } while (p);

    return false;
  }

  // If ptr is hazard then reclaim it later
  void ReclaimLater(void* const ptr, std::function<void(void*)>&& func) {
    ReclaimNode*& old_head = reclaim_list_.head;
    old_head->ptr = ptr;
    old_head->delete_func = std::move(func);

    ReclaimNode* new_head = reclaim_pool_.Pop();
    new_head->next = old_head;
    old_head = new_head;
  }

  // Try to reclaim all no hazard pointers
  void ReclaimNoHazardPointer() {
    ReclaimNode* pre = reclaim_list_.head;
    ReclaimNode* p = pre->next;
    while (p) {
      if (!Hazard(p->ptr)) {
        ReclaimNode* temp = p;
        p = pre->next = p->next;
        temp->delete_func(temp->ptr);
        reclaim_pool_.Push(temp);
      } else {
        pre = p;
        p = p->next;
      }
    }
  }

 private:
  Reclaimer() : hazard_pointer_(nullptr) {
    std::atomic<HazardPointer*>& head = g_hazard_pointer_list.head;
    HazardPointer* p = head.load(std::memory_order_acquire);
    do {
      // Try to get the idle hazard pointer
      if (!p->flag.test_and_set()) {
        hazard_pointer_ = p;
        break;
      }
      p = p->next;
    } while (p);

    if (nullptr == hazard_pointer_) {
      // No idle hazard pointer, allocate new one
      HazardPointer* new_head = new HazardPointer();
      new_head->flag.test_and_set();
      hazard_pointer_ = new_head;
      HazardPointer* old_head = head.load(std::memory_order_relaxed);
      do {
        new_head->next = old_head;
      } while (!head.compare_exchange_weak(old_head, new_head,
                                           std::memory_order_release,
                                           std::memory_order_relaxed));
    }

    assert(nullptr != hazard_pointer_);
  }

  ~Reclaimer() {
    // The Reclaimer destruct when the thread exit
    assert(nullptr == hazard_pointer_->ptr.load(std::memory_order_relaxed));

    // 1.Hand over the hazard pointer
    hazard_pointer_->flag.clear();

    // 2.reclaim all no hazard pointers
    ReclaimNode* p = reclaim_list_.head->next;
    while (p) {
      // Wait until p->ptr is no hazard
      // Maybe less efficient?
      while (Hazard(p->ptr)) {
        std::this_thread::yield();
      }
      ReclaimNode* temp = p;
      p = p->next;
      temp->delete_func(temp->ptr);
      delete temp;
    }
  }

  struct ReclaimNode {
    ReclaimNode() : ptr(nullptr), next(nullptr), delete_func(nullptr) {}
    ~ReclaimNode() {}

    void* ptr;
    ReclaimNode* next;
    std::function<void(void*)> delete_func;
  };

  struct ReclaimList {
    ReclaimList() : head(new ReclaimNode()) {}
    ~ReclaimList() { delete head; }

    ReclaimNode* head;
  };

  // Reuse ReclaimNode
  struct ReclaimPool {
    ReclaimPool() : head(new ReclaimNode()) {}
    ~ReclaimPool() {
      ReclaimNode* temp;
      while (head) {
        temp = head;
        head = head->next;
        delete temp;
      }
    }

    void Push(ReclaimNode* node) {
      node->next = head;
      head = node;
    }

    ReclaimNode* Pop() {
      if (nullptr == head->next) {
        return new ReclaimNode();
      }

      ReclaimNode* temp = head;
      head = head->next;
      temp->next = nullptr;
      return temp;
    }

    ReclaimNode* head;
  };

  HazardPointer* hazard_pointer_;
  std::unordered_set<ReclaimNode*> reclaim_set;
  ReclaimList reclaim_list_;
  ReclaimPool reclaim_pool_;
};
#endif  // RECLAIMER_H
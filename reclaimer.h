#ifndef RECLAIMER_H
#define RECLAIMER_H

#include <atomic>
#include <cassert>
#include <functional>
#include <thread>

// An estimate count that must be greater or equal than the max number of
// threads, you need to specify this number
#ifdef MAX_THREADS
const int kEstimateHazardPointerCount = MAX_THREADS;
#else
// defalut max number of threads
const int kEstimateHazardPointerCount = 64;
#endif

struct HazardPointer {
  HazardPointer() : ptr(nullptr) {}
  ~HazardPointer() {}

  HazardPointer(const HazardPointer& other) = delete;
  HazardPointer(HazardPointer&& other) = delete;
  HazardPointer& operator=(const HazardPointer& other) = delete;
  HazardPointer& operator=(HazardPointer&& other) = delete;

  std::atomic_flag flag;
  // We must use atomic pointer to ensure that the modification of pointer
  // is visible to other threads
  std::atomic<void*> ptr;
};

static HazardPointer g_hazard_pointers[kEstimateHazardPointerCount];

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
  void MarkHazard(void* const ptr) { hazard_pointer_->ptr.store(ptr); }

  // Check if the ptr is hazard
  bool Hazard(void* const ptr) {
    for (int i = 0; i < kEstimateHazardPointerCount; ++i) {
      if (g_hazard_pointers[i].ptr.load() == ptr) {
        return true;
      }
    }
    return false;
  }

  // If ptr is hazard then reclaim it later
  void ReclaimLater(void* const ptr, std::function<void(void)>&& func) {
    ReclaimNode*& old_head = reclaim_list_.head;
    old_head->ptr = ptr;
    old_head->delete_func = std::move(func);

    ReclaimNode* new_head = reclaim_pool_.Pop();
    new_head->next = old_head;
    old_head = new_head;
    ++reclaim_list_.size;
  }

  // Try to reclaim all no hazard pointers
  void ReclaimNoHazardPointer() {
    ReclaimNode* pre = reclaim_list_.head;
    ReclaimNode* p = pre->next;
    while (p) {
      if (!Hazard(p->ptr)) {
        ReclaimNode* temp = p;
        p = pre->next = p->next;
        temp->delete_func();
        reclaim_pool_.Push(temp);
        --reclaim_list_.size;
      } else {
        pre = p;
        p = p->next;
      }
    }
  }

 private:
  Reclaimer() : hazard_pointer_(nullptr) {
    for (int i = 0; i < kEstimateHazardPointerCount; ++i) {
      if (!g_hazard_pointers[i].flag.test_and_set()) {
        hazard_pointer_ = &g_hazard_pointers[i];
        assert(nullptr == hazard_pointer_->ptr);
        break;
      }
    }

    // If assertion satisfies, you should increase kEstimateHazardPointerCount
    assert(nullptr != hazard_pointer_);
  }

  ~Reclaimer() {
    // The Reclaimer destruct when the thread exit
    assert(nullptr == hazard_pointer_->ptr);

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
      temp->delete_func();
      delete temp;
    }
  }

  struct ReclaimNode {
    ReclaimNode() : ptr(nullptr), next(nullptr), delete_func(nullptr) {}
    ~ReclaimNode() {}

    void* ptr;
    ReclaimNode* next;
    std::function<void(void)> delete_func;
  };

  struct ReclaimList {
    ReclaimList() : head(new ReclaimNode()), size(0) {}
    ~ReclaimList() { delete head; }

    ReclaimNode* head;
    size_t size;
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

      // delete node;
    }

    ReclaimNode* Pop() {
      if (nullptr == head->next) {
        return new ReclaimNode();
      }

      ReclaimNode* temp = head;
      head = head->next;
      temp->next = nullptr;
      return temp;

      // return new ReclaimNode();
    }

    ReclaimNode* head;
  };

  HazardPointer* hazard_pointer_;
  ReclaimList reclaim_list_;
  ReclaimPool reclaim_pool_;
};
#endif  // RECLAIMER_H

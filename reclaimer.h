#ifndef RECLAIMER_H
#define RECLAIMER_H

#include <atomic>
#include <cassert>
#include <thread>

#include <cstdio>
#define Log(...)                                                  \
  fprintf(stderr, "[thread-%lu-%s]:", std::this_thread::get_id(), \
          __FUNCTION__);                                          \
  fprintf(stderr, __VA_ARGS__);                                   \
  fprintf(stderr, "\n")

template <typename T>
class Reclaimer {
 public:
  struct HazardPointer {
    HazardPointer() : ptr(nullptr) {}
    ~HazardPointer() {}

    std::atomic_flag flag;
    void* ptr;
  };

  static Reclaimer& GetInstance(
      std::shared_ptr<Reclaimer<T>::HazardPointer>* const hazard_pointers,
      const int count) {
    // Each thread has its own reclaimer
    thread_local static Reclaimer reclaimer(hazard_pointers, count);
    return reclaimer;
  }

  Reclaimer(const Reclaimer&) = delete;
  Reclaimer(Reclaimer&&) = delete;
  Reclaimer& operator=(const Reclaimer&) = delete;
  Reclaimer& operator=(Reclaimer&&) = delete;

  // Mark ptr as an hazard pointer
  // If ptr == nullptr then mask last ptr(that is hazard) as no hazard
  void MarkHazard(void* const ptr) { hazard_pointer_->ptr = ptr; }

  // Check if the ptr is hazard
  bool Hazard(void* const ptr) {
    for (int i = 0; i < hazard_pointer_count_; ++i) {
      if (hazard_pointers_[i]->ptr == ptr) {
        return true;
      }
    }
    return false;
  }

  // If ptr is hazard then reclaim it later
  void ReclaimLater(void* const ptr) {
    ReclaimNode*& old_head = reclaim_list.head;
    old_head->ptr = ptr;

    ReclaimNode* new_head = new ReclaimNode();
    new_head->next = old_head;
    old_head = new_head;
  }

  // Try to reclaim all no hazard pointers
  void ReclaimNoHazardPointer() {
    ReclaimNode* pre = reclaim_list.head;
    ReclaimNode* p = pre->next;
    while (p) {
      if (!Hazard(p->ptr)) {
        ReclaimNode* temp = p;
        p = pre->next = p->next;
        delete temp;
      } else {
        pre = p;
        p = p->next;
      }
    }
  }

 private:
  struct ReclaimNode {
    ReclaimNode() : ptr(nullptr), next(nullptr) {}
    ~ReclaimNode() {
      if (ptr != nullptr) {
        delete reinterpret_cast<T*>(ptr);
      }
    }

    void* ptr;
    ReclaimNode* next;
  };

  struct ReclaimList {
    ReclaimList() : head(new ReclaimNode) {}
    ~ReclaimList() { delete head; }

    ReclaimNode* head;
  };

  Reclaimer(std::shared_ptr<Reclaimer<T>::HazardPointer>* const hazard_pointers,
            const int count)
      : hazard_pointer_count_(count), hazard_pointers_(hazard_pointers) {
    hazard_pointer_ = nullptr;
    for (int i = 0; i < count; ++i) {
      if (!hazard_pointers[i]->flag.test_and_set()) {
        hazard_pointer_ = hazard_pointers[i];
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
    ReclaimNode* p = reclaim_list.head->next;
    while (p) {
      // Wait until p->ptr is no hazard
      // Maybe less efficient?
      while (Hazard(p->ptr)) {
        std::this_thread::yield();
      }
      ReclaimNode* temp = p;
      p = p->next;
      delete temp;
    }
  }

  const int hazard_pointer_count_;
  std::shared_ptr<Reclaimer<T>::HazardPointer> hazard_pointer_;
  const std::shared_ptr<Reclaimer<T>::HazardPointer>* const hazard_pointers_;
  ReclaimList reclaim_list;
};

#endif  // RECLAIMER_H

#ifndef __MEMORY_POOL_H__
#define __MEMORY_POOL_H__

#include <forward_list>
#include <vector>

#include "concurrentqueue.h" // lock-free thread-safe queue

#ifndef FIXED_POOL_BLOCK_COUNT
#define FIXED_POOL_BLOCK_COUNT 64
#endif

constexpr size_t kMinAlignment = 4;
constexpr size_t kDefaultBlockCount = FIXED_POOL_BLOCK_COUNT;
constexpr size_t kStackConsumeItems = 16;

class FixedPool {
  template <typename T> friend class PoolState;

private:
  using uchar = unsigned char;
  struct Header {
    uintptr_t pool_identifier;
    uint32_t next_block_idx;
  };

  inline uchar *AddrFromIndex(uint32_t i) const {
    // block = [(padding)(header)(data)]
    // returns &((header)(data))
    return (mem_start_ + (i * size_of_each_block_)) + pad_before_header_;
  }

  inline uint32_t IndexFromAddr(const uchar *p) const {
    // [(padding)(header)(data)]
    // p = &((header)(data))
    return (((uint32_t)((p - pad_before_header_) - mem_start_)) /
            (uint32_t)size_of_each_block_);
  }

  static inline Header *ReadHeader(void *p) {
    Header *h = reinterpret_cast<Header *>(reinterpret_cast<char *>(p) -
                                           sizeof(Header));
    return h;
  }

  FixedPool();
  FixedPool(uintptr_t id);
  void CreatePool(size_t size_of_each_block, uint32_t num_of_blocks);
  void DestroyPool();
  void *ForcedAllocate();
  void ForcedDeAllocate(void *p);

private:
  uint32_t num_of_blocks_;     // Num of blocks
  size_t size_of_each_block_;  // Size of each block
  uint32_t num_free_blocks_;   // Num of remaining blocks
  uint32_t num_initialized_;   // Num of initialized blocks
  uint32_t pad_before_header_; // Extra bytes allocated for alignment
  uchar *mem_start_;           // Beginning of memory pool
  uchar *next_;                // Num of next free block
  uintptr_t id_;               // Assigned id

public:
  static FixedPool *Create(size_t size_of_each_block, uint32_t num_of_blocks);
  static FixedPool *Create(uintptr_t id, size_t size_of_each_block,
                           uint32_t num_of_blocks);

  ~FixedPool();

  void *Allocate();
  void DeAllocate(void *p);
  void ReclaimAll();

  uint32_t GetNumOfBlocks() const { return num_of_blocks_; }
  uintptr_t GetId() const { return id_; }
  inline void *ToData(uchar *p) {
    return (void *)((size_t)(p) + sizeof(Header));
  }
  bool IsAnyBlockAvailable() const { return num_free_blocks_ > 0; }
  bool IsAnyBlockUsed() const {
    return (num_of_blocks_ - num_free_blocks_) > 0;
  }
  bool IsBlockUsed(size_t block_idx) const {
    return next_ == nullptr ||
           ((Header *)AddrFromIndex(block_idx))->next_block_idx ==
               num_of_blocks_ + 1;
  }
};

template <typename T> class PoolState {
private:
  struct InnerFixedPool {
    size_t id;
    uintptr_t owner_identifier;
    FixedPool *pool_instance;

    InnerFixedPool(size_t t_id, uintptr_t t_owner_identifier, size_t blocks)
        : id(t_id), owner_identifier(t_owner_identifier),
          pool_instance(FixedPool::Create((uintptr_t)this, sizeof(T), blocks)) {
    }
    ~InnerFixedPool() { delete pool_instance; }
  };

  std::vector<InnerFixedPool *> pools_;
  std::forward_list<size_t> free_pools_;

  // Lock-free thread-safe queue
  moodycamel::ConcurrentQueue<T *> dealloc_req_queue_;
  moodycamel::ConsumerToken consumer_token_;

public:
  PoolState() : consumer_token_(dealloc_req_queue_) { AddNewPool(); }
  ~PoolState() { Destroy(); }

  // Adds a new pool with specific number of **blocks**.
  size_t AddNewPool(size_t blocks = kDefaultBlockCount) {
    size_t pool_id = pools_.size();

    auto inner_pool = new InnerFixedPool(pool_id, (uintptr_t)this, blocks);
    pools_.push_back(inner_pool);
    free_pools_.push_front(pool_id);
    return pool_id;
  }

  // Returns the active pool which has free block(s) or adds/creates a new pool
  // and returns it.
  FixedPool *GetActivePool() {
    size_t current_pool_id = free_pools_.front();
    FixedPool *active_pool = pools_[current_pool_id]->pool_instance;
    if (active_pool->IsAnyBlockAvailable()) {
      return active_pool;
    } else {
      // We should do synchronization overhead stuff only when we really need
      // it.
      ConsumeDeallocRequests();
      if (active_pool->IsAnyBlockAvailable()) {
        return active_pool;
      }
      free_pools_.pop_front();
    }

    if (!free_pools_.empty()) {
      current_pool_id = free_pools_.front();

      active_pool = pools_[current_pool_id]->pool_instance;
      return active_pool;
    }

    current_pool_id = AddNewPool();
    return pools_[current_pool_id]->pool_instance;
  }

  template <typename... Args> T *New(Args &&...args) {
    FixedPool *pool = GetActivePool();

    void *space = pool->ForcedAllocate();
    return new (space) T(std::forward<Args>(args)...);
  }

  // Tries to dealloc the given instance and always calls the destructor.
  //
  // Safety: The object `instance` must've been created using the
  // `PoolState::New` function.
  void Delete(T *instance) {
    InnerFixedPool *inner_pool =
        (InnerFixedPool *)FixedPool::ReadHeader((void *)instance)
            ->pool_identifier;
    PoolState<T> *pool_owner = (PoolState<T> *)inner_pool->owner_identifier;
    // We're stating our intention that we're basically checking if a moved
    // object to a different thread has requested to deallocate some space.
    if (this != pool_owner) {
      // Calling the destructor before actually deallocating the reserved
      // memory. So the current thread can act as if the object has been freed.
      instance->~T();
      pool_owner->AddDeallocRequest(instance);
      return;
    }
    SetNextFreePool(inner_pool->id);

    // calling destructor
    instance->~T();
    inner_pool->pool_instance->ForcedDeAllocate((void *)instance);
  }

  // Shouldn't be called in a busy loop.
  void Clear() {
    for (size_t i = pools_.size() - 1; i >= 0; i--) {
      auto &pool = pools_[i];
      if (!pool->pool_instance->IsAnyBlockAvailable()) {
        free_pools_.push_front(i);
      }

      DeAllocateUsedBlocks(pool->pool_instance);
      pool->pool_instance->ReclaimAll();
    }
  }

private:
  inline void Destroy() {
    for (auto &pool : pools_) {
      DeAllocateUsedBlocks(pool->pool_instance);
      delete pool;
    }
  }

  // Must be called before `FixedPool::ForcedDeAllocate` gets called. Since
  // we're kind of checking if it is already in the "free pools" list.
  inline void SetNextFreePool(size_t pool_id) {
    if (pools_[pool_id]->pool_instance->IsAnyBlockAvailable())
      return;

    free_pools_.push_front(pool_id);
  }

  inline void DeAllocateUsedBlocks(FixedPool *pool) {
    // Since everything is being "reclaimed" why not just do some thread
    // syncronization and reclaim spaces moved to another thread.
    //
    // That's why the call to this implicit function shouldn't happen in a busy
    // loop.
    ConsumeDeallocRequests();

    for (size_t i = 0, blocks_count = pool->GetNumOfBlocks(); i < blocks_count;
         i++) {
      if (!pool->IsBlockUsed(i))
        continue;
      T *instance = (T *)pool->ToData(pool->AddrFromIndex(i));
      instance->~T();

      pool->ForcedDeAllocate((void *)instance);
    }
  }

  inline void AddDeallocRequest(T *data) { dealloc_req_queue_.enqueue(data); }

  inline void ConsumeDeallocRequests() {
    size_t count = 0;
    do {
      T *items[kStackConsumeItems];
      count = dealloc_req_queue_.try_dequeue_bulk(consumer_token_, items,
                                                  kStackConsumeItems);
      for (size_t i = 0; i < count; i++) {
        T *instance = items[i];
        InnerFixedPool *inner_pool =
            (InnerFixedPool *)FixedPool::ReadHeader((void *)instance)
                ->pool_identifier;
        FixedPool *pool = inner_pool->pool_instance;

        SetNextFreePool(inner_pool->id);
        // We're not calling the destructor, we've already called it. We're just
        // now reclaiming the reserved space as we need it.
        //
        // Safety: The object should've been moved to the *other* thread which
        // has requested deallocating/deleting this object.
        pool->ForcedDeAllocate((void *)instance);
      }
    } while (count > 0);
  }
};

class PoolManager {
public:
  PoolManager(const PoolManager &) = delete;
  PoolManager(const PoolManager &&) = delete;
  PoolManager &operator=(const PoolManager &) = delete;
  PoolManager &operator=(const PoolManager &&) = delete;

  // `thread_local`'s language implementation guarantees that the destructor
  // will be called when the thread is exitting...
  template <typename T, typename... Args> inline static PoolState<T> &Get() {
    static thread_local PoolState<T> t_pool_state = PoolState<T>();
    return t_pool_state;
  }
};

namespace pool {
template <typename T, typename... Args> inline T *New(Args &&...args) {
  return PoolManager::Get<T>().New(std::forward<Args>(args)...);
}

template <typename T> inline void Delete(T *instance) {
  PoolManager::Get<T>().Delete(instance);
}
}; // namespace pool

#endif //__MEMORY_POOL_H__

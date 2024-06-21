#ifndef __MEMORY_POOL_H__
#define __MEMORY_POOL_H__

#include <limits>
#include <memory>
#include <thread>
#include <vector>

#include "concurrentqueue.h" // lock-free thread-safe queue

#ifndef FIXED_POOL_BLOCK_COUNT
#define FIXED_POOL_BLOCK_COUNT 64
#endif

constexpr size_t kMinAlignment = 4;
constexpr size_t kDefaultBlockCount = FIXED_POOL_BLOCK_COUNT;
constexpr size_t kStackConsumeItems = 16;

class FixedPool {
  using uchar = unsigned char;
  struct Header {
    uintptr_t pool_identifier;
    uint32_t next_block_idx;
  };
  template <typename T> friend class PoolManager;

private:
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

  FixedPool(size_t id, uintptr_t owner_identifier);
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
  size_t id_;                  // Assigned id
  uintptr_t owner_identifier_; // Owner identifier
  bool *used_blocks_;          // Used blocks

public:
  static FixedPool *Create(size_t size_of_each_block, uint32_t num_of_blocks);
  static FixedPool *Create(size_t id, uintptr_t owner,
                           size_t size_of_each_block, uint32_t num_of_blocks);
  static inline FixedPool *GetPoolFromData(void *p) {
    return (FixedPool *)ReadHeader(p)->pool_identifier;
  }

  FixedPool() = delete;
  ~FixedPool();

  void *Allocate();
  void DeAllocate(void *p);
  void ReclaimAll();

  uint32_t GetNumOfBlocks() const { return num_of_blocks_; }
  size_t GetId() const { return id_; }
  size_t GetOwnerIdentfier() const { return owner_identifier_; }
  inline void *ToData(uchar *p) {
    return (void *)((size_t)(p) + sizeof(Header));
  }
  bool IsAnyBlockAvailable() const { return num_free_blocks_ > 0; }
  bool IsAnyBlockUsed() const {
    return (num_of_blocks_ - num_free_blocks_) > 0;
  }
  bool IsBlockUsed(size_t block_idx) const { return used_blocks_[block_idx]; }
};

template <typename T> class PoolManager {
  struct InnerFixedPoolInfo {
    FixedPool *pool_instance;
    size_t next_pool_id;
  };
  friend class GlobalPoolManager;

private:
  inline static thread_local PoolManager *shared_instance_ = nullptr;
  std::vector<InnerFixedPoolInfo> pools_;
  size_t current_pool_id_;

  // Lock-free thread-safe queue
  moodycamel::ConcurrentQueue<T *> dealloc_req_queue_;
  moodycamel::ConsumerToken consumer_token_;

private:
  PoolManager();

public:
  static PoolManager *GetInstance() {
    if (shared_instance_ == nullptr) {
      shared_instance_ = new PoolManager();
    }
    return shared_instance_;
  }

  // Adds a new pool with specific number of **blocks**.
  size_t AddNewPool(size_t blocks = kDefaultBlockCount) {
    size_t pool_id = pools_.size();
    FixedPool *pool =
        FixedPool::Create(pool_id, (uintptr_t)this, sizeof(T), blocks);

    pools_.emplace_back(pool, pool_id + 1);
    return pool_id;
  }

  // Returns the active pool which has free block(s) or adds/creates a new pool
  // and returns it.
  InnerFixedPoolInfo &GetActivePool() {
    InnerFixedPoolInfo &active_pool = pools_[current_pool_id_];
    if (active_pool.pool_instance->IsAnyBlockAvailable()) {
      return active_pool;
    } else {
      // We should do synchronization overhead stuff only when we really need
      // it.
      ConsumeDeallocRequests();
      if (active_pool.pool_instance->IsAnyBlockAvailable()) {
        return active_pool;
      }
    }

    current_pool_id_ = active_pool.next_pool_id;
    if (current_pool_id_ == pools_.size()) {
      (void)AddNewPool();
    }

    return pools_[current_pool_id_];
  }

  template <typename... Args> T *New(Args &&...args) {
    InnerFixedPoolInfo &pool = GetActivePool();

    void *space = pool.pool_instance->ForcedAllocate();
    return new (space) T(std::forward<Args>(args)...);
  }

  // Tries to dealloc the given instance and always calls the destructor.
  //
  // Safety: The object `instance` must've been created using the
  // `PoolManager::New` function.
  void Delete(T *instance) {
    FixedPool *pool = FixedPool::GetPoolFromData((void *)instance);
    PoolManager<T> *pool_owner = (PoolManager<T> *)pool->GetOwnerIdentfier();
    // Since `shared_instance_` is marked as `thread_local` we're stating our
    // intention that we're basically checking if a moved object to a different
    // thread has requested to deallocate some space.
    if (shared_instance_ != pool_owner) {
      // Calling the destructor before actually deallocating the reserved
      // memory.
      instance->~T();
      pool_owner->AddDeallocRequest(instance);
      return;
    }
    size_t pool_id = pool->GetId();
    // calling destructor
    instance->~T();
    pool->ForcedDeAllocate((void *)instance);

    SetNextFreePool(pool_id);
  }

  // Shouldn't be called in a busy loop.
  void Clear() {
    for (auto &pool : pools_) {
      DeAllocateUsedBlocks(pool);
      pool.pool_instance->ReclaimAll();
    }
    current_pool_id_ = 0;
  }

private:
  // User should call `GlobalPoolManager::DestroyAll` when the main thread is
  // exiting.
  //
  // Safety: Since Main thread will call it implcitly while exiting, no other
  // thread should hold any of the `PoolManager` instances.
  static void DestroyManager(void *ptr) {
    PoolManager<T> *thiz = (PoolManager<T> *)ptr;

    for (auto &pool : thiz->pools_) {
      thiz->DeAllocateUsedBlocks(pool);
      delete pool.pool_instance;
    }

    delete thiz;
  }

  inline void SetNextFreePool(size_t pool_id) {
    if (pool_id == current_pool_id_)
      return;
    // We're basically swapping around...
    InnerFixedPoolInfo &pool_info = pools_[pool_id];
    InnerFixedPoolInfo &current_pool_info = pools_[current_pool_id_];

    current_pool_info.next_pool_id = pool_info.next_pool_id;
    pool_info.next_pool_id = current_pool_id_;

    current_pool_id_ = pool_id;
  }

  inline void DeAllocateUsedBlocks(const InnerFixedPoolInfo &pool) {
    // Since everything is being "reclaimed" why not just do some thread
    // syncronization and reclaim spaces moved to another thread.
    //
    // That's why the call to this implicit function shouldn't happen in a busy
    // loop.
    ConsumeDeallocRequests();

    FixedPool *pool_instance = pool.pool_instance;
    for (size_t i = 0, blocks_count = pool_instance->GetNumOfBlocks();
         i < blocks_count; i++) {
      if (!pool_instance->IsBlockUsed(i))
        continue;
      T *instance = (T *)pool_instance->ToData(pool_instance->AddrFromIndex(i));
      instance->~T();

      pool_instance->ForcedDeAllocate((void *)instance);
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
        FixedPool *pool = FixedPool::GetPoolFromData((void *)instance);
        size_t pool_id = pool->GetId();
        // We're not calling the destructor, we've already called it. We're just
        // now reclaiming the reserved space as we need it.
        //
        // Safety: The object should've been moved to the *other* thread which
        // has requested deallocating/deleting this object.
        pool->ForcedDeAllocate((void *)instance);

        SetNextFreePool(pool_id);
      }
    } while (count > 0);
  }
};

class GlobalPoolManager {
  struct PoolManagerContext {
    using DestroyFunc = void (*)(void *thiz);

    void *instance;
    DestroyFunc destroy_func;
  };

private:
  inline static std::shared_ptr<GlobalPoolManager> shared_instance_ = nullptr;

  // Lock-free thread-safe queue
  moodycamel::ConcurrentQueue<PoolManagerContext> pool_mgr_queue_;
  moodycamel::ConsumerToken consumer_token_;

public:
  // Initializes the shared instance of `GlobalPoolManager`.
  //
  // Safety: Should be called by the main thread.
  static void Init() {
    if (shared_instance_ != nullptr)
      return;
    shared_instance_ = std::make_shared<GlobalPoolManager>();
  }

  // Returns the shared instance.
  //
  // Safety: `GlobalPoolManager::Init` should be called by the main thread
  // before any other threads starts calling it.
  static std::shared_ptr<GlobalPoolManager> GetInstance() {
    return shared_instance_;
  }

  // Should be called by the Main thread before exiting.
  // Shouldn't be called in a busy loop.
  //
  // Safety: Since it's intended to be called when main thread is exiting, no
  // other thread should have any reference to it.
  static void DestroyAll() {
    std::shared_ptr<GlobalPoolManager> thiz = GetInstance();

    if (thiz == nullptr)
      return;

    size_t count = 0;
    do {
      PoolManagerContext items[kStackConsumeItems];
      count = thiz->pool_mgr_queue_.try_dequeue_bulk(thiz->consumer_token_,
                                                     items, kStackConsumeItems);
      for (size_t i = 0; i < count; i++) {
        PoolManagerContext &mgr = items[i];
        mgr.destroy_func(mgr.instance);
      }
    } while (count > 0);

    shared_instance_.reset();
    shared_instance_ = nullptr;
  }

  GlobalPoolManager() : consumer_token_(pool_mgr_queue_) {}

  // Registers a `PoolManager<T>` instance to it's *queue*.
  //
  // When `GlobalPoolManager::DestroyAll` is called
  // `PoolManager<T>::DestroyManager` will be called and it's first argument
  // will be the *instance*.
  template <typename T> void RegisterPoolManager(PoolManager<T> *instance) {
    pool_mgr_queue_.enqueue(
        PoolManagerContext{(void *)instance, PoolManager<T>::DestroyManager});
  }
};

// `GlobalPoolManager` needs to be accessed by the PoolManager
template <class T>
PoolManager<T>::PoolManager()
    : current_pool_id_(0), consumer_token_(dealloc_req_queue_) {
  // Add a single pool first which will have 64 blocks.
  AddNewPool();
  GlobalPoolManager::GetInstance()->RegisterPoolManager(this);
}

namespace pool {
template <typename T, typename... Args> inline T *New(Args &&...args) {
  return PoolManager<T>::GetInstance()->New(std::forward<Args>(args)...);
}

template <typename T> inline void Delete(T *instance) {
  PoolManager<T>::GetInstance()->Delete(instance);
}
}; // namespace pool

#endif //__MEMORY_POOL_H__

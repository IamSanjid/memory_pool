#ifndef __MEMORY_POOL_H__
#define __MEMORY_POOL_H__

#include <vector>

#include "concurrentqueue.h" // lock-free thread-safe queue
#include "fixed_pool.h"

// Notes: We cannot identify if an object has been moved to another thread, well
// there isn't even any concept of moving objects, we just return the pointer of
// an allocated object, but the thing is user might want to return an object
// from a thread to another thread(which is valid C/C++ language thing to do)
// so it kind of doesn't make sense to deallocate that object when the object
// creator thread is exiting. So we don't deallocate any object if the current
// thread is exiting, we're using static storage mechanism of the language to
// only delete all the objects when static storage decides it's time to end it.
//
// We could kind of write some cheap wrappers around that raw pointer but then
// again why not just use stl smart pointers with custom deleter to use
// `pool::Delete` when deleting the object?
//
// Just realized that we can't really write a low overhead *memory allocator* to
// address every use-case, in this implementation already allocating a lot of
// bytes unrelated to the specific "object" allocation, just to make it
// thread-safe, no-loop during allocation/deallocation of an object.

template <typename T> class PoolManager;

template <typename T> class Pool {
private:
  friend class PoolManager<T>;
  struct InnerFixedPool {
    size_t id;
    size_t next_id;
    uintptr_t owner_identifier;
    FixedPool *pool_instance;

    InnerFixedPool(size_t t_id, size_t t_next_id, uintptr_t t_owner_identifier,
                   size_t blocks)
        : id(t_id), next_id(t_next_id), owner_identifier(t_owner_identifier),
          pool_instance(FixedPool::Create((uintptr_t)this, sizeof(T), blocks)) {
    }
    ~InnerFixedPool() { delete pool_instance; }
  };

  // Represents the `Pool<T>`'s moveable state.
  struct PoolState {
    // Lock-free thread-safe queue
    moodycamel::ConcurrentQueue<T *> dealloc_req_queue;
    moodycamel::ConsumerToken consumer_token;

    InnerFixedPool *next_pool;
    std::vector<InnerFixedPool *> pools;
    size_t num_free_pools;

    PoolState()
        : consumer_token(dealloc_req_queue),
          next_pool(
              new InnerFixedPool(0, 1, (uintptr_t)this, kDefaultBlockCount)),
          pools({next_pool}), num_free_pools(1) {}

    inline InnerFixedPool *AddNewPool() {
      size_t pool_id = pools.size();

      InnerFixedPool *inner_pool = new InnerFixedPool(
          pool_id, pool_id + 1, (uintptr_t)this, kDefaultBlockCount);
      pools.push_back(inner_pool);

      next_pool = inner_pool;
      num_free_pools++;

      return next_pool;
    }

    // Must be called before `FixedPool::ForcedDeAllocate` gets called. Since
    // we're kind of checking if it is already in the "free pools" list.
    inline void SetNextFreePool(InnerFixedPool *pool) {
      if (pool->pool_instance->IsAnyBlockAvailable())
        return;
      num_free_pools++;
      pool->next_id = next_pool->id;
      next_pool = pool;
    }

    inline void AddDeallocRequest(T *data) { dealloc_req_queue.enqueue(data); }
  };

private:
  PoolState *state_ = nullptr;

public:
  // `thread_local`'s language implementation guarantees that the destructor
  // will be called when the thread is exitting...
  inline static Pool &Instance() {
    static thread_local Pool instance = Pool();
    return instance;
  }

  Pool() { Init(); }

  Pool(const Pool &) = delete;
  Pool &operator=(const Pool &) = delete;

  ~Pool() { Destroy(); }

  // Creates a new T object and initializes with all the required constructor
  // arguments.
  //
  // The object's life time if not deleted by calling `Pool<T>::Delete` is until
  // the main process/thread exits.
  template <typename... Args> T *New(Args &&...args) {
    FixedPool *pool = GetActiveFixedPool();

    void *space = pool->ForcedAllocate();
    return new (space) T(std::forward<Args>(args)...);
  }

  // Tries to dealloc the given instance and always calls the destructor.
  //
  // Safety: The object `instance` must've been created using the
  // `Pool::New` function.
  void Delete(T *instance) {
    InnerFixedPool *inner_pool =
        (InnerFixedPool *)FixedPool::ReadHeader((void *)instance)
            ->pool_identifier;
    PoolState *pool_owner_state = (PoolState *)inner_pool->owner_identifier;
    // We're stating our intention that we're basically checking if a *moved*
    // object to a different thread has requested to deallocate some space.
    // We assume that object has been *moved* to current thread. And the thread
    // who created this object doesn't own it anymore.
    if (state_ != pool_owner_state) {
      // Calling the destructor before actually deallocating the reserved
      // memory. So the current thread can act as if the object has been freed.
      instance->~T();
      pool_owner_state->AddDeallocRequest(instance);
      return;
    }
    state_->SetNextFreePool(inner_pool);

    instance->~T();
    inner_pool->pool_instance->ForcedDeAllocate((void *)instance);
  }

  // Reclaims all the allocated space for reuse.
  // Calls all the allocated object's destructor.
  void Clear() {
    std::vector<InnerFixedPool *> &pools = state_->pools;

    ConsumeDeallocRequests();
    for (size_t i = 0, n = pools.size(); i < n; i++) {
      auto &pool = pools[i];

      pool->id = i;
      pool->next_id = i + 1;

      DeleteObjectsFromPool(pool->pool_instance);
    }
  }

private:
  // Calls object's destructor.
  static inline void DeleteObjectsFromPool(FixedPool *pool) {
    for (size_t i = 0, blocks_count = pool->GetNumOfBlocks(); i < blocks_count;
         i++) {
      if (!pool->IsBlockUsed(i))
        continue;
      T *instance = (T *)pool->ToData(pool->AddrFromIndex(i));
      instance->~T();

      pool->ForcedDeAllocate((void *)instance);
    }
  }

  static inline void DeleteState(PoolState *state) {
    std::vector<InnerFixedPool *> &pools = state->pools;
    for (auto &pool : pools) {
      DeleteObjectsFromPool(pool->pool_instance);
      delete pool;
    }
    delete state;
  }

  void Init();
  void Destroy();

  // Returns the active pool which has free block(s) or adds/creates a new pool
  // and returns it.
  inline FixedPool *GetActiveFixedPool() {
    InnerFixedPool *active_pool = state_->next_pool;
    FixedPool *active_fixed_pool = active_pool->pool_instance;
    if (active_fixed_pool->IsAnyBlockAvailable()) {
      return active_fixed_pool;
    }

    // We should do synchronization overhead stuff only when we really need
    // it.
    ConsumeDeallocRequests();
    if (active_fixed_pool->IsAnyBlockAvailable()) {
      return active_fixed_pool;
    }

    state_->num_free_pools--;
    if (state_->num_free_pools > 0) {
      InnerFixedPool *next_pool = state_->pools[active_pool->next_id];
      state_->next_pool = next_pool;
      active_pool = next_pool;
    } else {
      active_pool = state_->AddNewPool();
    }

    return active_pool->pool_instance;
  }

  inline void ConsumeDeallocRequests() {
    size_t count = 0;
    T *items[kStackConsumeItems];
    do {
      count = state_->dealloc_req_queue.try_dequeue_bulk(
          state_->consumer_token, items, kStackConsumeItems);
      for (size_t i = 0; i < count; i++) {
        T *instance = items[i];
        InnerFixedPool *inner_pool =
            (InnerFixedPool *)FixedPool::ReadHeader((void *)instance)
                ->pool_identifier;
        FixedPool *pool = inner_pool->pool_instance;

        state_->SetNextFreePool(inner_pool);
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

template <typename T> class PoolManager {
private:
  friend class Pool<T>;
  using PoolState = Pool<T>::PoolState;

  // Lock-free thread-safe queue
  moodycamel::ConcurrentQueue<PoolState *> free_pools_{};

public:
  PoolManager() = default;

  PoolManager(const PoolManager &) = delete;
  PoolManager &operator=(const PoolManager &) = delete;

  ~PoolManager() {
    // Program is exiting normally without exceptions/errors...
    size_t count = 0;
    PoolState *items[kStackConsumeItems];
    do {
      count = free_pools_.try_dequeue_bulk(items, kStackConsumeItems);
      for (size_t i = 0; i < count; i++) {
        Pool<T>::DeleteState(items[i]);
      }
    } while (count > 0);
  }

  // Gets a thread_local `Pool` instance.
  inline static Pool<T> &Get() { return Pool<T>::Instance(); }

private:
  static PoolManager &Instance() {
    static PoolManager instance = PoolManager();
    return instance;
  }

  void AddFreePool(PoolState *pool) { free_pools_.enqueue(pool); }

  PoolState *GetFreePool() {
    PoolState *pool = nullptr;
    if (free_pools_.try_dequeue(pool)) {
      return pool;
    }
    return nullptr;
  }
};

// Needs to access `PoolManager`
template <typename T> void Pool<T>::Init() {
  PoolState *state = PoolManager<T>::Instance().GetFreePool();
  if (state == nullptr)
    state = new PoolState();
  else if (state->num_free_pools == 0)
    (void)state->AddNewPool();

  state_ = state;
}

template <typename T> void Pool<T>::Destroy() {
  ConsumeDeallocRequests();
  PoolManager<T>::Instance().AddFreePool(state_);
}

namespace pool {
template <typename T, typename... Args> inline T *New(Args &&...args) {
  return Pool<T>::Instance().New(std::forward<Args>(args)...);
}

template <typename T> inline void Delete(T *instance) {
  Pool<T>::Instance().Delete(instance);
}
}; // namespace pool

#endif //__MEMORY_POOL_H__

# Extended Fixed-Sized-Memory-Pool
Extended this paper's algorithm: https://arxiv.org/pdf/2210.16471

Things tried to add:
  * Adds new *Fixed Sized Pool* automatically when the previous pool is full.
  * Thread safe? Used `thread_local`, every thread should've an instance of a 
    `PoolManager`. Objects can be moved to different threads, the correct `PoolManager`
    instance will deallocate it when `Delete` is requested for that object.
  * Global functions are available to be called by main thread to automatically
    free objects and call their destructor. This applies to all the `PoolManager`(s)
    created by different threads.

# Concerns
- It spends too much time on syscalls? ([More Details](benchmarks/README.md#perf))
And I don't know why, meaning I actually don't know what my program really doing.
Just added few theories here and there.

- Is it worth? Probably not for many cases. It's definately not good for containers
like `std::vector`. It's probably good when multiple objects needs to be 
created/deleted quite often, it will probably miss less cpu cache-line than usual
mallocs, so, faster memory access?

- Not sure but the way(original paper) *Deallocates* memory the old object's memory
might retain and the new allocated object memory might still contain those but
again if we deallocate through `PoolManager::Delete` the object's destructor is
called which might clean it up.

# Logic Error
[This Line](https://github.com/IamSanjid/memory_pool/blob/logic-error/memory_pool.h#L181) had a silly but huge logic error. Say [`pools_`](https://github.com/IamSanjid/memory_pool/blob/logic-error/memory_pool.h#L102) has two pools their ids/indecies are `0,1`. First an object from the 0th/`pools_[0]` gets destroyed, so [`current_pool_id_`](https://github.com/IamSanjid/memory_pool/blob/logic-error/memory_pool.h#L103) becoms 0. Now think of a scenerio this 0th pool has few free blocks but then an object from the 1th/`pools_[1]` gets destroyed so `current_pool_id_` becomes `1`, but there is nothing which is keeping track that 0th pool has some free blocks, so when this 1th pool gets out of space nothing goes back and uses previously allocated 0th pool's space, so it almost becomes a huge memory leak.

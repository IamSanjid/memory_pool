# Extended Fixed-Sized-Memory-Pool
Extended this paper's algorithm: https://arxiv.org/pdf/2210.16471

Things tried to add:
  * Adds new *Fixed Sized Pool* automatically when the previous pool is full.
  * Thread safe? Used `thread_local`, every thread should've an instance of a 
    `PoolState`. Objects can be moved to different threads, the correct `PoolState`
    instance will deallocate it when `Delete` is requested for that object.
  * For *copying* objects, `pools::New<ObjType>(/* existing object */)` can be used.
    Maybe should introduce `pools::Copy` to make it more obvious?
  * Every `PoolState` gets destroyed at when the thread owning it exits, C++'s
    `thread_local` implementation gauruntees it.

# Concerns
- Is it worth? Probably not for many cases. It's definately not good for containers
like `std::vector`(in theory you can use `PoolState` as custom allocator for these
conatiners). It's probably good when multiple objects needs to be 
created/deleted quite often, it will probably miss less cpu cache-line than usual
mallocs, so, faster memory access?

- Not sure but the way(original paper) *Deallocates* memory the old object's memory
might retain and the new allocated object memory might still contain those but
again if we deallocate through `pools::Delete` the object's destructor is called 
which might clean it up. Have to keep an eye on `FixedPool::ReclaimAll` if 
we really need those `memset`(s) there.

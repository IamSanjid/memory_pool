#include "memory_pool.h"

#include <cassert>
#include <cstring>

static inline bool IsAligned(size_t val, size_t alignment) {
  size_t lowBits = val & (alignment - 1);
  return (lowBits == 0);
}

static inline size_t Align(size_t val, size_t alignment) {
  assert((alignment & (alignment - 1)) == 0 &&
         "Invalid alignment. Must be power of two.");
  size_t r = (val + (alignment - 1)) & ~(alignment - 1);
  assert(IsAligned(r, alignment) && "Alignment failed.");
  return r;
}

FixedPool *FixedPool::Create(size_t size_of_each_block,
                             uint32_t num_of_blocks) {
  FixedPool *instance = new FixedPool();
  instance->CreatePool(size_of_each_block, num_of_blocks);

  return instance;
}

FixedPool *FixedPool::Create(uintptr_t id, size_t size_of_each_block,
                             uint32_t num_of_blocks) {
  FixedPool *instance = new FixedPool(id);
  instance->CreatePool(size_of_each_block, num_of_blocks);

  return instance;
}

FixedPool::FixedPool() : FixedPool((uintptr_t)this) {}

FixedPool::FixedPool(uintptr_t id)
    : num_of_blocks_(0), size_of_each_block_(0), num_free_blocks_(0),
      num_initialized_(0), pad_before_header_(0), mem_start_(nullptr),
      next_(nullptr), id_(id) {}

FixedPool::~FixedPool() { DestroyPool(); }

void FixedPool::CreatePool(size_t size_of_each_block, uint32_t num_of_blocks) {
  num_of_blocks_ = num_of_blocks;

  size_t size_of_each_block_with_header = sizeof(Header) + size_of_each_block;
  size_t alignedSizeOfEachBlock =
      Align(size_of_each_block_with_header, kMinAlignment);
  pad_before_header_ = alignedSizeOfEachBlock - size_of_each_block_with_header;
  size_of_each_block_ = alignedSizeOfEachBlock;

  mem_start_ = new uchar[size_of_each_block_ * num_of_blocks_];
  ReclaimAll();
}

void FixedPool::DestroyPool() {
  delete[] mem_start_;
  mem_start_ = nullptr;
}

void *FixedPool::ForcedAllocate() {
  if (num_initialized_ < num_of_blocks_) {
    Header *h = (Header *)AddrFromIndex(num_initialized_);
    h->next_block_idx = num_initialized_ + 1;
    h->pool_identifier = id_;
    num_initialized_++;
  }
  void *ret = ToData(next_);

  --num_free_blocks_;
  if (num_free_blocks_ != 0) {
    uint32_t &next_idx = ((Header *)next_)->next_block_idx;

    next_ = AddrFromIndex(next_idx);

    // Marking as used..
    next_idx = num_of_blocks_ + 1;
  } else {
    next_ = nullptr;
  }

  return ret;
}

void *FixedPool::Allocate() {
  if (num_free_blocks_ == 0) {
    return nullptr;
  }
  return ForcedAllocate();
}

void FixedPool::ForcedDeAllocate(void *p) {
  Header *h = ReadHeader(p);

  if (next_ != nullptr) {
    h->next_block_idx = IndexFromAddr(next_);
    // block = [(padding)(header)(data)]
    // next_ = &((header)(data))
    next_ = (uchar *)h;
  } else {
    // Doesn't matter what we set the next block index it kind of acts like the
    // end block
    h->next_block_idx = num_of_blocks_;
    next_ = (uchar *)h;
  }
  ++num_free_blocks_;
}

void FixedPool::DeAllocate(void *p) {
  Header *h = ReadHeader(p);
  if (h->pool_identifier != id_)
    return;

  ForcedDeAllocate(p);
}

inline void FixedPool::ReclaimAll() {
  num_initialized_ = 0;
  num_free_blocks_ = num_of_blocks_;
  std::memset(mem_start_, 0xf,
              size_of_each_block_ * num_free_blocks_ * sizeof(uchar));

  next_ = (uchar *)AddrFromIndex(0);
}

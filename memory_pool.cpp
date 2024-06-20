#include <cassert>
#include <cstring>

#include "memory_pool.h"

constexpr size_t kDefaultPoolId = std::numeric_limits<size_t>::max();

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
  return Create(kDefaultPoolId, 0, size_of_each_block, num_of_blocks);
}

FixedPool *FixedPool::Create(size_t id, uintptr_t owner,
                             size_t size_of_each_block,
                             uint32_t num_of_blocks) {
  FixedPool *instance = new FixedPool(id, owner);
  instance->CreatePool(size_of_each_block, num_of_blocks);

  return instance;
}

FixedPool::FixedPool(size_t id, uintptr_t owner_identfier)
    : num_of_blocks_(0), size_of_each_block_(0), num_free_blocks_(0),
      num_initialized_(0), pad_before_header_(0), mem_start_(nullptr), next_(0),
      id_(id), owner_identifier_(owner_identfier) {}

FixedPool::~FixedPool() { DestroyPool(); }

void FixedPool::CreatePool(size_t size_of_each_block, uint32_t num_of_blocks) {
  num_of_blocks_ = num_of_blocks;

  size_t size_of_each_block_with_header = sizeof(Header) + size_of_each_block;
  size_t alignedSizeOfEachBlock =
      Align(size_of_each_block_with_header, kMinAlignment);
  pad_before_header_ = alignedSizeOfEachBlock - size_of_each_block_with_header;
  size_of_each_block_ = alignedSizeOfEachBlock;

  mem_start_ = new uchar[size_of_each_block_ * num_of_blocks_];
  used_blocks_ = new bool[num_of_blocks_];
  ReclaimAll();
}

void FixedPool::DestroyPool() {
  delete[] mem_start_;
  delete[] used_blocks_;
  mem_start_ = nullptr;
  used_blocks_ = nullptr;
}

void *FixedPool::ForcedAllocate() {
  if (num_initialized_ < num_of_blocks_) {
    Header *h = (Header *)AddrFromIndex(num_initialized_);
    h->next_block_idx = num_initialized_ + 1;
    h->pool_identifier = (uintptr_t)this;
    num_initialized_++;
  }
  void *ret = ToData(next_);
  used_blocks_[IndexFromAddr(next_)] = true;

  --num_free_blocks_;
  if (num_free_blocks_ != 0) {
    next_ = AddrFromIndex(((Header *)next_)->next_block_idx);
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
  used_blocks_[IndexFromAddr(next_)] = false;
  ++num_free_blocks_;
}

void FixedPool::DeAllocate(void *p) {
  Header *h = ReadHeader(p);
  if (h->pool_identifier != (uintptr_t)this)
    return;

  ForcedDeAllocate(p);
}

inline void FixedPool::ReclaimAll() {
  num_initialized_ = 0;
  num_free_blocks_ = num_of_blocks_;
  std::memset(mem_start_, 0xf,
              size_of_each_block_ * num_free_blocks_ * sizeof(uchar));
  std::memset(used_blocks_, 0x0, num_free_blocks_ * sizeof(bool));

  next_ = (uchar *)AddrFromIndex(0);
}

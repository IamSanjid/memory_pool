#include <cstddef>
#include <cstdint>

#ifndef FIXED_POOL_BLOCK_COUNT
#define FIXED_POOL_BLOCK_COUNT 64
#endif

constexpr size_t kMinAlignment = 4;
constexpr size_t kDefaultBlockCount = FIXED_POOL_BLOCK_COUNT;
constexpr size_t kStackConsumeItems = 16;

class FixedPool {
  template <typename T> friend class Pool;
  template <typename T> friend class PoolManager;

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

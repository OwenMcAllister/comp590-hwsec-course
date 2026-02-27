#include "util.h"
#include <sys/mman.h>
#include <unistd.h>

#define LINE_SIZE 64
#define L2_SETS 1024
#define L2_WAYS 16
#define NUM_BITS 8
#define SET_SPAN (L2_SETS * LINE_SIZE)
#define BUFF_SIZE (SET_SPAN * L2_WAYS)
#define DATA_SET_BASE 256

// One window in cycles. At ~3GHz, 1ms = ~3,000,000 cycles.
#define WINDOW_CYCLES 5000000ULL  // How long the receiver waits for the sender to prime
#define ALIGN_CYCLES 500000000ULL // Align both processes to the same cycle (give enough time for both to start up)

static inline uint64_t now_cycles()
{
    uint32_t lo, hi;
    asm volatile("rdtscp" : "=a"(lo), "=d"(hi) :: "rcx");
    return ((uint64_t)hi << 32) | lo;
}

static inline void evict_set(void *buf, int set)
{ // Access all lines in the given set to evict it
  volatile char tmp;
  for (int way = 0; way < L2_WAYS; way++)
  {
    size_t offset = (size_t)way * SET_SPAN + (size_t)set * LINE_SIZE;
    tmp = *((volatile char *)buf + offset);
  }
  // asm volatile("" ::: "memory"); // Ensure all memory operations have completed before moving on
}

int main(int argc, char **argv)
{
  // Allocate a large buffer that maps to all cache sets. We use huge pages to ensure it is contiguous in physical memory, which helps with eviction.
  void *buf = mmap(NULL, BUFF_SIZE, PROT_READ | PROT_WRITE,
                   MAP_POPULATE | MAP_ANONYMOUS | MAP_PRIVATE | MAP_HUGETLB,
                   -1, 0);
  if (buf == (void *)-1)
  {
    perror("mmap() error\n");
    exit(EXIT_FAILURE);
  }

  for (int bit = 0; bit < NUM_BITS; bit++)
  {
      uintptr_t addr = (uintptr_t)buf + (size_t)(DATA_SET_BASE + bit) * LINE_SIZE;
      int set_index = (addr >> 6) & (L2_SETS - 1); // bits [15:6]
      printf("bit %d -> virtual addr 0x%lx -> set index %d\n", bit, addr, set_index);
  }

  // Warm all pages to ensure they are mapped in and to avoid page faults during the timing loop
  for (size_t i = 0; i < BUFF_SIZE; i += LINE_SIZE)
  {
    *((volatile char *)buf + i) = 1;
  }
  // asm volatile("" ::: "memory"); // Ensure all memory operations have completed before moving on

  // Align both processes to the same cycle boundary
  uint64_t now = now_cycles();
  uint64_t T0 = (now / ALIGN_CYCLES + 2ULL) * ALIGN_CYCLES; // (now / ALIGN_CYCLES + 2) is the next alignment point, giving enough time for both processes to start up and get ready
  while (now_cycles() < T0)
  {
    asm volatile("lfence" ::: "memory"); // lfence() to prevent out-of-order execution from affecting our timing
  }

  printf("Please type a message.\n");
  char text_buf[128];
  fgets(text_buf, sizeof(text_buf), stdin);
  unsigned char ch = (unsigned char)atoi(text_buf);

  while (1)
  {
    uint64_t window_start = now_cycles();
    uint64_t midpoint = window_start + WINDOW_CYCLES / 2;
    uint64_t window_end = window_start + WINDOW_CYCLES;

    // First half: idle — let the receiver prime all sets
    while (now_cycles() < midpoint)
    {
      asm volatile("lfence" ::: "memory");
    }

    // Second half: evict set i iff bit i is 1, repeat until window ends
    while (now_cycles() < window_end)
    {
      for (int bit = 0; bit < NUM_BITS; bit++)
      {
        if (ch & (1 << bit))
        {
          printf("  evicting set %d (bit %d = 1)\n", DATA_SET_BASE + bit, bit);
          evict_set(buf, DATA_SET_BASE + bit);
        }
      }
      asm volatile("lfence" ::: "memory");
    }
  }

  return 0;
}
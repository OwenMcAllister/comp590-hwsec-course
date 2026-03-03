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

// Slot timing in cycles. Sender and receiver do not need strict global alignment.
#define SLOT_CYCLES 5000000ULL
#define SLOT_GUARD_CYCLES 150000ULL

static const int bit_order[NUM_BITS] = {0, 5, 2, 7, 1, 6, 3, 4};

static inline uint64_t now_cycles()
{
    uint32_t lo, hi;
    asm volatile("rdtscp" : "=a"(lo), "=d"(hi) :: "rcx");
    return ((uint64_t)hi << 32) | lo;
}

static inline void evict_set(void *buf, int set)
{ // Access all lines in the given set to evict it
  volatile char tmp;
  for (int pass = 0; pass < 3; pass++)
  {
    for (int k = 0; k < L2_WAYS; k++)
    {
      int way = (pass * 5 + k * 7) & (L2_WAYS - 1);
      size_t offset = (size_t)way * SET_SPAN + (size_t)set * LINE_SIZE;
      tmp = *((volatile char *)buf + offset);
    }
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

  printf("Please type a message.\n");
  char text_buf[128];
  fgets(text_buf, sizeof(text_buf), stdin);
  unsigned char ch = (unsigned char)atoi(text_buf);

  printf("Sender transmitting value %u (0x%02x) in repeated slots.\n", ch, ch);

  while (1)
  {
    uint64_t slot_start = now_cycles();
    uint64_t active_end = slot_start + SLOT_CYCLES - SLOT_GUARD_CYCLES;
    uint64_t slot_end = slot_start + SLOT_CYCLES;

    // Active part of slot: repeatedly evict sets for bits that are 1.
    while (now_cycles() < active_end)
    {
      for (int i = 0; i < NUM_BITS; i++)
      {
        int bit = bit_order[i];
        if (ch & (1 << bit))
        {
          evict_set(buf, DATA_SET_BASE + bit);
        }
      }
      asm volatile("lfence" ::: "memory");
    }

    // Quiet tail to reduce boundary effects between adjacent slots.
    while (now_cycles() < slot_end)
    {
      asm volatile("lfence" ::: "memory");
    }
  }

  return 0;
}
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
{ // Returns the current time in CPU cycles
  unsigned int aux;
  asm volatile("rdtscp" : "=a"(aux) :: "rcx", "rdx");
  return aux;
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
  
  while (1)
  {
    char text_buf[128];
    fgets(text_buf, sizeof(text_buf), stdin);

    int num = string_to_int(text_buf);

    char ch = (char)num;
    char input_str[2] = {ch, '\0'};
    char *binary = string_to_binary(input_str);
    int binary_len = strlen(binary);

    for (int bi = 0; bi < binary_len; bi++)
    {
      printf("Sending bit %d/%d: %c\n", bi + 1, binary_len, binary[bi]);
      char bit_char = binary[bi];
      int bit = (bit_char == '1') ? 1 : 0;

      uint64_t window_start = now_cycles();
      uint64_t midpoint = window_start + WINDOW_CYCLES / 2;
      uint64_t window_end = window_start + WINDOW_CYCLES;

      // First half: idle, let receiver prime
      while (now_cycles() < midpoint)
      {
        asm volatile("lfence" ::: "memory");
      }

      // Second half: repeatedly evict if bit is 1
      while (now_cycles() < window_end)
      {
        if (bit)
        {
          evict_set(buf, DATA_SET_BASE);
        }
        asm volatile("lfence" ::: "memory");
      }
    }

    free(binary);
  }

  return 0;
}
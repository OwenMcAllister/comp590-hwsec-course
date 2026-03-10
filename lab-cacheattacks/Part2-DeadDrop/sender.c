#include"util.h"
// mman library to be used for hugepage allocations (e.g. mmap or posix_memalign only)
#include <sys/mman.h>
#include <unistd.h>

#define BUFF_SIZE (2 * 1024 * 1024)
#define L2_STEP_SIZE 65536
#define BASE_SET 200
#define BIT_STRIDE 64
#define SET_REPLICAS 3
#define SYMBOL_REPEAT 220
#define IDLE_GAP 40
#define MARKER_SET 64

// Get the address of the first cache line in the target set
uint8_t* get_set_addr(uint8_t *buffer, int target_set) {
        // Shift the set index to te left by 6 bits, to align int with the index bits
        int set_offset = target_set << 6;
        return buffer + set_offset;
}

// Evict each cacheline from a designated cache set
void evict_l2_set(uint8_t *buffer, int target_set) {
        uint8_t *set_addr = get_set_addr(buffer, target_set);
        volatile uint8_t tmp;

        // Access 16 different cache lines that map to the same L2 set
        for (int i=0; i < 16; i++) {
                // Jump between lines in the cache set by modifying the physical tag bits
                uint8_t *eviction_addr = set_addr + (i * L2_STEP_SIZE);
                tmp = *eviction_addr;

		asm volatile("mfence" ::: "memory");
        }
}

int main(int argc, char **argv)
{
  // Allocate a buffer using huge page
  // See the handout for details about hugepage management
  void *buf= mmap(NULL, BUFF_SIZE, PROT_READ | PROT_WRITE, MAP_POPULATE | MAP_ANONYMOUS | MAP_PRIVATE | MAP_HUGETLB, -1, 0);
  
  // Check if the allocation succeeded
  if (buf == (void*) - 1) {
     perror("mmap() error\n");
     exit(EXIT_FAILURE);
  }
  // The first access to a page triggers overhead associated with
  // page allocation, TLB insertion, etc.
  // Thus, we use a dummy write here to trigger page allocation
  // so later access will not suffer from such overhead.
  *((char *)buf) = 1; // dummy write to trigger page allocation

  // Read user input and transmit it to the receiver by evicting specific cache sets.
  while (1) {
    printf("Please type a message.\n");
    char text_buf[128];
    if (fgets(text_buf, sizeof(text_buf), stdin) == NULL) {
      break;
    }

    // Remove newline character from the input string
    int value = string_to_int(text_buf);
    if (value < 0 || value > 255) {
      printf("Input must be an integer in [0, 255].\n");
      continue;
    }

    // Transmit the value by evicting specific cache sets.
    for (int r = 0; r < SYMBOL_REPEAT; r++) {
      // Transmit activity marker so receiver can distinguish data from idle.
      for (int rep = 0; rep < SET_REPLICAS; rep++) {
        int marker_target_set = MARKER_SET + (rep * 4);
        evict_l2_set(buf, marker_target_set);
        asm volatile("lfence" ::: "memory");
      }

      // Evict cache sets corresponding to bits with value 1.
      for (int bit = 0; bit < 8; bit++) {
        int bit_val = (value >> (7 - bit)) & 1;
        if (bit_val == 1) {
          for (int rep = 0; rep < SET_REPLICAS; rep++) {
            int target_set = BASE_SET + (bit * BIT_STRIDE) + (rep * 4);
            evict_l2_set(buf, target_set);
            asm volatile("lfence" ::: "memory");
          }
        }
      }
    }

    // Idle gap to separate consecutive messages.
    for (int q = 0; q < IDLE_GAP; q++) {
      asm volatile("lfence" ::: "memory");
    }
  }

  printf("Sender finished.\n");
  return 0;
}

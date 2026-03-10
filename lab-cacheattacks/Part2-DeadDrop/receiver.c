#include"util.h"
// mman library to be used for hugepage allocations (e.g. mmap or posix_memalign only)
#include <sys/mman.h>
#include <unistd.h>

#define HUGE_PAGE_SIZE (2 * 1024 * 1024)
#define L2_STEP_SIZE 65536 // 2^16 allows us to modify the tag, while keeping the set index the same
#define MISS_THRESHOLD 130 // based on timing data from P1
#define BASE_SET 200 // agreed upon set index for the channel
#define BIT_STRIDE 64 // distance in the set index between bits in the channel, to avoid self-thrashing
#define SET_REPLICAS 3 // number of sets to use for each bit, to increase signal-to-noise ratio
#define PRIME_PROBE_LINES 8 // number of lines to prime/probe in each set, to balance signal strength with self-thrashing
#define BIT_MISS_THRESHOLD 5 // number of misses to classify a bit as 1, based on timing data from P1
#define REARM_SAMPLES 8 // number of consecutive matching samples to confirm a value
#define MARKER_SET 64 
#define MARKER_MISS_THRESHOLD 10 // number of misses in the marker set to classify the channel as active, based on timing data from P1

// Allocate a huge page, returning a pointer to the buffer. Exits on failure.
void* allocate_huge_page() {
	void *buffer = mmap(NULL, HUGE_PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_POPULATE | MAP_ANONYMOUS | MAP_PRIVATE | MAP_HUGETLB, -1, 0);

	if (buffer == (void*) - 1) {
		perror("mmap() error\n");
		exit(EXIT_FAILURE);
	}

	return buffer;
}

// Given a buffer and a target cache set index, return a pointer to the start of that cache set in the buffer
uint8_t* get_set_addr(uint8_t *buffer, int target_set) {
	// Shift the set index to te left by 6 bits, to align int with the index bits
	int set_offset = target_set << 6;
	return buffer + set_offset;
}

// Evict each cacheline from a designated cache set
void evict_l2_set(uint8_t *buffer, int target_set) {
	uint8_t *set_addr = get_set_addr(buffer, target_set);
	volatile uint8_t tmp;

	// Prime with fewer lines to avoid self-thrashing the set.
	for (int i=0; i < PRIME_PROBE_LINES; i++) {
		// Jump between lines in the cache set by modifying the physical tag bits
		uint8_t *eviction_addr = set_addr + (i * L2_STEP_SIZE);
		tmp = *eviction_addr;

		asm volatile("mfence" ::: "memory");
	}
}

// Probe a cache set by measuring access times to the lines in the set, returning the number of lines that miss in the cache
int probe_cache_set(uint8_t *buffer, int target_set) {
	uint8_t *set_addr = get_set_addr(buffer, target_set);
	int misses = 0;

	for (int i=0; i < PRIME_PROBE_LINES; i++) {
		// Need some binary operation to scramble access pattern
		int mixed_i = (i * 5) & (PRIME_PROBE_LINES - 1);

		uint8_t *probe_addr = set_addr + (mixed_i * L2_STEP_SIZE);
		
		asm volatile("lfence" ::: "memory"); // Start Fence
		uint32_t probe_time = measure_one_block_access_time((ADDR_PTR)probe_addr);
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

	int last_char = -1;
	int streak = 0;
	int CONFIDENCE_THRESH = 4;

	bool listening = true;
	while (listening) {
		// Put your covert channel code here
		// Probe evicted cache sets
		
		// Prime the exact sets used for the 8-bit channel.
	        for (int i = 0; i < 8; i++) {
			for (int j =0; j < SET_REPLICAS; j++){
				int target_set = BASE_SET + (i * BIT_STRIDE) + (j * 4);
              			evict_l2_set(buffer, target_set);
			}
        	}
		
		// Wait without usleep, so we don't give up the core
		for (volatile int w = 0; w < 50000; w++) {}
		
		// Probe the marker set to see if the sender is active, before probing the channel sets.
		int marker_misses = 0;
		for (int j = 0; j < SET_REPLICAS; j++) {
			int marker_target_set = MARKER_SET + (j * 4);
			marker_misses += probe_cache_set(buffer, marker_target_set);
		}

		// condition to determine if sender is active, based on timing data from P1
		if (marker_misses <= MARKER_MISS_THRESHOLD) {
			last_char = -1;
			streak = 0;
			continue;
		}

		int recv_value = 0;

		for (int i = 0; i < 8; i++) {
			int num_misses = 0;

			// Probe SET_REPLICAS sets for each bit index.
			for (int j = 0; j < SET_REPLICAS; j++) {
				int target_set = BASE_SET + (i * BIT_STRIDE) + (j * 4);
				num_misses += probe_cache_set(buffer, target_set);
			}

				if (num_misses > BIT_MISS_THRESHOLD) {
					recv_value |= (1 << (7 - i));
				}
		}
		if (streak < 0) {
			continue;
		}

		// confidence checking to avoid false positives
		if (recv_value == last_char) {
			if (streak < CONFIDENCE_THRESH) {
				streak++;
			}
			if (streak == CONFIDENCE_THRESH) {
				printf("%d\n", recv_value);
				fflush(stdout);
				streak = -REARM_SAMPLES;
			}
		} else {
			last_char = recv_value;
			streak = 1;
		}
		}

	printf("Receiver finished.\n");

	return 0;
}

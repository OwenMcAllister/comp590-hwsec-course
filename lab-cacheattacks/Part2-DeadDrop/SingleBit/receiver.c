
#include"util.h"
// mman library to be used for hugepage allocations (e.g. mmap or posix_memalign only)
#include <sys/mman.h>
#include <stdint.h>
#include <unistd.h>

#define HUGE_PAGE_SIZE (2 * 1024 * 1024)
#define L2_STEP_SIZE 65536 // 2^16 allows us to modify the tag, while keeping the set index the same
#define MISS_THRESHOLD 150 // value determined from the part 1 experiments

static int probe_set_misses(uint8_t *buf, int target_set)
{
	int misses = 0;
	uint8_t *set_addr = buf + (target_set << 6);

	for (int i = 0; i < 16; i++) {
		int mixed_i = (i * 5) & 0xF;
		uint8_t *probe_addr = set_addr + (mixed_i * L2_STEP_SIZE);
		uint32_t t = measure_one_block_access_time((ADDR_PTR)probe_addr);
		if (t > MISS_THRESHOLD) misses++;
	}
	return misses;
}

static int sample_bit_window(uint8_t *buf)
{
	const int groups = 3;
	const int group_stride = 4;
	const int base_set = 200;
	const int bit_samples = 120;
	const int bit_miss_threshold = 18;
	int ones_votes = 0;

	for (int s = 0; s < bit_samples; s++) {
		int total_misses = 0;
		for (int g = 0; g < groups; g++) {
			int set_id = base_set + (g * group_stride);
			total_misses += probe_set_misses(buf, set_id);
		}

		if (total_misses > bit_miss_threshold) {
			ones_votes++;
		}
		usleep(120);
	}

	return (ones_votes > (bit_samples / 2)) ? 1 : 0;
}

int main(int argc, char **argv)
{
	// Put your covert channel setup code here
	void *buf= mmap(NULL, HUGE_PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_POPULATE | MAP_ANONYMOUS | MAP_PRIVATE | MAP_HUGETLB, -1, 0);
	if (buf == (void*) - 1) {
		perror("mmap() error\n");
		exit(EXIT_FAILURE);
	}
	for (int i = 0; i < HUGE_PAGE_SIZE; i += 4096) {
		((uint8_t *)buf)[i] = 1;
	}

	printf("Please press enter.\n");

	char text_buf[2];
	fgets(text_buf, sizeof(text_buf), stdin);

	printf("Receiver now listening.\n");

	int last_bit = -1;
	int streak = 0;
	const int confidence_thresh = 3;
	bool listening = true;
	while (listening) {

		// Put your covert channel code here
		int bit = sample_bit_window((uint8_t *)buf);
		if (bit == last_bit) {
			streak++;
		} else {
			last_bit = bit;
			streak = 1;
		}
		if (streak == confidence_thresh) {
			printf("%d\n", bit);
			fflush(stdout);
		}

	}

	printf("Receiver finished.\n");

	return 0;
}

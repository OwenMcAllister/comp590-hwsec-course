#include "utility.h"

// sizes according to the machine
#define L1_SIZE (32 * 1024)
#define L2_SIZE (1 * 1024 * 1024)
#define L3_SIZE (11 * 1024 * 1024)

#define CACHE_LINE_SIZE 64 // 64 Bytes per cache line
#define EVICT_NUM 3 // accesing 3x the cache size should be enough to evict the target line from that cache level
#define EVICT_DEN 2 // dividing by 2 to reduce the eviction buffer size, to avoid evicting from the next level as well
#define EVICT_PASSES 3 // number of times we sweep through the eviction buffer to increase the chances of eviction
 
int main (int ac, char **av) {

    // create 4 arrays to store the latency numbers
    // the arrays are initialized to 0
    uint64_t dram_latency[SAMPLES] = {0};
    uint64_t l1_latency[SAMPLES] = {0};
    uint64_t l2_latency[SAMPLES] = {0};
    uint64_t l3_latency[SAMPLES] = {0};

    // A temporary variable we can use to load addresses
    // The volatile keyword tells the compiler to not put this variable into a
    // register- it should always try to load from memory/ cache.
    volatile char tmp;

    // Allocate a buffer of 64 Bytes
    // the size of an unsigned integer (uint64_t) is 8 Bytes
    // Therefore, we request 8 * 8 Bytes
    uint64_t *target_buffer = (uint64_t *)malloc(8*sizeof(uint64_t));

    if (NULL == target_buffer) {
        perror("Unable to malloc");
        return EXIT_FAILURE;
    }

    // [1.2] TODO: Uncomment the following line to allocate a buffer of a size
    // of your chosing. This will help you measure the latencies at L2 and L3.
    const size_t eviction_bytes = (L3_SIZE * EVICT_NUM) / EVICT_DEN; // size > L3 to ensure eviction, divided by EVICT_DEN to reduce the eviction buffer size
    uint8_t *eviction_buffer = (uint8_t *)malloc(eviction_bytes);

    if (NULL == eviction_buffer) {
        perror("Unable to malloc eviction_buffer");
        free(target_buffer);
        return EXIT_FAILURE;
    }

    const size_t eviction_lines = eviction_bytes / CACHE_LINE_SIZE; // number of cache lines in the eviction buffer
    size_t l1_evict_lines = ((L1_SIZE * EVICT_NUM) / EVICT_DEN) / CACHE_LINE_SIZE; // eviction lines needed to evict from L1
    size_t l2_evict_lines = ((L2_SIZE * EVICT_NUM) / EVICT_DEN) / CACHE_LINE_SIZE; // eviction lines needed to evict from L2
    // capping eviction lines to the size of the eviction buffer
    if (l1_evict_lines > eviction_lines) l1_evict_lines = eviction_lines; 
    if (l2_evict_lines > eviction_lines) l2_evict_lines = eviction_lines; 

    // touch pages once so page faults don't inject extra variance.
    for (size_t line = 0; line < eviction_lines; line++) {
        eviction_buffer[line * CACHE_LINE_SIZE] = (uint8_t)line;
    }

    // Example: Measure L1 access latency, store results in l1_latency array
    for (int i=0; i<SAMPLES; i++){
        // Step 1: bring the target cache line into L1 by simply accessing the line
        tmp = target_buffer[0];

        // Step 2: measure the access latency
        l1_latency[i] = measure_one_block_access_time((uint64_t)target_buffer);
    }

    // ======
    // [1.2] TODO: Measure DRAM Latency, store results in dram_latency array
    // ======
    //
    for (int i=0; i<SAMPLES; i++) {
        // Evict the target line from all cache levels so the next access hits DRAM.
        clflush(target_buffer);
        dram_latency[i] = measure_one_block_access_time((uint64_t)target_buffer);
    }

    // ======
    // [1.2] TODO: Measure L2 Latency, store results in l2_latency array
    // ======
    //
    for (int i=0; i<SAMPLES; i++) {
        tmp = target_buffer[0];

        // Sweep >L1 worth of lines to push target out of L1, while staying cached below.
        for (int pass = 0; pass < EVICT_PASSES; pass++) {
            for (size_t line = 0; line < l1_evict_lines; line++) {
                tmp ^= eviction_buffer[line * CACHE_LINE_SIZE]; // XOR to prevent compiler optimizing away the loop
            }
        }

        l2_latency[i] = measure_one_block_access_time((uint64_t)target_buffer);
    }

    // ======
    // [1.2] TODO: Measure L3 Latency, store results in l3_latency array
    // ======
    //
    for (int i=0; i<SAMPLES; i++) {
        tmp = target_buffer[0];

        // Sweep >L2 worth of lines to push target out of L1+L2, leaving L3/DRAM.
        for (int pass = 0; pass < EVICT_PASSES; pass++) {
            for (size_t line = 0; line < l2_evict_lines; line++) {
                tmp ^= eviction_buffer[line * CACHE_LINE_SIZE]; // XOR to prevent compiler optimizing away the loop
            }
        }

        l3_latency[i] = measure_one_block_access_time((uint64_t)target_buffer);
    }


    // Print the results to the screen
    // [1.5] Change print_results to print_results_for_python so that your code will work
    // with the python plotter software
    print_results_for_python(dram_latency, l1_latency, l2_latency, l3_latency);

    free(target_buffer);

    // [1.2] TODO: Uncomment this line once you uncomment the eviction_buffer creation line
    free(eviction_buffer);
    return 0;
}

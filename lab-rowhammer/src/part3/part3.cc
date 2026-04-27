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
#define THRESHOLD 400
#define POOL_SIZE 1000 // Number of addresses to sample for binning
#define ROUNDS  100




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
    // TODO - Exercise 3-1
    std::array<std::vector<uint64_t>, BANKS> bins;

    size_t num_bins = 0;
    size_t num_samples = 0;

    // Walk by one DRAM row size. This keeps the sampled addresses spread out
    // enough to cover different rows and banks instead of only nearby columns.
    for (uint64_t virt_addr = starting_addr;
         virt_addr < final_addr && num_samples < POOL_SIZE;
         virt_addr += ROW_SIZE) {
        uint64_t phys_addr = virt_to_phys(virt_addr);

        // Skip addresses that were not translated successfully.
        if (phys_addr == 0) {
            continue;
        }

        bool placed = false;

        // Compare this address with one representative from each existing bin.
        // If the median latency is above THRESHOLD, the two addresses likely
        // conflict in the same DRAM bank, so they belong in the same bin.
        for (size_t i = 0; i < num_bins; i++) {
            uint64_t rep_phys_addr = bins[i][0];
            uint64_t rep_virt_addr = phys_to_virt(rep_phys_addr);

            // A representative should be mapped, but skip it if lookup fails.
            if (rep_virt_addr == 0) {
                continue;
            }

            uint64_t time_vals[ROUNDS] = {0};
            for (size_t j = 0; j < ROUNDS; j++) {
                time_vals[j] = measure_bank_latency(
                    (volatile char *)virt_addr,
                    (volatile char *)rep_virt_addr);
            }

            uint64_t latency = median(time_vals, ROUNDS);
            if (latency > THRESHOLD) {
                bins[i].push_back(phys_addr);
                placed = true;
                break;
            }
        }

        // If no existing bin conflicts with this address, start a new bank bin.
        if (!placed && num_bins < BANKS) {
            bins[num_bins].push_back(phys_addr);
            num_bins++;
            placed = true;
        }

        if (placed) {
            num_samples++;
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

#include <emmintrin.h>

#include "shared.hh"
#include "params.hh"
#include "util.hh"

// Physical Page Number to Virtual Page Number Map
std::map<uint64_t, uint64_t> PPN_VPN_map;

// Base pointer to a large memory pool
void * allocated_mem;

/*
 * setup_PPN_VPN_map
 *
 * Populates the Physical Page Number -> Virtual Page Number mapping table
 *
 * Inputs: mem_map - Base pointer to the large allocated pool
 *         PPN_VPN_map - Reference to a PPN->VPN map 
 *
 * Side-Effects: For *each page* in the allocated pool, the virtual page 
 *               number is into the map with a key corresponding to the 
 *               page's physical page number.
 *
 */
void setup_PPN_VPN_map(void * mem_map,
                       std::map<uint64_t, uint64_t> &PPN_VPN_map) {
    uint64_t phys_addr;
    // You can assume the page size is 2MB as opposed to 4KB.
    // The populated reverse page table should cover a 2GB region following the pointer mem map
    for (uint64_t offset = 0; offset < (1UL << 31); offset += HUGE_PAGE_SIZE) {
        uint64_t virt_addr = (uint64_t)mem_map + offset;
        phys_addr = virt_to_phys(virt_addr);
        if (phys_addr != 0) {
            uint64_t vpn = virt_addr / HUGE_PAGE_SIZE;
            uint64_t ppn = phys_addr / HUGE_PAGE_SIZE;
            PPN_VPN_map[ppn] = vpn;
        }
    }
}

/*
 * allocate_pages
 *
 * Allocates a memory block of size BUFFER_SIZE_MB
 *
 * Make sure to write something to each page in the block to ensure
 * that the memory has actually been allocated!
 *
 * Inputs: none
 * Outputs: A pointer to the beginning of the allocated memory block
 */
void * allocate_pages(uint64_t memory_size) {
    void * memory_block = mmap(NULL, memory_size, PROT_READ | PROT_WRITE,
            MAP_POPULATE | MAP_ANONYMOUS | MAP_PRIVATE | MAP_HUGETLB, -1, 0);
    assert(memory_block != (void*)-1);

    for (uint64_t i = 0; i < memory_size; i += HUGE_PAGE_SIZE) {
        uint64_t * addr = (uint64_t *) ((uint8_t *) (memory_block) + i);
        *addr = i;
    } 

    return memory_block;
}

/* 
 * virt_to_phys
 *
 * Determines the physical address mapped to by a given virtual address
 *
 * Inputs: virt_addr - A virtual pointer/address
 * Output: phys_ptr - The physical address corresponding to the virtual pointer
 *                    IMPORTANT: If the virtual pointer is not currently
 *                               present, return 0
 *
 */


uint64_t virt_to_phys(uint64_t virt_addr) {
    uint64_t phys_addr = 0;

    FILE * pagemap;
    uint64_t entry;

    // TODO: Exercise 1-1
    // Compute the virtual page number from the virtual address
    uint64_t virt_page_number = virt_addr / 0x1000;
    uint64_t file_offset = virt_page_number * sizeof(uint64_t);

    if ((pagemap = fopen("/proc/self/pagemap", "r"))) {
        if (lseek(fileno(pagemap), file_offset, SEEK_SET) == file_offset) {
            if (fread(&entry, sizeof(uint64_t), 1, pagemap)) {
                if (entry & (1ULL << 63)) {
                    uint64_t phys_page_number = entry & ((1ULL << 54) - 1); // The physical page number is stored in the lower 54 bits of the entry
                    // Virtual page number is mapped to the physical page number
                    // The offset within the page is the same as the offset within the virtual page
                    // 64 bit physical address means 4 KB page size, so we multiply the physical page number by 0x1000 and add the offset
                    phys_addr = (phys_page_number * 0x1000) + (virt_addr % 0x1000);
                } 
            }
        }
        fclose(pagemap);
    }
    return phys_addr;
}

/*
 * phys_to_virt
 *
 * Determines the virtual address mapping to a given physical address
 *
 * HINT: This should use your PPN_VPN_map!
 *
 * Inputs: phys_addr - A physical pointer/address
 * Output: virt_addr - The virtual address corresponding to the physical pointer
 *                     If the physical pointer is not mapped, return 0
 *
 */

/*
As we know that for address translation, we take the VPN and translate it to PPN, but keep
the page offset unchanged. So a virtual address and its corresponding physical address always
have the same page offset. When considering 2MB and 4KB pages, we can consider a 2MB
page to consist of 512 4KB pages. For these 4KB pages, the lower 9 bits of their VPNs are
counted as page offset for a 2MB page. Therefore, when being translated between virtual and
physical address, these 9 bits do not need to be changed.
*/

// 4KB for probing pagemap interface, but 2MB for reverse page table
uint64_t phys_to_virt(uint64_t phys_addr) {
    uint64_t phys_page_number = phys_addr / HUGE_PAGE_SIZE;
    auto it = PPN_VPN_map.find(phys_page_number);
    if (it != PPN_VPN_map.end()) {
        uint64_t virt_page_number = it->second;
        // The offset within the page is the same as the offset within the physical page
        return (virt_page_number * HUGE_PAGE_SIZE) + (phys_addr % HUGE_PAGE_SIZE);
    } else {
        return 0;
    }
}


/*
 * get_rand_addr
 *
 * Gets a random virtual address (aligned to cacheline size)
 *
 *
 * Inputs: allocate buffer size 
 * Output: virt_addr - A random virtual address within the allocated memory
 *
 */

char* get_rand_addr(size_t buf_size)
{
    size_t num_cls = buf_size / CACHELINE_SIZE;
    size_t idx = rand64() % num_cls;
    return (char*)allocated_mem + idx * CACHELINE_SIZE;
}

/*
 * measure_bank_latency
 *
 * Measures a (potential) bank collision between two addresses,
 * and returns its timing characteristics.
 *
 * Inputs: addr_A/addr_B - Two (virtual) addresses used to observe
 *                         potential contention
 * Output: Timing difference (derived by a scheme of your choice)
 *
 */
uint64_t measure_bank_latency(volatile char *addr_A, volatile char *addr_B) {
    /*
    Given two physical addresses x and y, again make
    sure both of the addresses are not cached. Access the two addresses back-to-back without memory
    fences in between and measure their collective access latency. If the two addresses are mapped to
    the same bank and different rows, they will cause memory bus contention in addition to row buffer
    conflicts, and thus will result in even longer latency.
    */

    clflush(addr_A); 
    clflush(addr_B);

    // 352

    uint64_t start = rdtscp();
    // Access the two addresses back-to-back without memory fences in between
    volatile char temp_A = *addr_A;
    volatile char temp_B = *addr_B;
    uint64_t end = rdtscp();
    return end - start;
}

/*
 * phys_to_bankid
 *
 * Computes the bank id of a physical address
 *
 * Inputs: phys_ptr: a physical address; candidate: the bank function derived from part3
 * Output: bank index
 *
 */

uint64_t phys_to_bankid(uint64_t phys_ptr, uint8_t candidate)
{
    static std::array<std::function<uint64_t(uint64_t)>, 3> functions = {

        // candidate 0
        [](uint64_t x) {
            return ((get_bit(x, 14) ^ get_bit(x, 17)) << 3) | 
                   ((get_bit(x, 15) ^ get_bit(x, 18)) << 2) | 
                   ((get_bit(x, 16) ^ get_bit(x, 19)) << 1) |
                   ((get_bit(x, 7) ^ get_bit(x, 8) ^ get_bit(x, 9) ^
                     get_bit(x, 12) ^ get_bit(x, 13) ^
                     get_bit(x, 15) ^ get_bit(x, 16)));
        },

        // candidate 1
        [](uint64_t x) {
            return ((get_bit(x, 15) ^ get_bit(x, 18)) << 3) | 
                   ((get_bit(x, 16) ^ get_bit(x, 19)) << 2) | 
                   ((get_bit(x, 17) ^ get_bit(x, 20)) << 1) |
                   ((get_bit(x, 7) ^ get_bit(x, 8) ^ get_bit(x, 9) ^
                     get_bit(x, 12) ^ get_bit(x, 13) ^
                     get_bit(x, 18) ^ get_bit(x, 19)));
        },

        // candidate 2
        [](uint64_t x) {
            return ((get_bit(x, 13) ^ get_bit(x, 17)) << 3) | 
                   ((get_bit(x, 14) ^ get_bit(x, 18)) << 2) | 
                   ((get_bit(x, 15) ^ get_bit(x, 19)) << 1) |
                   ((get_bit(x, 7) ^ get_bit(x, 8) ^ get_bit(x, 9) ^
                     get_bit(x, 12) ^ get_bit(x, 13) ^
                     get_bit(x, 20) ^ get_bit(x, 21)));
        },
    };

    return functions[candidate](phys_ptr) & 0xF;
}

/*
 * phys_to_rowid
 *
 * Computes the row id of a physical address
 *
 * Inputs: phys_ptr: a physical address;
 * Output: row index
 *
 */

uint64_t phys_to_rowid(uint64_t phys_ptr){
	return (phys_ptr & ROW_MASK) >> __builtin_ctzl(ROW_MASK);
}

/*
 * phys_to_colid
 *
 * Computes the column id of a physical address
 *
 * Inputs: phys_ptr: a physical address;
 * Output: column index
 *
 */

uint64_t phys_to_colid(uint64_t phys_ptr){
    return (phys_ptr & COL_MASK) >> __builtin_ctzl(COL_MASK);
}


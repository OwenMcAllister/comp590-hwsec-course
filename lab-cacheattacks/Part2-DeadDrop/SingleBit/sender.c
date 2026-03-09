
#include"util.h"
// mman library to be used for hugepage allocations (e.g. mmap or posix_memalign only)
#include <sys/mman.h>
#include <stdint.h>
#include <unistd.h>

// TODO: define your own buffer size
#define BUFF_SIZE (2 * 1024 * 1024)
#define L2_STEP_SIZE 65536
#define CACHE_LINE_SIZE 64

static volatile uint8_t sink = 0;

static uint64_t now_ns(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ((uint64_t)ts.tv_sec * 1000000000ULL) + (uint64_t)ts.tv_nsec;
}

// implementation of the sender
static void send_bit(uint8_t *buf, int bit)
{
  const int bit_window_us = 100000;
  const int groups = 3; // Number of sets to access for encoding a '1' bit
  const int group_stride = 4; // Stride to select sets that map to the same cache set index
  const int base_set = 200; // Base set index to start from (can be tuned based on the cache architecture)
  uint64_t end_ns = now_ns() + (uint64_t)bit_window_us * 1000ULL;

  if (bit == 1) {
    while (now_ns() < end_ns) {
      for (int g = 0; g < groups; g++) {
        int set_id = base_set + (g * group_stride);
        uint8_t *set_addr = buf + (set_id << 6);
        // Access lines with same set index but different tags.
        for (int i = 0; i < 16; i++) {
          sink ^= set_addr[i * L2_STEP_SIZE];
        }
      }
    }
  } else {
    while (now_ns() < end_ns) {
      usleep(500);
    }
  }
}

int main(int argc, char **argv)
{
  // Allocate a buffer using huge page
  // See the handout for details about hugepage management
  void *buf= mmap(NULL, BUFF_SIZE, PROT_READ | PROT_WRITE, MAP_POPULATE | MAP_ANONYMOUS | MAP_PRIVATE | MAP_HUGETLB, -1, 0);
  
  if (buf == (void*) - 1) {
     perror("mmap() error\n");
     exit(EXIT_FAILURE);
  }
  // The first access to a page triggers overhead associated with
  // page allocation, TLB insertion, etc.
  // Thus, we use a dummy write here to trigger page allocation
  // so later access will not suffer from such overhead.
  //*((char *)buf) = 1; // dummy write to trigger page allocation


  // TODO:
  // Put your covert channel setup code here
  *((char *)buf) = 1; // dummy write to trigger page allocation

  printf("Please type a message.\n");

  bool sending = true;
  while (sending) {
      char text_buf[128];
      if (fgets(text_buf, sizeof(text_buf), stdin) == NULL) {
        break;
      }

      // TODO:
      // Put your covert channel code here
      if (text_buf[0] == 'q') {
        sending = false;
        continue;
      }

      int bit = (text_buf[0] == '1') ? 1 : 0;
      send_bit((uint8_t *)buf, bit);
  }

  printf("Sender finished.\n");
  return 0;
}

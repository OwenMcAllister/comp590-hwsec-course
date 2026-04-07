## 1-1

**Given the attack plan above, how many addresses need to be flushed in the first step?**

256 addresses needs to be flushed at the first step. As the secret is one byte at a time, and one byte can have 256 possible values (`0x00` to `0xFF`). 

The victim pseudocode says: `load shared_mem[4096 * secret_data]`

The value of the secret_data chooses which page-sized offset is accessed. As, 4096 is the stride, each possible byte value maps to different page in shared_mem. So to make sure none of the possible secret-dependent pages are already cached, we need to flush all the possible candidate locations. 

## 2-1

**Copy your code in `run_attacker` from `attacker-part1.c` to `attacker-part2.c`. Does your Flush+Reload attack from Part 1 still work? Why or why not?**

Flush+Reload does not work directly now in the normal execution path because of the added bounds check. 

When `offset ≥ part2_limit`, the load is not executed architecturally, so no shared memory page is accessed and no cache signal is created. Therefore, we cannot observe anything using Flush+Reload in the normal case.

## 2-3

**In our example, the attacker tries to leak the values in the array `secret_part2`. In a real-world attack, attackers can use Spectre to leak data located in an arbitrary address in the victim's space. Explain how an attacker can achieve such leakage.**

In a real-world Spectre attack, the attacker is not limited to a fixed array like secret_part2, but instead controls the inputs to the victim so that any memory address can be accessed during speculative execution. The value read from that address is then encoded into a cache side effect, such as accessing a probe array. By measuring cache timings, the attacker can recover the data byte by byte, even though the access is not allowed in normal execution.

## 2-4

**Experiment with how often you train the branch predictor. What is the minimum number of times you need to train the branch (i.e. `if offset < part2_limit`) to make the attack work?**

Using our current attacker configuration (attack_attempts = 700), the attack worked with a minimum of 1 in-bounds branch-training iteration before the out-of-bounds access.


## 3-2

**Describe the strategy you employed to extend the speculation window of the target branch in the victim.**

We repeatedly trained the branch predictor with in-bounds offsets (`offset < 4`) so the bounds check was predicted as taken, then triggered the victim with an out-of-bounds offset. To widen the speculative window, we increased memory pressure before each attack attempt by flushing the probe array and thrashing the cache, which delayed branch resolution. We then ran this process many times per byte and selected the value with the strongest vote to reduce noise.


## 3-3

**Assume you are an attacker looking to exploit a new machine that has the same kernel module installed as the one we attacked in this part. What information would you need to know about this new machine to port your attack? Could it be possible to determine this infomration experimentally? Briefly describe in 5 sentences or less.**

To port the attack, we would need the machine’s cache timing profile (L1/L2/L3/DRAM latencies), branch predictor behavior, and how noisy scheduling/CPU frequency scaling are under load. 
We would also need to know whether Spectre mitigations are enabled in hardware, kernel, or compiler, since they can shrink or block the speculation window. 
The key attack parameters to retune are cache-hit threshold, number of branch-training iterations, and number of measurement attempts per leaked byte. 
Yes, most of this can be determined experimentally by running timing microbenchmarks (clflush + rdtsc) and sweeping attacker parameters while measuring leak accuracy. In practice, calibration is iterative: profile timings, run attacks, adjust parameters, and repeat until leakage is stable.

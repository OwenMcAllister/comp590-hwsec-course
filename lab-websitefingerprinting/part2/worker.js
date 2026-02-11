// Number of sweep counts
// TODO (Exercise 2-1): Choose an appropriate value!
let P = 10;

// Number of elements in your trace
let K = 5 * 1000 / P; 

// Array of length K with your trace's values
let T;

// Value of performance.now() when you started recording your trace
let start;

// LLC-sized buffer (8MB is typical for modern processors)
// Each element is 8 bytes, and cache line is 64 bytes (8 elements per line)
const LLC_SIZE = 8 * 1024 * 1024; // 8MB in bytes
const CACHE_LINE_SIZE = 64; // bytes
const ELEMENTS_PER_LINE = CACHE_LINE_SIZE / 8; // 8 elements (8 bytes each)
const NUM_CACHE_LINES = LLC_SIZE / CACHE_LINE_SIZE; // Number of cache lines

// Create LLC-sized buffer
let buffer = new Float64Array(NUM_CACHE_LINES * ELEMENTS_PER_LINE);

function record() {
  // Create empty array for saving trace values
  T = new Array(K);

  // Fill array with -1 so we can be sure memory is allocated
  T.fill(-1, 0, T.length);

  // Save start timestamp
  start = performance.now();

  // TODO (Exercise 2-1): Record data for 5 seconds and save values to T.
  let traceIndex = 0;
  
  // Record for 5 seconds
  while (traceIndex < K) {
    let windowStart = performance.now();
    let sweepCount = 0;
    
    // Count how many sweeps fit in P milliseconds
    while (performance.now() - windowStart < P) {
      // Perform one sweep over the LLC
      // Access each cache line sequentially
      for (let i = 0; i < buffer.length; i += ELEMENTS_PER_LINE) {
        buffer[i] = buffer[i] + 1; // Read and write to ensure cache access
      }
      sweepCount++;
    }
    
    // Save the sweep count for this time window
    T[traceIndex] = sweepCount;
    traceIndex++;
  }
  // Once done recording, send result to main thread
  postMessage(JSON.stringify(T));
}

// DO NOT MODIFY BELOW THIS LINE -- PROVIDED BY COURSE STAFF
self.onmessage = (e) => {
  if (e.data.type === "start") {
    setTimeout(record, 0);
  }
};

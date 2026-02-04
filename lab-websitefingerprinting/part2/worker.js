// Number of sweep counts
// TODO (Exercise 2-1): Choose an appropriate value!
let P = 5;

// Number of elements in your trace
let K = 5 * 1000 / P; 

// Array of length K with your trace's values
let T;

// Value of performance.now() when you started recording your trace
let start;

function record() {
  T = new Array(K); // preparing space for exactly K measurements

  // Fill array with -1 so we can be sure memory is allocated
  T.fill(-1, 0, T.length);

  // Save start timestamp
  start = performance.now();

  // TODO (Exercise 2-1): Record data for 5 seconds and save values to T.
  // Theres always a bigger fish pseudocode followed
  const endTime = start + 5000; // 5 seconds from start
  // Each JS number is 8 bytes (64 bits) (double precision)
  // 16 elements x 8 bytes = 128 bytes = 1 cache line (typical)
  // This spacing helps us jump accross cache lines, not stay in one
  const LINE_SIZE = 16; // 128/sizeof(double) Note that js treats all numbers as double
  const size = 10000; // Number of cache lines to sweep
  const buffer = new Float64Array(size * LINE_SIZE); // Buffer to access

  while (performance.now() < endTime) {
    let counter = 0;
    const tBegin = performance.now();

    do {
      counter++;

      // Access one element per cache line.
      for (let i = 0; i < size; i++) {
        let tmp = buffer[i * LINE_SIZE]; // Access first element of each cache line
      }
    } while (performance.now() - tBegin < P);

    // Record the counter value in the appropriate trace index
    const traceIndex = Math.floor((tBegin - start) / P); // Which index in T to record to
    if (traceIndex >= 0 && traceIndex < K) {
      T[traceIndex] = counter;
    }
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

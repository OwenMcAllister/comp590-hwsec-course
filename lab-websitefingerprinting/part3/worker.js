// Number of sweep counts
// TODO (Exercise 3-1): Choose an appropriate value!
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
  while (performance.now() < endTime) {
    let counter = 0;
    const tBegin = performance.now();

    do {
      counter++;
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

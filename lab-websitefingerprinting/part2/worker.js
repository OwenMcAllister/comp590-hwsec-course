// P is the amount of time we spend counting in each interval (in ms)
let P = 5; // The smallest time interval that we can measure with performance.now() in ms

// Number of elements in your trace
let K = 5 * 1000 / P; 

// Array of length K with your trace's values
let T;

// Value of performance.now() when you started recording your trace
let start;

function record() {
  // Create empty array for saving trace values
  T = new Array(K);

  // Fill array with -1 so we can be sure memory is allocated
  T.fill(-1, 0, T.length);

  // Counts form a trace with length of K
  let elapsed = 0;
  let index = 0;
  const LINE_SIZE = 16; // 128/sizeof(double)
  const NUM_LINES = 1000; // At least 1000 cache lines need to be accessed to see resolution
  const M = new Array(NUM_LINES * LINE_SIZE).fill(-1); // Fill with -1 to ensure allocation

  while (elapsed < 5000) { // Repeat sweep counting for 5 seconds
    let counter = 0;
    let beginTime = performance.now();

    while (performance.now() - beginTime < P) {
      counter++;

      // Access cache lines sequentially, a "sweep" over the LLC
      for (let j = 0; j < NUM_LINES; j++) {
        let val = M[j * LINE_SIZE]; // Access one element per cache line
      }
    }

    // After P milliseconds, record the counter value in T
    if (index < K) {
      T[index] = counter;
    }

    elapsed += P; // Increment elapsed time
    index++;
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

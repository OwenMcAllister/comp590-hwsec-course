const runs = 10;

// Different numbers of cache lines to test
const CACHE_LINE_COUNTS = [
  1, 10, 100, 1000,
  10000, 100000,
  1000000, 10000000
];

function getMedian(values) {
  const sorted = [...values].sort((a, b) => a - b);
  const mid = Math.floor(sorted.length / 2);
  if (sorted.length % 2 === 0) {
    return (sorted[mid - 1] + sorted[mid]) / 2;
  }
  return sorted[mid];
}

function measureOneLine() {
  // Each JS number is 8 bytes (64 bits) (double precision)
  // 16 elements x 8 bytes = 128 bytes = 1 cache line (typical)
  // This spacing helps us jump accross cache lines, not stay in one
  const LINE_SIZE = 16; // 128/sizeof(double) Note that js treats all numbers as double
  
  // for memorygram samples
  let result = [];

  // Fill with -1 to ensure allocation
  // filling with zeros may lead to optimizations, as JS engines may use sparse arrays, hence we use -1
  // Size = runs * LINE_SIZE, so we have enough space for all runs
  const M = new Array(runs * LINE_SIZE).fill(-1);

  // repeat the measurement 'runs' times
  for (let i = 0; i < runs; i++) {
    const start = performance.now();

    // Access the first element of the i-th cache line
    // this forces the CPU to load that cache line into cache
    let val = M[i * LINE_SIZE]; // checks 1 cache line
    const end = performance.now();

    // Store how long this memory access took
    // If cache is busy (victim using it) → this value is larger
    // If cache is free → this value is smaller
    result.push(end - start);
  }

  return result;
}

function measureNLines() {
  const LINE_SIZE = 16;
  const results = [];

  // For each N in CACHE_LINE_COUNTS, measure access times
  for (const N of CACHE_LINE_COUNTS) {
    // Single region; shuffle access order per run to reduce warm-up bias
    const M = new Float64Array(N * LINE_SIZE).fill(-1);
    const times = new Array(runs);
    const indices = new Array(N);
    for (let j = 0; j < N; j++) indices[j] = j;

    // repeat the measurement 'runs' times
    for (let i = 0; i < runs; i++) {
      // Fisher–Yates shuffle of cache-line indices
      // This ensures we access cache lines in a random order each run, preventing any warm-up 
      // https://extremelearning.com.au/fisher-yates-algorithm/
      for (let j = N - 1; j > 0; j--) {
        const k = Math.floor(Math.random() * (j + 1));
        const tmp = indices[j];
        indices[j] = indices[k];
        indices[k] = tmp;
      }
      const start = performance.now();

      for (let j = 0; j < N; j++) {
        const stride = indices[j] * LINE_SIZE;
        let val = M[stride]; // Access first element of each cache line
      }

      const end = performance.now();
      times[i] = end - start;
    }

    const median = getMedian(times);

    results.push({ N, times, median });
    console.log(`Measured ${N} cache lines (median: ${median.toFixed(4)}ms)`);
  }

  return results;
}

// optional: Hello World
console.log("Hello, World!");

document.getElementById(
  "exercise1-values"
).innerText = (() => {
  const times = measureOneLine();
  return `1 Cache Line: [${times.map((t) => t.toFixed(4)).join(", ")}] (median: ${getMedian(times).toFixed(4)}ms)`;
})();

document.getElementById(
  "exercise2-values"
).innerHTML = measureNLines()
  .map(
    ({ N, times, median }) =>
      `${N} cache lines: [${times.map((t) => t.toFixed(4)).join(", ")}] (median: ${median.toFixed(4)}ms)`
  )
  .join("<br>");

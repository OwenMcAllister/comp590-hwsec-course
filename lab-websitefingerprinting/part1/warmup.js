const runs = 10;
console.log("Warmup started");

function measureOneLine() {
  const LINE_SIZE = 16; // 128/sizeof(double) Note that js treats all numbers as double
  let result = [];

  // Fill with -1 to ensure allocation
  const M = new Array(runs * LINE_SIZE).fill(-1);

  for (let i = 0; i < runs; i++) {
    const start = performance.now();
    let val = M[i * LINE_SIZE];
    const end = performance.now();

    result.push(end - start);
  }

  return result;
}

function measureNLines(N) {
  // Goal: Determine the timing resolution (quality of JS timer) of performance.now() by measuring the latency of accessing multiple cache lines.
  let result = [];

  // My cache line is 128 bytes
  const LINE_SIZE = 16; // 128/sizeof(double) Note that js treats all numbers as double
  
  // We need to access an array in such a way that we touch N different cache lines
  
  const M = new Array(runs * N * LINE_SIZE).fill(-1); // Fill with -1 to ensure allocation

  for (let i = 0; i < runs; i++) {
    const start = performance.now();
    for (let j = 0; j < N; j++) {
      stride = i * N * LINE_SIZE + j * LINE_SIZE
      // i * N * LINE_SIZE to ensure we are in the right run
      // j * LINE_SIZE to access different cache lines
      let val = M[stride]; // Access one element per cache line
    }
    const end = performance.now();

    result.push(end - start);
  }

  return result;
}

function median(values) {
  values.sort((a, b) => a - b);
  const half = Math.floor(values.length / 2);

  if (values.length % 2) {
    return values[half];
  } else {
    return (values[half - 1] + values[half]) / 2.0;
  }
}

measurementResults = measureNLines(100000);

document.getElementById(
  "exercise1-values"
).innerText = `1 Cache Line: [${measureOneLine().join(", ")}]`;

document.getElementById(
  "exercise2-values"
).innerText = `N Cache Lines: [${measurementResults.join(", ")}]`;

document.getElementById(
  "median"
).innerText = `Median time for N cache line access: ${median(measurementResults)} ms`;
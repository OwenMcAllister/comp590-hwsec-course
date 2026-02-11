console.log("Hello World!");

const runs = 10;

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

function measureNLines() {
  const LINE_SIZE = 16;
  const N = 10000;
  let result = [];

  const M = new Array(N * LINE_SIZE).fill(-1);

  for (let i=0; i < runs; i++){
    let sum = 0;
    const start = performance.now();
    for (let i=0; i < N; i++){
      sum += M[i * LINE_SIZE];
    }
    const end = performance.now();
    result.push(end - start);
  }
  

  // TODO: Exercise 1-1

  return result;
}

document.getElementById(
  "exercise1-values"
).innerText = `1 Cache Line: [${measureOneLine().join(", ")}]`;

document.getElementById(
  "exercise2-values"
).innerText = `N Cache Lines: [${measureNLines().join(", ")}]`;

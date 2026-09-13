# options-pricing-calculator

A simple options pricing calculator written in C++

https://pheneas123.github.io/simple-options-pricing-calculator/

## Features

- **European (Black–Scholes)**  
  - Price: closed-form  
  - Greeks: closed-form (Delta, Gamma, Vega, Theta, Rho)

- **Binary (cash-or-nothing)**  
  - Price: closed-form  
  - Greeks: closed-form

- **American options**  
  - Price: CRR binomial with early exercise  
  - Greeks: tree-node Delta/Gamma; finite differences for Vega/Theta/Rho

- Handles dividend yield `q` and continuous compounding.

## Plans
- Finish maths pdf with American options
- Improve FD mode with confidence intervals

## Validation

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/wasm_reference > build/reference.jsonl
node tests/wasm_tests.cjs build/reference.jsonl
node tests/frontend_tests.cjs
```

The browser-engine check requires Node.js 18 or newer and compares the bundled
`docs/options.js` / `docs/options.wasm` against current C++. After changing the
engine, rebuild both assets with Emscripten before committing:

```sh
emcmake cmake -S . -B build/wasm -DBUILD_TESTING=OFF
cmake --build build/wasm --parallel
```

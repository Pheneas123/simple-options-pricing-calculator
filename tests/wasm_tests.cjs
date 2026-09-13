// Compare the checked-in browser engine against a freshly compiled native build.
// Usage: wasm_reference > reference.jsonl && node tests/wasm_tests.cjs reference.jsonl
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const docs = path.join(__dirname, '../docs');
const context = {
  console, WebAssembly, TextDecoder, TextEncoder, URL, setTimeout, clearTimeout,
  performance, document: { currentScript: { src: 'http://localhost/options.js' } },
  window: {},
  fetch: async url => {
    assert(String(url).endsWith('/options.wasm'), `Unexpected asset request: ${url}`);
    return new Response(fs.readFileSync(path.join(docs, 'options.wasm')),
      { headers: { 'Content-Type': 'application/wasm' } });
  }
};
vm.createContext(context);
vm.runInContext(fs.readFileSync(path.join(docs, 'options.js'), 'utf8'), context);
(async () => {
  assert(process.argv[2], 'Pass the native reference JSONL file path');
  const module = await context.createModule();
  const cases = fs.readFileSync(process.argv[2], 'utf8').trim().split('\n').map(JSON.parse);
  const metrics = ['price', 'delta', 'gamma', 'vega', 'theta', 'rho'];
  for (const test of cases) {
    const args = [test.call ? module.Type.Call : module.Type.Put, test.spot, 100, 0.05, 0.02, 0.2, 1];
    const actual = test.model === 0 ? module.black_scholes(...args)
      : test.model === 1 ? module.binary_cash_or_nothing(...args, 1)
      : module.american_option(...args, 300);
    metrics.forEach((metric, i) => {
      const expected = test.values[i];
      assert(Number.isFinite(actual[metric]) && Math.abs(actual[metric] - expected) <= 1e-6 * Math.max(1, Math.abs(expected)),
        `WASM mismatch: model=${test.model} call=${test.call} spot=${test.spot} ${metric}: ${actual[metric]} versus ${expected}`);
    });
  }
  console.log(`WASM/native parity: ${cases.length * metrics.length} checks passed`);
})().catch(error => { console.error(error); process.exitCode = 1; });

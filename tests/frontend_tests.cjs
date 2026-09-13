const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const nodes = new Map();
const calls = [];
const canvasMessages = [];
const ctx = new Proxy({}, { get(target, key) {
  if (key === 'createLinearGradient') return () => ({ addColorStop() {} });
  if (key === 'fillText') return text => canvasMessages.push(text);
  return target[key] ?? (() => {});
} });
const html = fs.readFileSync(path.join(__dirname, '../docs/index.html'), 'utf8');
for (const match of html.matchAll(/id="([^"]+)"/g)) {
  nodes.set(match[1], {
    value: '', textContent: '', style: {}, listeners: {},
    addEventListener(type, fn) { this.listeners[type] = fn; },
    dispatchEvent(event) { this.listeners[event.type]?.(); },
    getBoundingClientRect: () => ({ width: 600, height: 310 }),
    getContext: () => ctx
  });
}
for (const [key, value] of Object.entries({ S: 100, K: 100, r: 0.05, q: 0, sigma: 0.2, T: 1 })) {
  nodes.get(`${key}_num`).value = String(value);
}
nodes.get('curve_model').value = 'european';
nodes.get('curve_type').value = nodes.get('heatmap_type').value = 'call';
// Browser input.value always converts assigned numbers to strings.
for (const node of nodes.values()) {
  let value = node.value;
  Object.defineProperty(node, 'value', { get: () => value, set: next => { value = String(next); } });
}
const result = price => ({ price, delta: 0.5, gamma: 0.02, vega: 30, theta: -5, rho: 40 });
const engine = {
  Type: { Call: 1, Put: -1 },
  black_scholes: () => result(10),
  binary_cash_or_nothing: (...args) => { assert.equal(args[7], 1); return result(0.5); },
  american_option: (...args) => {
    calls.push(args);
    const [, , , r, q, sigma, T, steps] = args;
    const u = Math.exp(sigma * Math.sqrt(T / steps));
    const p = (Math.exp((r - q) * T / steps) - 1 / u) / (u - 1 / u);
    return result(p < 0 || p > 1 ? NaN : 11);
  }
};
let pending;
const context = {
  document: { getElementById: id => { assert(nodes.has(id), `Missing HTML element: ${id}`); return nodes.get(id); } },
  window: { devicePixelRatio: 1, addEventListener() {} },
  console, Event: class { constructor(type) { this.type = type; } },
  setTimeout: fn => { pending = fn; return 1; }, clearTimeout: () => { pending = null; },
  createModule: async () => engine
};
vm.createContext(context);
const source = fs.readFileSync(path.join(__dirname, '../docs/app.js'), 'utf8');
vm.runInContext(source.replace(/\ninit\(\);\s*$/, ''), context);
const flush = () => { const fn = pending; pending = null; fn?.(); };
const compute = () => { vm.runInContext('compute()', context); flush(); };
(async () => {
  await vm.runInContext('init()', context);
  flush();
  assert.equal(nodes.get('call_price').textContent, '10.00000');
  assert.equal(nodes.get('model_error').textContent, '');
  for (const field of ['r', 'q']) {
    nodes.get(`${field}_num`).value = '';
    compute();
    assert.match(nodes.get('input_error').textContent, /every field/);
    assert.equal(nodes.get('call_price').textContent, '—');
    assert(canvasMessages.slice(-2).every(text => /valid market inputs/.test(text)));
    vm.runInContext('resetInputs()', context); flush();
  }
  nodes.get('curve_model').value = 'american';
  compute();
  assert(calls.length > 40, 'American chart should invoke the model');
  assert(calls.every(args => args[7] === 300), 'Table and chart must use the same resolution');
  nodes.get('r_num').value = '0.25';
  nodes.get('sigma_num').value = '0.01';
  nodes.get('T_num').value = '30';
  compute();
  assert.match(nodes.get('model_error').textContent, /tree probability invalid/);
  assert.equal(nodes.get('amC_price').textContent, '—');
  assert.equal(nodes.get('call_price').textContent, '10.00000');
  assert(canvasMessages.includes('Model unavailable for these inputs; see results above.'));
  vm.runInContext('resetInputs()', context); flush();
  assert.equal(nodes.get('input_error').textContent, '');
  assert.equal(nodes.get('model_error').textContent, '');
  assert.equal(nodes.get('amC_price').textContent, '11.00000');
  console.log('Frontend checks passed: blank inputs, reset, chart clearing, payout, tree resolution and model errors');
})().catch(error => { console.error(error); process.exitCode = 1; });

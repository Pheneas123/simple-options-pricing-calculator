
let Module = null; // Emscripten module instance
const READY = { wasm: false }; // track readiness

function $(id){ return document.getElementById(id); }
function to6(x){ return Number.isFinite(x) ? x.toFixed(6) : "—"; }
function setErr(msg){
  // surface any load/compute errors in the table
  const ids = ["call_price","put_price","call_delta","put_delta","call_gamma","put_gamma","call_vega","put_vega","call_theta","put_theta","call_rho","put_rho"];
  ids.forEach(i => { const el = $(i); if (el) el.textContent = msg; });
}

async function loadWasm(){
  try {
    if (typeof createModule !== "function") {
      throw new Error("options.js (createModule) not found. Check script tag order/paths.");
    }
    Module = await createModule();   // provided by options.js
    READY.wasm = true;
    compute(); // run once when ready
  } catch (err) {
    console.error(err);
    setErr("WASM load failed (see console)");
  }
}

function syncPair(numId, rngId, min, max, step){
  const n = $(numId), r = $(rngId);
  r.min = min; r.max = max; r.step = step; n.step = step;

  // keep initial values in sync
  if (n.value !== "") r.value = Math.min(+max, Math.max(+min, +n.value));
  else n.value = r.value;

  const clamp = v => Math.min(+max, Math.max(+min, +v));
  n.addEventListener('input', () => { r.value = clamp(n.value); compute(); });
  r.addEventListener('input', () => { n.value = r.value; compute(); });
}

function readState(){
  return {
    S: parseFloat($('S_num').value),
    K: parseFloat($('K_num').value),
    r: parseFloat($('r_num').value),
    q: parseFloat($('q_num').value),
    sigma: parseFloat($('sigma_num').value),
    T: parseFloat($('T_num').value),
  };
}

function renderPair(prefix, res){
  $(`${prefix}_price`).textContent = to6(res.price);
  $(`${prefix}_delta`).textContent = to6(res.delta);
  $(`${prefix}_gamma`).textContent = to6(res.gamma);
  $(`${prefix}_vega`).textContent  = to6(res.vega);
  $(`${prefix}_theta`).textContent = to6(res.theta);
  $(`${prefix}_rho`).textContent   = to6(res.rho);
}

let timer;
function compute(){
  // Bindings not ready yet: just skip until loadWasm resolves.
  if (!READY.wasm || !Module || !Module.black_scholes) return;

  clearTimeout(timer);
  timer = setTimeout(() => {
    const s = readState();
    if (!(s.S>0 && s.K>0 && s.sigma>0 && s.T>0)) return;

    try {
      const callRes = Module.black_scholes(Module.Type.Call, s.S, s.K, s.r, s.q, s.sigma, s.T);
      const putRes  = Module.black_scholes(Module.Type.Put,  s.S, s.K, s.r, s.q, s.sigma, s.T);
      renderPair('call', callRes);
      renderPair('put',  putRes);
    } catch (e) {
      console.error(e);
      setErr("Compute error (see console)");
    }
  }, 50);
}

(function init(){
  // Initial placeholders
  $('call_price').textContent = 'Loading…';
  $('put_price').textContent  = 'Loading…';

  // Wire inputs immediately (so sliders mirror numbers even before WASM is ready)
  syncPair('S_num','S_rng',1,1000,1);
  syncPair('K_num','K_rng',1,1000,1);
  syncPair('r_num','r_rng',-0.05,0.15,0.0001);
  syncPair('q_num','q_rng',0.00,0.10,0.0001);
  syncPair('sigma_num','sigma_rng',0.01,2.00,0.0001);
  syncPair('T_num','T_rng',0.01,10.00,0.01);

  // Kick off WASM load (async)
  loadWasm();
})();

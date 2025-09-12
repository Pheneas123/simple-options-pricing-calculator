// app.js
let Module;

function $(id){ return document.getElementById(id); }
function to6(x){ return Number.isFinite(x) ? x.toFixed(6) : "—"; }

async function loadWasm(){
  // options.js was loaded by a <script> tag; it defines createModule(...)
  Module = await createModule();   // <-- this comes from options.js
}

function syncPair(numId, rngId, min, max, step){
  const n = $(numId), r = $(rngId);
  r.min = min; r.max = max; r.step = step; n.step = step;
  const clamp = v => Math.min(+max, Math.max(+min, +v));
  n.addEventListener('input', () => { r.value = clamp(n.value); compute(); });
  r.addEventListener('input', () => { n.value = r.value; compute(); });
}

function readState(){
  return {
    type: $('type').value === 'Call' ? Module.Type.Call : Module.Type.Put,
    S: parseFloat($('S_num').value),
    K: parseFloat($('K_num').value),
    r: parseFloat($('r_num').value),
    q: parseFloat($('q_num').value),
    sigma: parseFloat($('sigma_num').value),
    T: parseFloat($('T_num').value),
  };
}

function render(res){
  $('price').textContent = to6(res.price);
  $('delta').textContent = to6(res.delta);
  $('gamma').textContent = to6(res.gamma);
  $('vega').textContent  = to6(res.vega);
  $('theta').textContent = to6(res.theta);
  $('rho').textContent   = to6(res.rho);
}

let timer;
function compute(){
  clearTimeout(timer);
  timer = setTimeout(() => {
    const s = readState();
    if (!(s.S>0 && s.K>0 && s.sigma>0 && s.T>0)) return;
    const res = Module.black_scholes(s.type, s.S, s.K, s.r, s.q, s.sigma, s.T);
    render(res);
  }, 60);
}

(async function init(){
  $('price').textContent = 'Loading…';
  await loadWasm();

  syncPair('S_num','S_rng',1,1000,1);
  syncPair('K_num','K_rng',1,1000,1);
  syncPair('r_num','r_rng',-0.05,0.15,0.0001);
  syncPair('q_num','q_rng',0.00,0.10,0.0001);
  syncPair('sigma_num','sigma_rng',0.01,2.00,0.0001);
  syncPair('T_num','T_rng',0.01,10.00,0.01);

  $('type').addEventListener('change', compute);
  compute();
})();

let Module = null;
let timer;

const $ = id => document.getElementById(id);
const outputPrefixes = ["call", "put", "binC", "binP", "amC", "amP"];
const defaults = { S: 100, K: 100, r: 0.05, q: 0, sigma: 0.2, T: 1 };

function formatNumber(value) {
  if (!Number.isFinite(value)) return "—";
  const magnitude = Math.abs(value);
  if (magnitude !== 0 && magnitude < 0.0001) return value.toExponential(3);
  return value.toFixed(magnitude >= 100 ? 3 : 5);
}

function setOutputs(message) {
  outputPrefixes.forEach(prefix => {
    ["price", "delta", "gamma", "vega", "theta", "rho"].forEach(metric => {
      $(`${prefix}_${metric}`).textContent = message;
    });
  });
}

function syncPair(numId, rangeId, min, max, step) {
  const number = $(numId);
  const range = $(rangeId);
  Object.assign(range, { min, max, step });
  number.step = step;

  const updateFill = () => {
    const percent = ((Number(range.value) - min) / (max - min)) * 100;
    range.style.background = `linear-gradient(90deg, var(--accent) ${percent}%, var(--border) ${percent}%)`;
  };

  range.value = Math.min(max, Math.max(min, Number(number.value)));
  updateFill();
  number.addEventListener("input", () => {
    range.value = Math.min(max, Math.max(min, Number(number.value)));
    updateFill();
    compute();
  });
  range.addEventListener("input", () => {
    number.value = range.value;
    updateFill();
    compute();
  });
}

function readState() {
  return {
    S: Number($("S_num").value),
    K: Number($("K_num").value),
    r: Number($("r_num").value),
    q: Number($("q_num").value),
    sigma: Number($("sigma_num").value),
    T: Number($("T_num").value)
  };
}

function validate(state) {
  if (![state.S, state.K, state.r, state.q, state.sigma, state.T].every(Number.isFinite)) return "Enter a number in every field.";
  if (state.S <= 0 || state.K <= 0) return "Spot and strike must be greater than zero.";
  if (state.sigma <= 0 || state.T <= 0) return "Volatility and time must be greater than zero.";
  return "";
}

function render(prefix, result) {
  ["price", "delta", "gamma", "vega", "theta", "rho"].forEach(metric => {
    $(`${prefix}_${metric}`).textContent = formatNumber(result[metric]);
  });
}

function optionType(value) {
  return value === "call" ? Module.Type.Call : Module.Type.Put;
}

function resizeCanvas(canvas) {
  const ratio = window.devicePixelRatio || 1;
  const rect = canvas.getBoundingClientRect();
  const width = Math.max(1, Math.round(rect.width * ratio));
  const height = Math.max(1, Math.round(rect.height * ratio));
  if (canvas.width !== width || canvas.height !== height) {
    canvas.width = width;
    canvas.height = height;
  }
  const context = canvas.getContext("2d");
  context.setTransform(ratio, 0, 0, ratio, 0, 0);
  return { context, width: rect.width, height: rect.height };
}

function drawValueChart(state) {
  const canvas = $("value_chart");
  const { context: ctx, width, height } = resizeCanvas(canvas);
  const padding = { top: 12, right: 14, bottom: 34, left: 48 };
  const innerWidth = width - padding.left - padding.right;
  const innerHeight = height - padding.top - padding.bottom;
  const isCall = $("curve_type").value === "call";
  const model = $("curve_model").value;
  const type = optionType(isCall ? "call" : "put");
  const minSpot = Math.max(0.01, Math.min(state.S, state.K) * 0.45);
  const maxSpot = Math.max(state.S, state.K) * 1.55;
  const points = 40;
  const values = [];

  for (let i = 0; i <= points; i++) {
    const spot = minSpot + (maxSpot - minSpot) * i / points;
    const result = model === "american"
      ? Module.american_option(type, spot, state.K, state.r, state.q, state.sigma, state.T, 100)
      : Module.black_scholes(type, spot, state.K, state.r, state.q, state.sigma, state.T);
    values.push({ spot, value: result.price, payoff: isCall ? Math.max(spot - state.K, 0) : Math.max(state.K - spot, 0) });
  }

  const maxValue = Math.max(1, ...values.flatMap(point => [point.value, point.payoff])) * 1.12;
  const x = spot => padding.left + (spot - minSpot) / (maxSpot - minSpot) * innerWidth;
  const y = value => padding.top + innerHeight - value / maxValue * innerHeight;
  ctx.clearRect(0, 0, width, height);
  ctx.font = "9px DM Mono";
  ctx.fillStyle = "#697384";
  ctx.strokeStyle = "#252d3a";
  ctx.lineWidth = 1;

  for (let i = 0; i <= 4; i++) {
    const value = maxValue * i / 4;
    const py = y(value);
    ctx.beginPath(); ctx.moveTo(padding.left, py); ctx.lineTo(width - padding.right, py); ctx.stroke();
    ctx.textAlign = "right"; ctx.fillText(value.toFixed(value >= 10 ? 0 : 1), padding.left - 8, py + 3);
  }
  for (let i = 0; i <= 4; i++) {
    const spot = minSpot + (maxSpot - minSpot) * i / 4;
    ctx.textAlign = "center"; ctx.fillText(spot.toFixed(0), x(spot), height - 8);
  }

  ctx.save();
  ctx.setLineDash([4, 5]);
  ctx.strokeStyle = "#778193";
  ctx.beginPath(); values.forEach((point, i) => i ? ctx.lineTo(x(point.spot), y(point.payoff)) : ctx.moveTo(x(point.spot), y(point.payoff))); ctx.stroke();
  ctx.strokeStyle = "#ffad66";
  ctx.beginPath(); ctx.moveTo(x(state.S), padding.top); ctx.lineTo(x(state.S), padding.top + innerHeight); ctx.stroke();
  ctx.restore();

  const gradient = ctx.createLinearGradient(0, padding.top, 0, padding.top + innerHeight);
  gradient.addColorStop(0, "rgba(85,230,165,.2)"); gradient.addColorStop(1, "rgba(85,230,165,0)");
  ctx.beginPath(); values.forEach((point, i) => i ? ctx.lineTo(x(point.spot), y(point.value)) : ctx.moveTo(x(point.spot), y(point.value)));
  ctx.lineTo(x(maxSpot), y(0)); ctx.lineTo(x(minSpot), y(0)); ctx.closePath(); ctx.fillStyle = gradient; ctx.fill();
  ctx.beginPath(); values.forEach((point, i) => i ? ctx.lineTo(x(point.spot), y(point.value)) : ctx.moveTo(x(point.spot), y(point.value)));
  ctx.strokeStyle = "#55e6a5"; ctx.lineWidth = 2; ctx.stroke();
}

function heatColor(value, min, max) {
  const t = max === min ? 0.5 : (value - min) / (max - min);
  const colors = [[18, 27, 38], [20, 113, 106], [85, 230, 165]];
  const scaled = t * 2;
  const index = Math.min(1, Math.floor(scaled));
  const local = scaled - index;
  return `rgb(${colors[index].map((channel, i) => Math.round(channel + (colors[index + 1][i] - channel) * local)).join(",")})`;
}

function drawHeatmap(state) {
  const canvas = $("price_heatmap");
  const { context: ctx, width, height } = resizeCanvas(canvas);
  const padding = { top: 8, right: 16, bottom: 38, left: 48 };
  const cols = 15, rows = 11;
  const cellWidth = (width - padding.left - padding.right) / cols;
  const cellHeight = (height - padding.top - padding.bottom) / rows;
  const minSpot = Math.max(0.01, state.S * 0.65);
  const maxSpot = state.S * 1.35;
  const minVol = Math.max(0.01, state.sigma * 0.45);
  const maxVol = Math.min(2, Math.max(minVol + 0.05, state.sigma * 1.65));
  const type = optionType($("heatmap_type").value);
  const cells = [];

  for (let row = 0; row < rows; row++) {
    const vol = maxVol - (maxVol - minVol) * row / (rows - 1);
    for (let col = 0; col < cols; col++) {
      const spot = minSpot + (maxSpot - minSpot) * col / (cols - 1);
      cells.push({ row, col, value: Module.black_scholes(type, spot, state.K, state.r, state.q, vol, state.T).price });
    }
  }
  const values = cells.map(cell => cell.value);
  const min = Math.min(...values), max = Math.max(...values);
  ctx.clearRect(0, 0, width, height);
  cells.forEach(cell => {
    ctx.fillStyle = heatColor(cell.value, min, max);
    ctx.fillRect(padding.left + cell.col * cellWidth, padding.top + cell.row * cellHeight, cellWidth + .5, cellHeight + .5);
  });
  ctx.font = "9px DM Mono"; ctx.fillStyle = "#697384";
  ctx.textAlign = "right"; ctx.fillText(`${(maxVol * 100).toFixed(0)}%`, padding.left - 7, padding.top + 8); ctx.fillText(`${(minVol * 100).toFixed(0)}%`, padding.left - 7, padding.top + rows * cellHeight);
  ctx.textAlign = "center"; ctx.fillText(minSpot.toFixed(0), padding.left, height - 10); ctx.fillText("Spot price", padding.left + cols * cellWidth / 2, height - 10); ctx.fillText(maxSpot.toFixed(0), padding.left + cols * cellWidth, height - 10);
  ctx.save(); ctx.translate(12, padding.top + rows * cellHeight / 2); ctx.rotate(-Math.PI / 2); ctx.fillText("Volatility", 0, 0); ctx.restore();

  const currentX = padding.left + (state.S - minSpot) / (maxSpot - minSpot) * cols * cellWidth;
  const currentY = padding.top + (maxVol - state.sigma) / (maxVol - minVol) * rows * cellHeight;
  ctx.strokeStyle = "#ffffff"; ctx.lineWidth = 1.5; ctx.strokeRect(currentX - cellWidth / 2, currentY - cellHeight / 2, cellWidth, cellHeight);
}

function compute() {
  if (!Module) return;
  clearTimeout(timer);
  timer = setTimeout(() => {
    const state = readState();
    const error = validate(state);
    $("input_error").textContent = error;
    if (error) { setOutputs("—"); return; }

    try {
      render("call", Module.black_scholes(Module.Type.Call, state.S, state.K, state.r, state.q, state.sigma, state.T));
      render("put", Module.black_scholes(Module.Type.Put, state.S, state.K, state.r, state.q, state.sigma, state.T));
      render("binC", Module.binary_cash_or_nothing(Module.Type.Call, state.S, state.K, state.r, state.q, state.sigma, state.T, 1));
      render("binP", Module.binary_cash_or_nothing(Module.Type.Put, state.S, state.K, state.r, state.q, state.sigma, state.T, 1));
      render("amC", Module.american_option(Module.Type.Call, state.S, state.K, state.r, state.q, state.sigma, state.T, 300));
      render("amP", Module.american_option(Module.Type.Put, state.S, state.K, state.r, state.q, state.sigma, state.T, 300));
      drawValueChart(state);
      drawHeatmap(state);
    } catch (error) {
      console.error(error);
      setOutputs("Error");
    }
  }, 70);
}

function resetInputs() {
  Object.entries(defaults).forEach(([key, value]) => {
    $(`${key}_num`).value = value;
    $(`${key}_rng`).value = value;
    $(`${key}_rng`).dispatchEvent(new Event("input"));
  });
}

async function init() {
  syncPair("S_num", "S_rng", 1, 1000, 1);
  syncPair("K_num", "K_rng", 1, 1000, 1);
  syncPair("r_num", "r_rng", -0.05, 0.25, 0.0001);
  syncPair("q_num", "q_rng", 0, 0.15, 0.0001);
  syncPair("sigma_num", "sigma_rng", 0.01, 1.5, 0.0001);
  syncPair("T_num", "T_rng", 0.01, 30, 0.01);
  $("reset_btn").addEventListener("click", resetInputs);
  ["curve_model", "curve_type", "heatmap_type"].forEach(id => $(id).addEventListener("change", compute));
  window.addEventListener("resize", compute);

  try {
    Module = await createModule();
    compute();
  } catch (error) {
    console.error(error);
    setOutputs("Unavailable");
  }
}

init();

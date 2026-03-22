const canvas = document.getElementById('view');
const ctx = canvas.getContext('2d');

const el = {
  familyType: document.getElementById('familyType'),
  n: document.getElementById('n'),
  t0: document.getElementById('t0'),
  h: document.getElementById('h'),
  q: document.getElementById('q'),
  ma: document.getElementById('ma'),
  mb: document.getElementById('mb'),
  mc: document.getElementById('mc'),
  md: document.getElementById('md'),
  zoom: document.getElementById('zoom'),
  showParabola: document.getElementById('showParabola'),
  showLabels: document.getElementById('showLabels'),
  showIntersections: document.getElementById('showIntersections'),
  showSumBands: document.getElementById('showSumBands'),
  showPairSegments: document.getElementById('showPairSegments'),
  nVal: document.getElementById('nVal'),
  t0Val: document.getElementById('t0Val'),
  hVal: document.getElementById('hVal'),
  qVal: document.getElementById('qVal'),
  maVal: document.getElementById('maVal'),
  mbVal: document.getElementById('mbVal'),
  mcVal: document.getElementById('mcVal'),
  mdVal: document.getElementById('mdVal'),
  zoomVal: document.getElementById('zoomVal'),
  arithGroup: document.getElementById('arithGroup'),
  geomGroup: document.getElementById('geomGroup'),
  mobiusGroup: document.getElementById('mobiusGroup'),
  stats: document.getElementById('stats'),
};

function fmt(x, digits = 3) {
  if (!Number.isFinite(x)) return '—';
  return Number(x).toFixed(digits).replace(/\.0+$/, '').replace(/(\.\d*?)0+$/, '$1');
}

function resizeCanvas() {
  const dpr = window.devicePixelRatio || 1;
  const rect = canvas.getBoundingClientRect();
  canvas.width = Math.round(rect.width * dpr);
  canvas.height = Math.round(rect.height * dpr);
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  draw();
}

function worldToScreen(x, y, state) {
  return {
    x: state.cx + x * state.scale,
    y: state.cy - y * state.scale,
  };
}

function lineAtT(t) {
  return { t, m: 2 * t, b: -t * t };
}

function intersectionOfTangents(a, b) {
  return {
    x: (a + b) / 2,
    y: a * b,
  };
}

function buildParameters() {
  const familyType = el.familyType.value;
  const n = parseInt(el.n.value, 10);
  const t0 = parseFloat(el.t0.value);
  const params = [];
  const warnings = [];

  if (familyType === 'arithmetic') {
    const h = parseFloat(el.h.value);
    for (let i = 0; i < n; i += 1) params.push(t0 + i * h);
  } else if (familyType === 'geometric') {
    const q = parseFloat(el.q.value);
    for (let i = 0; i < n; i += 1) params.push(t0 * Math.pow(q, i));
  } else {
    const a = parseFloat(el.ma.value);
    const b = parseFloat(el.mb.value);
    const c = parseFloat(el.mc.value);
    const d = parseFloat(el.md.value);
    let t = t0;
    const det = a * d - b * c;
    if (Math.abs(det) < 1e-8) {
      warnings.push('ad - bc ≈ 0: преобразование вырождено.');
    }
    for (let i = 0; i < n; i += 1) {
      if (Number.isFinite(t)) params.push(t);
      const den = c * t + d;
      if (Math.abs(den) < 1e-6) {
        warnings.push(`Шаг ${i} → ${i + 1}: знаменатель почти 0, орбита обрывается.`);
        break;
      }
      t = (a * t + b) / den;
      if (!Number.isFinite(t) || Math.abs(t) > 1e6) {
        warnings.push(`Шаг ${i} → ${i + 1}: параметр ушёл слишком далеко.`);
        break;
      }
    }
  }

  return { params, warnings };
}

function makeGroupLine(sum, pts) {
  if (pts.length < 2) return null;
  const ordered = pts.slice().sort((a, b) => a.x - b.x || a.y - b.y);
  const first = ordered[0];
  const last = ordered[ordered.length - 1];
  const dx = last.x - first.x;
  const dy = last.y - first.y;

  if (Math.abs(dx) < 1e-9 && Math.abs(dy) < 1e-9) return null;

  const A = first.y - last.y;
  const B = last.x - first.x;
  const C = first.x * last.y - last.x * first.y;

  return { sum, pts: ordered, A, B, C };
}

function intersectLines(line1, line2) {
  const det = line1.A * line2.B - line2.A * line1.B;
  if (Math.abs(det) < 1e-9) return null;
  return {
    x: (line1.B * line2.C - line2.B * line1.C) / det,
    y: (line2.A * line1.C - line1.A * line2.C) / det,
  };
}

function uniquePoints(points) {
  const out = [];
  points.forEach((pt) => {
    const exists = out.some((other) =>
      Math.abs(other.x - pt.x) < 1e-7 && Math.abs(other.y - pt.y) < 1e-7
    );
    if (!exists) out.push(pt);
  });
  return out;
}

function dedupeLineIntersections(points) {
  const buckets = new Map();
  points.forEach((pt) => {
    const key = `${Math.round(pt.x * 1e6)}:${Math.round(pt.y * 1e6)}`;
    if (!buckets.has(key)) {
      buckets.set(key, { x: pt.x, y: pt.y, sums: [...pt.sums], multiplicity: 1 });
      return;
    }
    const existing = buckets.get(key);
    existing.multiplicity += 1;
    existing.sums.push(...pt.sums);
  });
  return [...buckets.values()];
}

function clipInfiniteLineToRect(line, state) {
  const candidates = [];
  const { A, B, C } = line;
  const pushIfInside = (x, y) => {
    if (!Number.isFinite(x) || !Number.isFinite(y)) return;
    if (x < state.minX - 1e-7 || x > state.maxX + 1e-7) return;
    if (y < state.minY - 1e-7 || y > state.maxY + 1e-7) return;
    candidates.push({ x, y });
  };

  if (Math.abs(B) > 1e-9) {
    pushIfInside(state.minX, (-A * state.minX - C) / B);
    pushIfInside(state.maxX, (-A * state.maxX - C) / B);
  }
  if (Math.abs(A) > 1e-9) {
    pushIfInside((-B * state.minY - C) / A, state.minY);
    pushIfInside((-B * state.maxY - C) / A, state.maxY);
  }

  const points = uniquePoints(candidates);
  if (points.length < 2) return null;

  let best = null;
  let bestDist = -1;
  for (let i = 0; i < points.length; i += 1) {
    for (let j = i + 1; j < points.length; j += 1) {
      const dx = points[i].x - points[j].x;
      const dy = points[i].y - points[j].y;
      const dist = dx * dx + dy * dy;
      if (dist > bestDist) {
        bestDist = dist;
        best = [points[i], points[j]];
      }
    }
  }
  return best;
}

function computeData() {
  const familyType = el.familyType.value;
  const zoom = parseFloat(el.zoom.value);
  const { params, warnings } = buildParameters();
  const lines = params.map(lineAtT);
  const intersections = [];
  const groups = new Map();

  for (let i = 0; i < params.length; i += 1) {
    for (let j = i + 1; j < params.length; j += 1) {
      const pt = intersectionOfTangents(params[i], params[j]);
      const sum = i + j;
      const item = { i, j, sum, ...pt };
      intersections.push(item);
      if (!groups.has(sum)) groups.set(sum, []);
      groups.get(sum).push(item);
    }
  }

  const groupLines = [...groups.entries()]
    .map(([sum, pts]) => makeGroupLine(sum, pts))
    .filter(Boolean);

  const groupLineIntersections = [];
  if (el.showPairSegments.checked) {
    for (let i = 0; i < groupLines.length; i += 1) {
      for (let j = i + 1; j < groupLines.length; j += 1) {
        const pt = intersectLines(groupLines[i], groupLines[j]);
        if (!pt) continue;
        if (!Number.isFinite(pt.x) || !Number.isFinite(pt.y)) continue;
        if (Math.abs(pt.x) > 1e4 || Math.abs(pt.y) > 1e4) continue;
        groupLineIntersections.push({
          ...pt,
          sums: [groupLines[i].sum, groupLines[j].sum],
        });
      }
    }
  }
  const uniqueGroupLineIntersections = dedupeLineIntersections(groupLineIntersections);

  const xs = [];
  const ys = [];
  params.forEach((t) => {
    xs.push(t);
    ys.push(t * t);
  });
  intersections.forEach((p) => {
    xs.push(p.x);
    ys.push(p.y);
  });
  uniqueGroupLineIntersections.forEach((p) => {
    xs.push(p.x);
    ys.push(p.y);
  });

  if (xs.length === 0) {
    xs.push(-3, 3);
    ys.push(-1, 5);
  }

  let minX = Math.min(...xs);
  let maxX = Math.max(...xs);
  let minY = Math.min(...ys);
  let maxY = Math.max(...ys);

  const padX = Math.max(0.8, (maxX - minX) * 0.15);
  const padY = Math.max(0.8, (maxY - minY) * 0.15);
  minX -= padX;
  maxX += padX;
  minY -= padY;
  maxY += padY;

  const rect = canvas.getBoundingClientRect();
  const w = rect.width;
  const h = rect.height;
  const scale = zoom;

  const state = {
    w,
    h,
    scale,
    cx: w / 2 - ((minX + maxX) / 2) * scale,
    cy: h / 2 + ((minY + maxY) / 2) * scale,
    minX,
    maxX,
    minY,
    maxY,
  };

  return {
    familyType,
    params,
    lines,
    intersections,
    groups,
    groupLines,
    groupLineIntersections: uniqueGroupLineIntersections,
    warnings,
    state,
  };
}

function drawAxes(state) {
  ctx.save();
  ctx.strokeStyle = '#e5e7eb';
  ctx.lineWidth = 1;

  const x0 = worldToScreen(0, 0, state).x;
  const y0 = worldToScreen(0, 0, state).y;

  ctx.beginPath();
  ctx.moveTo(0, y0);
  ctx.lineTo(state.w, y0);
  ctx.moveTo(x0, 0);
  ctx.lineTo(x0, state.h);
  ctx.stroke();

  ctx.fillStyle = '#6b7280';
  ctx.font = '12px system-ui, sans-serif';
  ctx.fillText('x', state.w - 14, y0 - 6);
  ctx.fillText('y', x0 + 6, 14);
  ctx.restore();
}

function drawParabola(state) {
  ctx.save();
  ctx.strokeStyle = '#0f766e';
  ctx.lineWidth = 2;
  ctx.beginPath();
  let first = true;
  const step = Math.max(0.02, (1 / state.scale) * 8);
  for (let x = state.minX; x <= state.maxX; x += step) {
    const y = x * x;
    const s = worldToScreen(x, y, state);
    if (first) {
      ctx.moveTo(s.x, s.y);
      first = false;
    } else {
      ctx.lineTo(s.x, s.y);
    }
  }
  ctx.stroke();
  ctx.restore();
}

function drawTangents(data) {
  const { state, lines, params } = data;
  const palette = ['#1d4ed8', '#9333ea', '#0891b2', '#b45309', '#be123c', '#047857'];

  lines.forEach((line, idx) => {
    const color = palette[idx % palette.length];
    const x1 = state.minX;
    const y1 = line.m * x1 + line.b;
    const x2 = state.maxX;
    const y2 = line.m * x2 + line.b;
    const p1 = worldToScreen(x1, y1, state);
    const p2 = worldToScreen(x2, y2, state);

    ctx.save();
    ctx.strokeStyle = color;
    ctx.lineWidth = 1.6;
    ctx.beginPath();
    ctx.moveTo(p1.x, p1.y);
    ctx.lineTo(p2.x, p2.y);
    ctx.stroke();

    const touch = worldToScreen(params[idx], params[idx] * params[idx], state);
    ctx.fillStyle = color;
    ctx.beginPath();
    ctx.arc(touch.x, touch.y, 3.3, 0, Math.PI * 2);
    ctx.fill();

    if (el.showLabels.checked) {
      ctx.fillStyle = color;
      ctx.font = '12px system-ui, sans-serif';
      ctx.fillText(`ℓ${idx}  t=${fmt(params[idx], 2)}`, touch.x + 6, touch.y - 8);
    }
    ctx.restore();
  });
}

function drawGroupSegments(data) {
  if (!el.showPairSegments.checked) return;
  const { groupLines, groupLineIntersections, state } = data;

  ctx.save();
  ctx.lineWidth = 2;
  ctx.setLineDash([8, 5]);
  groupLines.forEach((line) => {
    const segment = clipInfiniteLineToRect(line, state);
    if (!segment) return;
    const [a, b] = segment;
    const sa = worldToScreen(a.x, a.y, state);
    const sb = worldToScreen(b.x, b.y, state);
    const hue = (line.sum * 43) % 360;
    ctx.strokeStyle = `hsla(${hue} 65% 45% / 0.9)`;
    ctx.beginPath();
    ctx.moveTo(sa.x, sa.y);
    ctx.lineTo(sb.x, sb.y);
    ctx.stroke();
  });
  ctx.restore();

  if (!groupLineIntersections.length) return;
  ctx.save();
  ctx.strokeStyle = '#111827';
  ctx.lineWidth = 1.5;
  groupLineIntersections.forEach((pt) => {
    const s = worldToScreen(pt.x, pt.y, state);
    ctx.beginPath();
    ctx.moveTo(s.x - 4, s.y - 4);
    ctx.lineTo(s.x + 4, s.y + 4);
    ctx.moveTo(s.x - 4, s.y + 4);
    ctx.lineTo(s.x + 4, s.y - 4);
    ctx.stroke();
  });
  ctx.restore();
}

function drawIntersections(data) {
  if (!el.showIntersections.checked) return;
  const { intersections, state } = data;
  intersections.forEach((pt) => {
    const s = worldToScreen(pt.x, pt.y, state);
    const hue = el.showSumBands.checked ? (pt.sum * 43) % 360 : 220;
    ctx.save();
    ctx.fillStyle = `hsla(${hue} 70% 45% / 0.88)`;
    ctx.beginPath();
    ctx.arc(s.x, s.y, 4.2, 0, Math.PI * 2);
    ctx.fill();
    ctx.strokeStyle = 'white';
    ctx.lineWidth = 1.1;
    ctx.stroke();
    ctx.restore();
  });
}

function renderStats(data) {
  const { familyType, params, intersections, groups, groupLines, groupLineIntersections, warnings } = data;
  const familyName = familyType === 'arithmetic'
    ? 'Арифметическая прогрессия'
    : familyType === 'geometric'
      ? 'Геометрическая прогрессия'
      : 'Дробно-линейная орбита';

  const groupEntries = [...groups.entries()].sort((a, b) => a[0] - b[0]);
  const html = [];
  html.push(`<div class="stat-box"><strong>Тип:</strong> ${familyName}</div>`);
  html.push(`<div class="stat-box"><strong>Фактически построено линий:</strong> ${params.length}</div>`);
  html.push(`<div class="stat-box"><strong>Число попарных пересечений:</strong> ${intersections.length}</div>`);
  html.push(`<div class="stat-box"><strong>Группы с одинаковым i+j:</strong><br>${groupEntries.map(([s, pts]) => `<span class="badge">s=${s}: ${pts.length}</span>`).join(' ') || '—'}</div>`);
  if (el.showPairSegments.checked) {
    html.push(`<div class="stat-box"><strong>Продлённых прямых по i+j:</strong> ${groupLines.length}<br><strong>Их попарных пересечений:</strong> ${groupLineIntersections.length}</div>`);
  }
  html.push(`<div class="stat-box"><strong>Параметры tᵢ:</strong><br>${params.map((t, i) => `<span class="badge">t${i}=${fmt(t, 3)}</span>`).join(' ')}</div>`);
  if (warnings.length) {
    html.push(`<div class="stat-box" style="border-color:#fecaca;background:#fef2f2;"><strong style="color:#991b1b;">Предупреждения:</strong><br>${warnings.map((w) => `<div style="margin-top:6px;">• ${w}</div>`).join('')}</div>`);
  }
  el.stats.innerHTML = html.join('');
}

function updateControlVisibility() {
  const type = el.familyType.value;
  el.arithGroup.style.display = type === 'arithmetic' ? '' : 'none';
  el.geomGroup.style.display = type === 'geometric' ? '' : 'none';
  el.mobiusGroup.style.display = type === 'mobius' ? '' : 'none';
}

function updateValueLabels() {
  el.nVal.textContent = el.n.value;
  el.t0Val.textContent = fmt(parseFloat(el.t0.value), 2);
  el.hVal.textContent = fmt(parseFloat(el.h.value), 3);
  el.qVal.textContent = fmt(parseFloat(el.q.value), 3);
  el.maVal.textContent = fmt(parseFloat(el.ma.value), 2);
  el.mbVal.textContent = fmt(parseFloat(el.mb.value), 2);
  el.mcVal.textContent = fmt(parseFloat(el.mc.value), 2);
  el.mdVal.textContent = fmt(parseFloat(el.md.value), 2);
  el.zoomVal.textContent = el.zoom.value;
}

function clear() {
  ctx.clearRect(0, 0, canvas.width, canvas.height);
  ctx.save();
  ctx.fillStyle = 'white';
  ctx.fillRect(0, 0, canvas.width, canvas.height);
  ctx.restore();
}

function draw() {
  updateControlVisibility();
  updateValueLabels();
  const data = computeData();
  clear();
  drawAxes(data.state);
  if (el.showParabola.checked) drawParabola(data.state);
  drawGroupSegments(data);
  drawTangents(data);
  drawIntersections(data);
  renderStats(data);
}

Object.values(el).forEach((node) => {
  if (node instanceof HTMLElement && ('value' in node || node.type === 'checkbox')) {
    node.addEventListener('input', draw);
    node.addEventListener('change', draw);
  }
});

window.addEventListener('resize', resizeCanvas);
resizeCanvas();

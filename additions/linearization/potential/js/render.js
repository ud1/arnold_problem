(function () {
  const App = window.App;

  function createSVG(tag, attrs) {
    const el = document.createElementNS("http://www.w3.org/2000/svg", tag);
    for (const [k, v] of Object.entries(attrs || {})) el.setAttribute(k, String(v));
    return el;
  }

  function isBlack(cell) {
    const invert = App.state._invertColor || false;
    return invert ? (cell.parity === 0) : (cell.parity === 1);
  }

  function chooseColorOrientation() {
    const cells = App.state.cells;
    if (!cells.length) { App.state._invertColor = false; return; }
    let c0 = 0, c1 = 0;
    for (const c of cells) {
      if (!App.state.flags.showOuter && c.external) continue;
      if (c.parity === 0) c0++; else c1++;
    }
    App.state._invertColor = c0 > c1;
  }

  function sideColorByCount(sideCount) {
    // Delphi-like: clYellow, clRed, clGreen, clPurple, clBlue.
    if (!Number.isFinite(sideCount)) return "#0000ff";
    if (sideCount <= 4) return "#ffff00";
    if (sideCount === 5) return "#ff0000";
    if (sideCount === 6) return "#008000";
    if (sideCount === 7) return "#800080";
    return "#0000ff";
  }

  function logColoredCellsDebug(rows) {
    if (!rows.length) return;
    const dbg = window.__lvColorDebug;
    if (!dbg || !dbg.enabled) return;
    const maxRows = Number.isFinite(dbg.maxRows) ? Math.max(1, Math.floor(dbg.maxRows)) : 80;
    const sample = rows.slice(0, maxRows);
    console.groupCollapsed(`[LineViewer] colored cells debug: total=${rows.length}, shown=${sample.length}`);
    console.table(sample);
    if (rows.length > sample.length) {
      console.log(`[LineViewer] truncated ${rows.length - sample.length} rows`);
    }
    console.groupEnd();
  }

  function distToLine(p, a, b) {
    const dx = b.x - a.x, dy = b.y - a.y;
    const len = Math.hypot(dx, dy);
    if (len < 1e-12) return Math.hypot(p.x - a.x, p.y - a.y);
    return Math.abs((p.x - a.x) * dy - (p.y - a.y) * dx) / len;
  }

  function isOnViewportEdge(p1, p2, vpPoly) {
    const eps = 1e-6;
    for (let i = 0; i < vpPoly.length; i++) {
      const a = vpPoly[i], b = vpPoly[(i + 1) % vpPoly.length];
      if (distToLine(p1, a, b) < eps && distToLine(p2, a, b) < eps) return true;
    }
    return false;
  }

  function drawCells() {
    const clipPoly = App.state.visibleClipPoly;
    if (!clipPoly) return;
    chooseColorOrientation();
    const coloredMode = !!App.state.flags.coloredMode;
    const highlightDefects = !!App.state.flags.highlightDefects;
    const showOuter = !!App.state.flags.showOuter;
    const defectFill = "#ff0000";
    const debugRows = [];
    let cellIndex = 0;

    // Pass 1: external black cells — inset stroke on real edges (only when showOuter is OFF)
    if (!showOuter && !coloredMode) {
      let extId = 0;
      for (const c of App.state.cells) {
        if (!c.external) continue;
        if (!isBlack(c)) continue;
        const clipped = App.clipPolygonByConvex(c.poly, clipPoly);
        if (clipped.length < 3) continue;
        const realEdges = [];
        for (let i = 0; i < clipped.length; i++) {
          const p1 = clipped[i];
          const p2 = clipped[(i + 1) % clipped.length];
          if (!isOnViewportEdge(p1, p2, clipPoly)) realEdges.push(p1, p2);
        }
        if (!realEdges.length) continue;
        const polyPoints = clipped.map(p => `${p.x},${p.y}`).join(" ");
        const d = [];
        for (let i = 0; i < realEdges.length; i += 2) {
          d.push(`M${realEdges[i].x},${realEdges[i].y}L${realEdges[i + 1].x},${realEdges[i + 1].y}`);
        }
        const clipId = `_ec${extId++}`;
        const g = createSVG("g", {});
        const cp = createSVG("clipPath", { id: clipId });
        cp.appendChild(createSVG("polygon", { points: polyPoints }));
        g.appendChild(cp);
        g.appendChild(createSVG("path", {
          d: d.join(""),
          fill: "none",
          stroke: "#000000",
          "stroke-width": 0.6,
          "stroke-opacity": 1.0,
          "vector-effect": "non-scaling-stroke",
          "clip-path": `url(#${clipId})`,
        }));
        App.el.polyLayer.appendChild(g);
      }
    }

    // Pass 2: black cells — solid fill
    for (const c of App.state.cells) {
      if (!showOuter && c.external) continue;
      const black = isBlack(c);
      if (!black && !coloredMode) continue;
      const clipped = App.clipPolygonByConvex(c.poly, clipPoly);
      if (clipped.length < 3) continue;
      const sideCount = Number.isFinite(c.sideCount) ? c.sideCount : (c.poly.length + (c.external ? 1 : 0));
      const isDefect = black && highlightDefects && sideCount !== 3;
      const fill = isDefect ? defectFill : (black ? "#000000" : sideColorByCount(sideCount));
      if (!black && coloredMode) {
        debugRows.push({
          idx: cellIndex,
          sideCount,
          polyLen: c.poly.length,
          clippedLen: clipped.length,
          external: !!c.external,
          fill,
        });
      }
      const points = clipped.map(p => `${p.x},${p.y}`).join(" ");
      const attrs = {
        points,
        fill,
        "fill-opacity": 1.0,
        stroke: "none",
      };
      if (isDefect) {
        attrs.stroke = "#000000";
        attrs["stroke-width"] = 0.8;
        attrs["stroke-opacity"] = 0.6;
        attrs["vector-effect"] = "non-scaling-stroke";
      }
      App.el.polyLayer.appendChild(createSVG("polygon", attrs));
      cellIndex++;
    }
    logColoredCellsDebug(debugRows);
  }

  function drawLines() {
    const clipPoly = App.state.visibleClipPoly;
    if (!clipPoly) return;
    const checkerMode = !!App.state.flags.checkerMode;
    const w = checkerMode ? 0.12 : 0.4;
    const lineColor = "#000000";
    for (const line of App.state.lines) {
      const seg = App.lineSegmentInPoly(line, clipPoly);
      if (!seg) continue;
      App.el.lineLayer.appendChild(createSVG("line", {
        x1: seg[0].x, y1: seg[0].y,
        x2: seg[1].x, y2: seg[1].y,
        stroke: lineColor,
        "stroke-width": w,
        "stroke-opacity": 1.0,
        "vector-effect": "non-scaling-stroke",
      }));
    }
  }

  function estimateLabelBox(text, center, fontSize) {
    const width = Math.max(fontSize * 0.9, text.length * fontSize * 0.62);
    const height = fontSize * 1.1;
    return {
      minX: center.x - width * 0.5,
      maxX: center.x + width * 0.5,
      minY: center.y - height * 0.5,
      maxY: center.y + height * 0.5,
    };
  }

  function clamp(v, lo, hi) {
    return Math.max(lo, Math.min(hi, v));
  }

  function nearestViewportEdge(point, viewportWidth, viewportHeight) {
    const distances = [
      { edge: "left", dist: Math.abs(point.x) },
      { edge: "right", dist: Math.abs(viewportWidth - point.x) },
      { edge: "top", dist: Math.abs(point.y) },
      { edge: "bottom", dist: Math.abs(viewportHeight - point.y) },
    ];
    distances.sort((a, b) => a.dist - b.dist);
    return distances[0].edge;
  }

  function chooseLabelDirection(items, index, axisLimit, minOffset) {
    const item = items[index];
    const coord = Number.isFinite(item.spaceIdeal) ? item.spaceIdeal : item.ideal;
    const lowerDivider = index > 0
      ? 0.5 * ((Number.isFinite(items[index - 1].spaceIdeal) ? items[index - 1].spaceIdeal : items[index - 1].ideal) + coord)
      : 0;
    const upperDivider = index + 1 < items.length
      ? 0.5 * (coord + (Number.isFinite(items[index + 1].spaceIdeal) ? items[index + 1].spaceIdeal : items[index + 1].ideal))
      : axisLimit;
    const edgePad = item.size * 0.5;
    const slackMinus = Math.max(0, coord - Math.max(lowerDivider, edgePad) - minOffset);
    const slackPlus = Math.max(0, Math.min(upperDivider, axisLimit - edgePad) - coord - minOffset);

    if (Math.abs(slackPlus - slackMinus) > 1e-9) {
      return slackPlus > slackMinus ? 1 : -1;
    }
    if (Math.abs(upperDivider - lowerDivider) > 1e-9) {
      return (axisLimit - upperDivider) > lowerDivider ? 1 : -1;
    }
    return item.ideal >= axisLimit * 0.5 ? -1 : 1;
  }

  function packEdgeLabels(items, axisLimit, gapPad) {
    const n = items.length;
    if (!n) return;

    const positions = items.map(item => clamp(item.target, item.lo, item.hi));
    for (let i = 1; i < n; i++) {
      const gap = (items[i - 1].size + items[i].size) * 0.5 + gapPad;
      positions[i] = Math.max(positions[i], positions[i - 1] + gap, items[i].lo);
    }
    for (let i = n - 2; i >= 0; i--) {
      const gap = (items[i].size + items[i + 1].size) * 0.5 + gapPad;
      positions[i] = Math.min(positions[i], positions[i + 1] - gap, items[i].hi);
    }
    for (let i = 1; i < n; i++) {
      const gap = (items[i - 1].size + items[i].size) * 0.5 + gapPad;
      positions[i] = Math.max(positions[i], positions[i - 1] + gap, items[i].lo);
    }
    for (let i = 0; i < n; i++) {
      items[i].position = clamp(positions[i], items[i].lo, items[i].hi);
    }
  }

  function layoutEdgeLabels(items, axisLimit) {
    if (!items.length) return;

    items.sort((a, b) => {
      const aCoord = Number.isFinite(a.spaceIdeal) ? a.spaceIdeal : a.ideal;
      const bCoord = Number.isFinite(b.spaceIdeal) ? b.spaceIdeal : b.ideal;
      if (Math.abs(aCoord - bCoord) > 1e-9) return aCoord - bCoord;
      if (Math.abs(a.ideal - b.ideal) > 1e-9) return a.ideal - b.ideal;
      return a.order - b.order;
    });

    for (const item of items) {
      item.ideal = clamp(item.ideal, item.size * 0.5, axisLimit - item.size * 0.5);
      if (Number.isFinite(item.spaceIdeal)) {
        item.spaceIdeal = clamp(item.spaceIdeal, item.size * 0.5, axisLimit - item.size * 0.5);
      }
    }

    const linePad = 6;
    const minOffset = linePad + 0.5;
    for (let i = 0; i < items.length; i++) {
      const item = items[i];
      const edgePad = item.size * 0.5;
      const dir = chooseLabelDirection(items, i, axisLimit, minOffset);
      const minusTarget = item.ideal - (edgePad + linePad);
      const plusTarget = item.ideal + (edgePad + linePad);
      const minusSlack = Math.max(0, item.ideal - edgePad - minOffset);
      const plusSlack = Math.max(0, axisLimit - edgePad - item.ideal - minOffset);
      item.dir = dir;
      item.target = dir < 0 ? minusTarget : plusTarget;
      item.lo = edgePad;
      item.hi = axisLimit - edgePad;

      if (dir < 0) {
        item.hi = Math.min(item.hi, item.ideal - minOffset);
        if (item.hi < item.lo && plusSlack > minusSlack + 1e-9) {
          item.dir = 1;
          item.target = plusTarget;
          item.lo = Math.max(edgePad, item.ideal + minOffset);
          item.hi = axisLimit - edgePad;
        }
      } else {
        item.lo = Math.max(item.lo, item.ideal + minOffset);
        if (item.lo > item.hi && minusSlack > plusSlack + 1e-9) {
          item.dir = -1;
          item.target = minusTarget;
          item.lo = edgePad;
          item.hi = Math.min(axisLimit - edgePad, item.ideal - minOffset);
        }
      }

      if (item.lo > item.hi) {
        item.lo = edgePad;
        item.hi = axisLimit - edgePad;
        item.target = clamp(item.ideal + item.dir * (edgePad + linePad), item.lo, item.hi);
      }
    }

    const gapPad = 2;
    packEdgeLabels(items, axisLimit, gapPad);
  }

  function drawLineNumbers() {
    const clipPoly = App.state.visibleClipPoly;
    if (!clipPoly) return;

    const viewportWidth = App.el.viewer.clientWidth;
    const viewportHeight = App.el.viewer.clientHeight;
    const sm = App.buildScreenMatrix();
    const fontSize = 9.6;
    const edgeInsetPx = 4;
    const linePad = 1.5;
    const edgeItems = { left: [], right: [], top: [], bottom: [] };

    for (let idx = 0; idx < App.state.lines.length; idx++) {
      const seg = App.lineSegmentInPoly(App.state.lines[idx], clipPoly);
      if (!seg) continue;

      const p0 = App.applyMat(sm, seg[0]);
      const p1 = App.applyMat(sm, seg[1]);
      const dx = p1.x - p0.x;
      const dy = p1.y - p0.y;
      const len = Math.hypot(dx, dy);
      if (!Number.isFinite(len) || len <= App.EPS) continue;

      const text = String(idx);
      const endpoints = [
        { point: p0, order: idx * 2 },
        { point: p1, order: idx * 2 + 1 },
      ];

      for (const endpoint of endpoints) {
        const boxAtOrigin = estimateLabelBox(text, { x: 0, y: 0 }, fontSize);
        const labelWidth = boxAtOrigin.maxX - boxAtOrigin.minX;
        const labelHeight = boxAtOrigin.maxY - boxAtOrigin.minY;
        const edge = nearestViewportEdge(endpoint.point, viewportWidth, viewportHeight);
        if (edge === "left" || edge === "right") {
          const x = edge === "left"
            ? edgeInsetPx + labelWidth * 0.5
            : viewportWidth - edgeInsetPx - labelWidth * 0.5;
          const innerX = edge === "left" ? x + linePad : x - linePad;
          const ideal = Math.abs(dx) > 1e-9
            ? endpoint.point.y + (x - endpoint.point.x) * dy / dx
            : endpoint.point.y;
          const spaceIdeal = Math.abs(dx) > 1e-9
            ? endpoint.point.y + (innerX - endpoint.point.x) * dy / dx
            : endpoint.point.y;
          edgeItems[edge].push({
            text,
            order: endpoint.order,
            fixed: x,
            ideal,
            spaceIdeal,
            size: labelHeight,
            axis: "y",
          });
        } else {
          const y = edge === "top"
            ? edgeInsetPx + labelHeight * 0.5
            : viewportHeight - edgeInsetPx - labelHeight * 0.5;
          const innerY = edge === "top" ? y + linePad : y - linePad;
          const ideal = Math.abs(dy) > 1e-9
            ? endpoint.point.x + (y - endpoint.point.y) * dx / dy
            : endpoint.point.x;
          const spaceIdeal = Math.abs(dy) > 1e-9
            ? endpoint.point.x + (innerY - endpoint.point.y) * dx / dy
            : endpoint.point.x;
          edgeItems[edge].push({
            text,
            order: endpoint.order,
            fixed: y,
            ideal,
            spaceIdeal,
            size: labelWidth,
            axis: "x",
          });
        }
      }
    }

    layoutEdgeLabels(edgeItems.left, viewportHeight);
    layoutEdgeLabels(edgeItems.right, viewportHeight);
    layoutEdgeLabels(edgeItems.top, viewportWidth);
    layoutEdgeLabels(edgeItems.bottom, viewportWidth);

    for (const edge of ["left", "right", "top", "bottom"]) {
      for (const item of edgeItems[edge]) {
        const center = item.axis === "y"
          ? { x: item.fixed, y: item.position }
          : { x: item.position, y: item.fixed };
        App.el.labelLayer.appendChild(createSVG("text", {
          x: center.x,
          y: center.y,
          fill: "#1f2937",
          "font-size": fontSize,
          "font-family": "IBM Plex Mono, monospace",
          "text-anchor": "middle",
          "dominant-baseline": "middle",
          "paint-order": "stroke",
          stroke: "#ffffff",
          "stroke-width": 3,
          "stroke-linejoin": "round",
          "vector-effect": "non-scaling-stroke",
        })).textContent = item.text;
      }
    }
  }

  function drawIntersections() {
    const r = 2.2;
    const sm = App.buildScreenMatrix();
    for (const p of App.state.intersections) {
      const sp = App.applyMat(sm, p);
      App.el.pointLayer.appendChild(createSVG("circle", {
        cx: sp.x,
        cy: sp.y,
        r,
        fill: "#d92828",
      }));
    }
  }

  function buildColorSideStats() {
    const black = new Map();
    const white = new Map();
    for (const c of App.state.cells) {
      const sides = Number.isFinite(c.sideCount) ? c.sideCount : (c.poly.length + (c.external ? 1 : 0));
      const bucket = isBlack(c) ? black : white;
      bucket.set(sides, (bucket.get(sides) || 0) + 1);
    }
    return { black, white };
  }

  function formatColorSideStats(prefix, map) {
    const keys = Array.from(map.keys()).sort((a, b) => a - b);
    if (!keys.length) return `${prefix}-`;
    return keys.map(k => `${prefix}${k}=${map.get(k)}`).join(" ");
  }

  function cellSideCount(c) {
    const v = Number.isFinite(c.sideCount) ? c.sideCount : (c.poly.length + (c.external ? 1 : 0));
    return Number.isFinite(v) ? Math.max(0, Math.floor(v)) : null;
  }

  function buildPolySideMap(cells) {
    const map = new Map();
    let maxSides = 0;
    for (const c of cells) {
      const sides = cellSideCount(c);
      if (!sides || sides < 3) continue;
      maxSides = Math.max(maxSides, sides);
      map.set(sides, (map.get(sides) || 0) + 1);
    }
    return { map, maxSides };
  }

  function buildPolySideMapsByColor(cells) {
    const black = new Map();
    const white = new Map();
    let maxSides = 0;
    for (const c of cells) {
      const sides = cellSideCount(c);
      if (!sides || sides < 3) continue;
      maxSides = Math.max(maxSides, sides);
      const bucket = isBlack(c) ? black : white;
      bucket.set(sides, (bucket.get(sides) || 0) + 1);
    }
    return { black, white, maxSides };
  }

  function sumMapValues(map) {
    let s = 0;
    for (const v of map.values()) s += v;
    return s;
  }

  function renderPolyStatsTable(opts) {
    const head = App.el.polyStatsHeadEl;
    const body = App.el.polyStatsBodyEl;
    if (!head || !body) return;

    const maxSides = Math.max(3, opts.maxSides || 3);

    head.textContent = "";
    const addHeadCell = (text) => {
      const th = document.createElement("th");
      th.textContent = text;
      head.appendChild(th);
    };
    const th = document.createElement("th");
    th.textContent = "Polygons";
    th.colSpan = 2;
    head.appendChild(th);
    for (let k = 3; k <= maxSides; k++) addHeadCell(String(k));

    body.textContent = "";
    for (const row of opts.rows) {
      const tr = document.createElement("tr");
      if (row.blackRow) tr.classList.add("black-row");

      const th = document.createElement("th");
      th.scope = "row";
      th.textContent = row.label || "";
      tr.appendChild(th);

      const addCell = (val) => {
        const td = document.createElement("td");
        td.textContent = String(val);
        tr.appendChild(td);
      };

      addCell(row.total);
      for (let k = 3; k <= maxSides; k++) addCell(row.map.get(k) || 0);
      body.appendChild(tr);
    }
  }

  function buildEdgeQualityStats() {
    const lines = App.state.lines;
    const n = lines.length;
    if (n < 2) return null;
    const g = App.buildGeoMatrix();
    let minLen = Infinity;
    let maxLen = 0;
    let segCount = 0;

    for (let i = 0; i < n; i++) {
      const pts = [];
      for (let j = 0; j < n; j++) {
        if (j === i) continue;
        const p = App.intersectLines(lines[i], lines[j]);
        if (!p) continue;
        pts.push(App.applyMat(g, p));
      }
      pts.sort((a, b) => a.x !== b.x ? a.x - b.x : a.y - b.y);
      for (let k = 1; k < pts.length; k++) {
        const len = Math.hypot(pts[k].x - pts[k - 1].x, pts[k].y - pts[k - 1].y);
        if (!Number.isFinite(len) || len <= App.EPS) continue;
        minLen = Math.min(minLen, len);
        maxLen = Math.max(maxLen, len);
        segCount++;
      }
    }

    if (!segCount || !Number.isFinite(minLen) || minLen <= App.EPS) return null;
    return { minLen, maxLen, ratio: maxLen / minLen };
  }

  App.computeEdgeQuality = function computeEdgeQuality() {
    return buildEdgeQualityStats();
  };

  App.render = function render() {
    const width = App.el.viewer.clientWidth;
    const height = App.el.viewer.clientHeight;
    App.el.clipRect.setAttribute("width", String(width));
    App.el.clipRect.setAttribute("height", String(height));

    const m = App.buildScreenMatrix();
    App.el.sceneGroup.setAttribute("transform", `matrix(${m[0]} ${m[1]} ${m[2]} ${m[3]} ${m[4]} ${m[5]})`);

    App.el.polyLayer.innerHTML = "";
    App.el.lineLayer.innerHTML = "";
    App.el.labelLayer.innerHTML = "";
    App.el.pointLayer.innerHTML = "";

    if (!App.state.bbox) return;
    App.state.visibleClipPoly = App.getViewportWorldPolygon();

    if (App.state.flags.checkerMode) drawCells();
    if (!App.state.flags.checkerMode) drawLines();
    if (App.state.flags.showLineNumbers) drawLineNumbers();
    if (App.state.flags.showPoints) drawIntersections();

    chooseColorOrientation();
    const includedCells = App.state.flags.showOuter
      ? App.state.cells
      : App.state.cells.filter(c => !c.external);
    const totalCells = includedCells.length;
    const extCount = includedCells.filter(c => c.external).length;
    const innerCount = totalCells - extCount;
    const edgeQuality = App.computeEdgeQuality();
    const g = App.buildGeoMatrix();
    App.setStatusBase(`Lines: ${App.state.lines.length}, Points: ${App.state.intersections.length}`);

    App.setGeoInfo(
`L=${edgeQuality ? edgeQuality.maxLen.toFixed(3) : "n/a"} S=${edgeQuality ? edgeQuality.minLen.toFixed(4) : "n/a"} G=${edgeQuality ? edgeQuality.ratio.toFixed(3) : "n/a"}
geo=[${g.map(v => v.toFixed(2)).join(",")}]`
    );

    if (App.state.flags.checkerMode) {
      const maps = buildPolySideMapsByColor(includedCells);
      const blackTotal = sumMapValues(maps.black);
      const whiteTotal = sumMapValues(maps.white);
      renderPolyStatsTable({
        maxSides: maps.maxSides,
        rows: [
          { label: "Black", map: maps.black, total: blackTotal, blackRow: true },
          { label: "White", map: maps.white, total: whiteTotal, blackRow: false },
        ],
      });
    } else {
      const all = buildPolySideMap(includedCells);
      renderPolyStatsTable({
        maxSides: all.maxSides,
        rows: [
          { label: "Total", map: all.map, total: sumMapValues(all.map), blackRow: false },
        ],
      });
    }

    App.setViewInfo(
`zoom=${App.state.view.zoom.toFixed(4)}
x=${App.state.view.panX.toFixed(1)} y=${App.state.view.panY.toFixed(1)}`
    );
  };
})();

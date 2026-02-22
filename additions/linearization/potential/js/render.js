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

  function drawCells() {
    const clipPoly = App.state.visibleClipPoly;
    if (!clipPoly) return;
    chooseColorOrientation();
    const coloredMode = !!App.state.flags.coloredMode;
    const debugRows = [];
    let cellIndex = 0;
    for (const c of App.state.cells) {
      if (!App.state.flags.showOuter && c.external) continue;
      const black = isBlack(c);
      if (!black && !coloredMode) continue;
      const clipped = App.clipPolygonByConvex(c.poly, clipPoly);
      if (clipped.length < 3) continue;
      const sideCount = Number.isFinite(c.sideCount) ? c.sideCount : (c.poly.length + (c.external ? 1 : 0));
      const fill = black ? "#000000" : sideColorByCount(sideCount);
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
      App.el.polyLayer.appendChild(createSVG("polygon", {
        points,
        fill,
        "fill-opacity": 1.0,
        stroke: "none",
      }));
      cellIndex++;
    }
    logColoredCellsDebug(debugRows);
  }

  function drawLines() {
    const clipPoly = App.state.visibleClipPoly;
    if (!clipPoly) return;
    const w = App.state.flags.checkerMode ? 1.2 : 0.4;
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
    App.el.pointLayer.innerHTML = "";

    if (!App.state.bbox) return;
    App.state.visibleClipPoly = App.getViewportWorldPolygon();

    if (App.state.flags.checkerMode) drawCells();
    if (!App.state.flags.checkerMode) drawLines();
    if (App.state.flags.showPoints) drawIntersections();

    chooseColorOrientation();
    const totalCells = App.state.cells.length;
    const extCount = App.state.cells.filter(c => c.external).length;
    const innerCount = totalCells - extCount;
    const sideStats = buildColorSideStats();
    const blackSideText = formatColorSideStats("b", sideStats.black);
    const whiteSideText = formatColorSideStats("w", sideStats.white);
    const edgeQuality = App.computeEdgeQuality();
    const g = App.buildGeoMatrix();
    App.setStatus(
`n=${App.state.lines.length} points=${App.state.intersections.length} cells=${innerCount}+${extCount}
${blackSideText}
${whiteSideText}
L=${edgeQuality ? edgeQuality.maxLen.toFixed(3) : "n/a"} S=${edgeQuality ? edgeQuality.minLen.toFixed(4) : "n/a"} G=${edgeQuality ? edgeQuality.ratio.toFixed(3) : "n/a"}
zoom=${App.state.view.zoom.toFixed(4)} pan=(${App.state.view.panX.toFixed(1)},${App.state.view.panY.toFixed(1)})
geo=[${g.map(v => v.toFixed(2)).join(",")}]`
    );
  };
})();

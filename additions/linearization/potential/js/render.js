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

  function drawCells() {
    const clipPoly = App.state.visibleClipPoly;
    if (!clipPoly) return;
    chooseColorOrientation();
    for (const c of App.state.cells) {
      if (!App.state.flags.showOuter && c.external) continue;
      if (!isBlack(c)) continue;
      const clipped = App.clipPolygonByConvex(c.poly, clipPoly);
      if (clipped.length < 3) continue;
      const points = clipped.map(p => `${p.x},${p.y}`).join(" ");
      App.el.polyLayer.appendChild(createSVG("polygon", {
        points,
        fill: "#111111",
        "fill-opacity": c.external ? 0.9 : 1.0,
        stroke: "none",
      }));
    }
  }

  function drawLines() {
    const clipPoly = App.state.visibleClipPoly;
    if (!clipPoly) return;
    const w = 1.2 / App.state.view.zoom;
    const lineColor = App.state.flags.checkerMode ? "#6b6254" : "#111111";
    for (const line of App.state.lines) {
      const seg = App.lineSegmentInPoly(line, clipPoly);
      if (!seg) continue;
      App.el.lineLayer.appendChild(createSVG("line", {
        x1: seg[0].x, y1: seg[0].y,
        x2: seg[1].x, y2: seg[1].y,
        stroke: lineColor,
        "stroke-width": w,
        "stroke-opacity": App.state.flags.checkerMode ? 0.45 : 0.95,
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
    if (!keys.length) return `${prefix}none`;
    return keys.map(k => `${prefix}${k}=${map.get(k)}`).join(" ");
  }

  App.render = function render() {
    const width = App.el.viewer.clientWidth;
    const height = App.el.viewer.clientHeight;
    App.el.clipRect.setAttribute("width", String(width));
    App.el.clipRect.setAttribute("height", String(height));

    const m = App.buildScreenMatrix();
    App.el.sceneGroup.setAttribute("transform", `matrix(${m[0]} ${m[1]} ${m[2]} ${m[3]} ${m[4]} ${m[5]})`);

    App.el.polyLayer.innerHTML = "";
    App.el.lineLayer.innerHTML = "";

    if (!App.state.bbox) return;
    App.state.visibleClipPoly = App.getViewportWorldPolygon();

    if (App.state.flags.checkerMode) drawCells();
    if (!App.state.flags.checkerMode) drawLines();

    chooseColorOrientation();
    const blackTotal = App.state.cells.filter(c => isBlack(c)).length;
    const blackVisible = App.state.cells.filter(c => isBlack(c) && (App.state.flags.showOuter || !c.external)).length;
    const extCount = App.state.cells.filter(c => c.external).length;
    const sideStats = buildColorSideStats();
    const blackSideText = formatColorSideStats("b", sideStats.black);
    const whiteSideText = formatColorSideStats("w", sideStats.white);
    const g = App.buildGeoMatrix();
    App.setStatus(
`lines=${App.state.lines.length}
intersections=${App.state.intersections.length}
cells=${App.state.cells.length}
black(total/visible)=${blackTotal}/${blackVisible}
external cells=${extCount}
${blackSideText}
${whiteSideText}
zoom=${App.state.view.zoom.toFixed(4)} pan=(${App.state.view.panX.toFixed(1)}, ${App.state.view.panY.toFixed(1)})
geo=[${g.map(v => v.toFixed(3)).join(", ")}]`
    );
  };
})();

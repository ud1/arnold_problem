(function () {
  const App = window.App;

  function bboxOfPoints(points) {
    let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
    for (const p of points) {
      minX = Math.min(minX, p.x);
      maxX = Math.max(maxX, p.x);
      minY = Math.min(minY, p.y);
      maxY = Math.max(maxY, p.y);
    }
    return { minX, maxX, minY, maxY };
  }

  function rebuildCellsForSceneBase() {
    App.state.cells = App.buildCells(App.state.lines, App.state.sceneBasePoly);
  }

  function ensureSceneCoverage() {
    if (!App.state.sceneBasePoly || App.state.sceneBasePoly.length < 3) return;
    if (!App.state.lines.length) return;
    const vp = App.getViewportWorldPolygon();
    if (!vp || vp.length < 3) return;

    const base = bboxOfPoints(App.state.sceneBasePoly);
    const vb = bboxOfPoints(vp);
    const spanX = Math.max(1e-6, base.maxX - base.minX);
    const spanY = Math.max(1e-6, base.maxY - base.minY);
    const guardX = spanX * 0.18;
    const guardY = spanY * 0.18;

    const nearEdge =
      vb.minX < base.minX + guardX ||
      vb.maxX > base.maxX - guardX ||
      vb.minY < base.minY + guardY ||
      vb.maxY > base.maxY - guardY;
    if (!nearEdge) return;

    const reqMinX = Math.min(base.minX, vb.minX);
    const reqMaxX = Math.max(base.maxX, vb.maxX);
    const reqMinY = Math.min(base.minY, vb.minY);
    const reqMaxY = Math.max(base.maxY, vb.maxY);
    const reqSpan = Math.max(reqMaxX - reqMinX, reqMaxY - reqMinY, 1);
    const pad = Math.max(10, reqSpan * 0.8);

    App.state.sceneBasePoly = [
      { x: reqMinX - pad, y: reqMinY - pad },
      { x: reqMaxX + pad, y: reqMinY - pad },
      { x: reqMaxX + pad, y: reqMaxY + pad },
      { x: reqMinX - pad, y: reqMaxY + pad },
    ];
    rebuildCellsForSceneBase();
  }

  function renderScene() {
    ensureSceneCoverage();
    App.render();
  }

  function buildFitTargetsWorld() {
    if (App.state.intersections.length > 0) return App.state.intersections;
    if (App.state.bbox) {
      const bb = App.state.bbox;
      return [
        { x: bb.minX, y: bb.minY },
        { x: bb.maxX, y: bb.minY },
        { x: bb.maxX, y: bb.maxY },
        { x: bb.minX, y: bb.maxY },
      ];
    }
    return [{ x: 0, y: 0 }];
  }

  function fitView() {
    if (!App.state.bbox) return;
    const targets = buildFitTargetsWorld();
    const mg = App.buildGeoMatrix();
    const c2 = targets.map(p => App.applyMat(mg, p));
    const b = bboxOfPoints(c2);
    let minX = b.minX, maxX = b.maxX, minY = b.minY, maxY = b.maxY;

    if (targets.length === 1) {
      const eps = 1;
      minX -= eps; maxX += eps; minY -= eps; maxY += eps;
    }

    const spanX = Math.max(1e-6, maxX - minX);
    const spanY = Math.max(1e-6, maxY - minY);
    const pad = 0.06;
    const vw = App.el.viewer.clientWidth;
    const vh = App.el.viewer.clientHeight;
    const zoom = Math.min(vw * (1 - pad * 2) / spanX, vh * (1 - pad * 2) / spanY);
    App.state.view.zoom = Math.max(1e-4, zoom);
    const cx = (minX + maxX) * 0.5;
    const cy = (minY + maxY) * 0.5;
    App.state.view.panX = -cx * App.state.view.zoom;
    App.state.view.panY = cy * App.state.view.zoom;
  }

  function resetTransform() {
    App.state.geoMatrix = App.matIdentity();
  }

  function resetView() {
    App.state.view = { panX: 0, panY: 0, zoom: 1 };
  }

  function screenAnchored(op) {
    const cx = App.el.viewer.clientWidth * 0.5;
    const cy = App.el.viewer.clientHeight * 0.5;
    return App.matMul(
      App.matTranslate(cx, cy),
      App.matMul(op, App.matTranslate(-cx, -cy))
    );
  }

  function applyScreenCommand(op) {
    const delta = screenAnchored(op);
    if (!App.applyGeoDeltaInScreen(delta)) return;
    renderScene();
  }

  function applyInputAndRebuild(fitAfter) {
    const text = document.getElementById("inputLog").value;
    const lines = App.parseInput(text);
    App.state.lines = lines;
    App.state.intersections = App.computeIntersections(lines);
    App.state.bbox = App.buildBBox(lines, App.state.intersections);
    const bb = App.state.bbox;
    const span = Math.max(bb.maxX - bb.minX, bb.maxY - bb.minY);
    const pad = Math.max(10, span * 120.0);
    App.state.sceneBasePoly = [
      { x: bb.minX - pad, y: bb.minY - pad },
      { x: bb.maxX + pad, y: bb.minY - pad },
      { x: bb.maxX + pad, y: bb.maxY + pad },
      { x: bb.minX - pad, y: bb.maxY + pad },
    ];
    App.state.pivot = {
      x: 0.5 * (App.state.bbox.minX + App.state.bbox.maxX),
      y: 0.5 * (App.state.bbox.minY + App.state.bbox.maxY),
    };
    rebuildCellsForSceneBase();
    if (fitAfter) fitView();
    renderScene();
  }

  function exportSvg() {
    const clone = App.el.svgRoot.cloneNode(true);
    clone.setAttribute("xmlns", "http://www.w3.org/2000/svg");
    clone.setAttribute("width", String(App.el.viewer.clientWidth));
    clone.setAttribute("height", String(App.el.viewer.clientHeight));
    clone.setAttribute("viewBox", `0 0 ${App.el.viewer.clientWidth} ${App.el.viewer.clientHeight}`);
    const xml = new XMLSerializer().serializeToString(clone);
    const blob = new Blob([xml], { type: "image/svg+xml;charset=utf-8" });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = "line_view.svg";
    a.click();
    URL.revokeObjectURL(url);
  }

  function wireTransformButtons() {
    const rotStep = 10 * Math.PI / 180;
    const skewStep = 6 * Math.PI / 180;

    document.getElementById("rotLeftBtn").addEventListener("click", () => {
      applyScreenCommand(App.matRotate(-rotStep));
    });
    document.getElementById("rotRightBtn").addEventListener("click", () => {
      applyScreenCommand(App.matRotate(rotStep));
    });

    document.getElementById("stretchXDownBtn").addEventListener("click", () => {
      applyScreenCommand(App.matScale(0.9, 1));
    });
    document.getElementById("stretchXUpBtn").addEventListener("click", () => {
      applyScreenCommand(App.matScale(1.1, 1));
    });
    document.getElementById("stretchYDownBtn").addEventListener("click", () => {
      applyScreenCommand(App.matScale(1, 0.9));
    });
    document.getElementById("stretchYUpBtn").addEventListener("click", () => {
      applyScreenCommand(App.matScale(1, 1.1));
    });

    document.getElementById("skewXLeftBtn").addEventListener("click", () => {
      applyScreenCommand(App.matSkew(-skewStep, 0));
    });
    document.getElementById("skewXRightBtn").addEventListener("click", () => {
      applyScreenCommand(App.matSkew(skewStep, 0));
    });
    document.getElementById("skewYDownBtn").addEventListener("click", () => {
      applyScreenCommand(App.matSkew(0, -skewStep));
    });
    document.getElementById("skewYUpBtn").addEventListener("click", () => {
      applyScreenCommand(App.matSkew(0, skewStep));
    });

    document.getElementById("mirrorXBtn").addEventListener("click", () => {
      applyScreenCommand(App.matScale(-1, 1));
    });
    document.getElementById("mirrorYBtn").addEventListener("click", () => {
      applyScreenCommand(App.matScale(1, -1));
    });
  }

  App.wireControls = function wireControls() {
    document.getElementById("parseBtn").addEventListener("click", () => applyInputAndRebuild(true));
    document.getElementById("fitBtn").addEventListener("click", () => { fitView(); renderScene(); });
    document.getElementById("resetTransformBtn").addEventListener("click", () => { resetTransform(); renderScene(); });
    document.getElementById("resetViewBtn").addEventListener("click", () => { resetView(); renderScene(); });
    document.getElementById("exportBtn").addEventListener("click", exportSvg);

    ["checkerMode", "showOuter"].forEach(id => {
      document.getElementById(id).addEventListener("change", (e) => {
        App.state.flags[id] = e.target.checked;
        renderScene();
      });
    });

    wireTransformButtons();

    let start = null;
    App.el.svgRoot.addEventListener("pointerdown", (e) => {
      App.el.svgRoot.setPointerCapture(e.pointerId);
      start = { x: e.clientX, y: e.clientY, panX: App.state.view.panX, panY: App.state.view.panY };
    });
    App.el.svgRoot.addEventListener("pointermove", (e) => {
      if (!start) return;
      App.state.view.panX = start.panX + (e.clientX - start.x);
      App.state.view.panY = start.panY + (e.clientY - start.y);
      renderScene();
    });
    App.el.svgRoot.addEventListener("pointerup", () => { start = null; });
    App.el.svgRoot.addEventListener("pointercancel", () => { start = null; });
    App.el.svgRoot.addEventListener("wheel", (e) => {
      e.preventDefault();
      const factor = Math.exp(-e.deltaY * 0.0014);
      const oldZoom = App.state.view.zoom;
      const newZoom = Math.max(1e-4, Math.min(1e4, oldZoom * factor));

      const rect = App.el.viewer.getBoundingClientRect();
      const px = e.clientX - rect.left;
      const py = e.clientY - rect.top;
      const wx = (px - rect.width / 2 - App.state.view.panX) / oldZoom;
      const wy = -(py - rect.height / 2 - App.state.view.panY) / oldZoom;

      App.state.view.zoom = newZoom;
      App.state.view.panX = px - rect.width / 2 - wx * newZoom;
      App.state.view.panY = py - rect.height / 2 + wy * newZoom;
      renderScene();
    }, { passive: false });

    window.addEventListener("resize", () => renderScene());
  };

  App.preload = function preload() {
    document.getElementById("inputLog").value = `#LINES_BEGIN\nm,b\n-28.3,-15.8\n-13.6,2.18\n-10.3,-18.7\n-10.1,5.76\n-8.77,3.62\n-8.49,8.42\n-6.28,1.04\n-6.28,9.98\n-2.97,0.797\n-1.25,5.5\n-1.22,1.62\n-0.452,5.88\n-0.452,2.41\n-0.151,3.91\n-0.142,-6.76\n2.4,8.41\n5.38,-0.629\n8.03,2.48\n8.17,-0.474\n14.4,9.54\n15.8,-12.1\n16.5,5.97\n16.6,-23\n#LINES_END`;
    resetTransform();
    applyInputAndRebuild(true);
  };
})();

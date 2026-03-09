(function () {
  const App = window.App;
  const LS_KEY = "lineViewerState.v1";
  const PERSIST_DELAY_MS = 120;
  let persistTimer = null;

  function safeJsonParse(text) {
    try {
      return JSON.parse(text);
    } catch (_err) {
      return null;
    }
  }

  function loadPersistedState() {
    if (!window.localStorage) return null;
    const raw = localStorage.getItem(LS_KEY);
    if (!raw) return null;
    const parsed = safeJsonParse(raw);
    if (!parsed || typeof parsed !== "object") return null;
    return parsed;
  }

  function sanitizeArray6(arr, fallback) {
    if (!Array.isArray(arr) || arr.length !== 6) return fallback;
    const nums = arr.map(Number);
    if (nums.some(v => !Number.isFinite(v))) return fallback;
    return nums;
  }

  function sanitizeView(v, fallback) {
    if (!v || typeof v !== "object") return fallback;
    const panX = Number(v.panX);
    const panY = Number(v.panY);
    const zoom = Number(v.zoom);
    if (!Number.isFinite(panX) || !Number.isFinite(panY) || !Number.isFinite(zoom)) return fallback;
    return {
      panX,
      panY,
      zoom: Math.max(1e-4, Math.min(1e4, zoom)),
    };
  }

  function capturePersistedState() {
    const inputEl = document.getElementById("inputLog");
    return {
      inputText: inputEl ? inputEl.value : "",
      geoMatrix: App.buildGeoMatrix().slice(),
      view: { ...App.state.view },
      flags: { ...App.state.flags },
    };
  }

  function persistStateNow() {
    if (!window.localStorage) return;
    const payload = capturePersistedState();
    try {
      localStorage.setItem(LS_KEY, JSON.stringify(payload));
    } catch (_err) {
      // Ignore storage quota / privacy mode errors.
    }
  }

  function schedulePersist() {
    if (persistTimer !== null) clearTimeout(persistTimer);
    persistTimer = setTimeout(() => {
      persistTimer = null;
      persistStateNow();
    }, PERSIST_DELAY_MS);
  }

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
    schedulePersist();
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

  function clamp(v, lo, hi) {
    return Math.max(lo, Math.min(hi, v));
  }

  function buildGeoFromScreenDelta(deltaScreen, baseGeo) {
    const view = App.buildViewMatrix();
    const invView = App.matInverse(view);
    if (!invView) return null;
    const deltaGeo = App.matMul(App.matMul(invView, deltaScreen), view);
    const next = App.matMul(deltaGeo, baseGeo);
    const det = next[0] * next[3] - next[1] * next[2];
    if (!Number.isFinite(det) || Math.abs(det) < 1e-10) return null;
    return next;
  }

  function linearQuality2x2(m) {
    const a = m[0], b = m[1], c = m[2], d = m[3];
    const s11 = a * a + b * b;
    const s12 = a * c + b * d;
    const s22 = c * c + d * d;
    const tr = s11 + s22;
    const disc = Math.sqrt(Math.max(0, 0.25 * tr * tr - (s11 * s22 - s12 * s12)));
    const l1 = Math.max(0, 0.5 * tr + disc);
    const l2 = Math.max(0, 0.5 * tr - disc);
    const maxSv = Math.sqrt(l1);
    const minSv = Math.sqrt(l2);
    if (!Number.isFinite(maxSv) || !Number.isFinite(minSv)) return null;
    const cond = minSv > 1e-12 ? maxSv / minSv : Infinity;
    return { minSv, maxSv, cond };
  }

  function isReasonableLinearPart(m) {
    const q = linearQuality2x2(m);
    if (!q) return false;
    if (q.minSv < 0.02) return false;
    if (q.maxSv > 25) return false;
    if (q.cond > 450) return false;
    return true;
  }

  function evaluateQualityForGeoMatrix(candidateGeo) {
    if (!candidateGeo || !isReasonableLinearPart(candidateGeo)) return Infinity;
    const prev = App.state.geoMatrix;
    App.state.geoMatrix = candidateGeo;
    const edgeQuality = App.computeEdgeQuality ? App.computeEdgeQuality() : null;
    App.state.geoMatrix = prev;
    if (!edgeQuality || !Number.isFinite(edgeQuality.ratio)) return Infinity;
    return edgeQuality.ratio;
  }

  function buildAutoCandidate(baseGeo, params) {
    const op = App.matMul(
      App.matScale(params.sx, params.sy),
      App.matSkew(params.kx, params.ky)
    );
    const deltaScreen = screenAnchored(op);
    const geo = buildGeoFromScreenDelta(deltaScreen, baseGeo);
    if (!geo) return null;
    return { geo, score: evaluateQualityForGeoMatrix(geo) };
  }

  function refineAutoParams(baseGeo, startParams, bounds) {
    let best = buildAutoCandidate(baseGeo, startParams);
    if (!best || !Number.isFinite(best.score)) return null;
    best.params = { ...startParams };
    let steps = { sx: 0.26, sy: 0.26, kx: 8 * Math.PI / 180, ky: 8 * Math.PI / 180 };
    const keys = ["sx", "sy", "kx", "ky"];

    for (let iter = 0; iter < 26; iter++) {
      let improved = false;
      for (const key of keys) {
        let keyBest = null;
        for (const dir of [-1, 1]) {
          const candidateParams = { ...best.params };
          const nextValue = clamp(candidateParams[key] + dir * steps[key], bounds[key][0], bounds[key][1]);
          if (Math.abs(nextValue - candidateParams[key]) < 1e-9) continue;
          candidateParams[key] = nextValue;
          const candidate = buildAutoCandidate(baseGeo, candidateParams);
          if (!candidate || !Number.isFinite(candidate.score)) continue;
          candidate.params = candidateParams;
          if (candidate.score + 1e-9 < best.score && (!keyBest || candidate.score < keyBest.score)) {
            keyBest = candidate;
          }
        }
        if (keyBest) {
          best = keyBest;
          improved = true;
        }
      }
      if (!improved) {
        steps = {
          sx: steps.sx * 0.56,
          sy: steps.sy * 0.56,
          kx: steps.kx * 0.56,
          ky: steps.ky * 0.56,
        };
      }
      if (steps.sx < 0.006 && steps.sy < 0.006 && steps.kx < 0.0018 && steps.ky < 0.0018) break;
    }
    return best;
  }

  function optimizeTransformAutomatically() {
    if (!App.state.lines || App.state.lines.length < 2) {
      App.setStatus("Нужно как минимум 2 линии для автооптимизации.");
      return;
    }
    if (typeof App.computeEdgeQuality !== "function") {
      App.setStatus("Метрика качества недоступна.");
      return;
    }

    const autoBtn = document.getElementById("autoTransformBtn");
    const prevLabel = autoBtn.textContent;
    autoBtn.disabled = true;
    autoBtn.textContent = "Auto...";

    const baseGeo = App.buildGeoMatrix().slice();
    const bounds = {
      sx: [0.05, 20],
      sy: [0.05, 20],
      kx: [-1.35, 1.35],
      ky: [-1.35, 1.35],
    };
    const seeds = [
      { sx: 1, sy: 1, kx: 0, ky: 0 },
      { sx: 0.85, sy: 1, kx: 0, ky: 0 },
      { sx: 1.18, sy: 1, kx: 0, ky: 0 },
      { sx: 1, sy: 0.85, kx: 0, ky: 0 },
      { sx: 1, sy: 1.18, kx: 0, ky: 0 },
      { sx: 1, sy: 1, kx: -0.17, ky: 0 },
      { sx: 1, sy: 1, kx: 0.17, ky: 0 },
      { sx: 1, sy: 1, kx: 0, ky: -0.17 },
      { sx: 1, sy: 1, kx: 0, ky: 0.17 },
    ];

    const beforeScore = evaluateQualityForGeoMatrix(baseGeo);
    let best = null;
    for (const seed of seeds) {
      const refined = refineAutoParams(baseGeo, seed, bounds);
      if (!refined || !Number.isFinite(refined.score)) continue;
      if (!best || refined.score < best.score) best = refined;
    }

    let improved = false;
    if (best && Number.isFinite(best.score) && best.score + 1e-9 < beforeScore) {
      App.state.geoMatrix = best.geo;
      improved = true;
    }
    renderScene();

    const fmt = (x) => (Number.isFinite(x) ? x.toFixed(3) : "n/a");
    const note = improved
      ? `Auto: G ${fmt(beforeScore)} -> ${fmt(best.score)}`
      : "Auto: улучшений не найдено в заданных пределах";
    App.setStatus(`${App.el.statusEl.textContent}\n${note}`);

    autoBtn.disabled = false;
    autoBtn.textContent = prevLabel;
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

  async function pasteFromClipboardToInput() {
    const input = document.getElementById("inputLog");
    if (!navigator.clipboard || !navigator.clipboard.readText) {
      App.setStatus("Clipboard API недоступен в этом браузере.");
      return;
    }
    try {
      const text = await navigator.clipboard.readText();
      input.value = text;
      input.focus();
    } catch (_err) {
      App.setStatus("Не удалось прочитать буфер обмена.");
    }
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

  function syncColoredModeAvailability() {
    const checker = document.getElementById("checkerMode");
    const colored = document.getElementById("coloredMode");
    if (!checker || !colored) return;
    const enabled = checker.checked;
    colored.disabled = !enabled;
    if (!enabled && colored.checked) {
      colored.checked = false;
      App.state.flags.coloredMode = false;
    }
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
    document.getElementById("inputLog").addEventListener("input", () => {
      schedulePersist();
    });
    document.getElementById("pasteBtn").addEventListener("click", () => { pasteFromClipboardToInput(); });
    document.getElementById("parseBtn").addEventListener("click", () => applyInputAndRebuild(true));
    document.getElementById("fitBtn").addEventListener("click", () => { fitView(); renderScene(); });
    document.getElementById("autoTransformBtn").addEventListener("click", optimizeTransformAutomatically);
    document.getElementById("resetTransformBtn").addEventListener("click", () => { resetTransform(); renderScene(); });
    document.getElementById("resetViewBtn").addEventListener("click", () => { resetView(); renderScene(); });
    document.getElementById("exportBtn").addEventListener("click", exportSvg);

    ["checkerMode", "coloredMode", "highlightDefects", "showOuter", "showPoints"].forEach(id => {
      document.getElementById(id).addEventListener("change", (e) => {
        App.state.flags[id] = e.target.checked;
        if (id === "checkerMode") syncColoredModeAvailability();
        renderScene();
      });
    });
    syncColoredModeAvailability();

    wireTransformButtons();

    document.addEventListener("paste", (e) => {
      const text = e.clipboardData && e.clipboardData.getData("text");
      if (typeof text !== "string") return;
      e.preventDefault();
      const input = document.getElementById("inputLog");
      input.value = text;
      input.focus();
      schedulePersist();
    });

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
    const defaultInput = `#LINES_BEGIN\nm,b\n-28.3,-15.8\n-13.6,2.18\n-10.3,-18.7\n-10.1,5.76\n-8.77,3.62\n-8.49,8.42\n-6.28,1.04\n-6.28,9.98\n-2.97,0.797\n-1.25,5.5\n-1.22,1.62\n-0.452,5.88\n-0.452,2.41\n-0.151,3.91\n-0.142,-6.76\n2.4,8.41\n5.38,-0.629\n8.03,2.48\n8.17,-0.474\n14.4,9.54\n15.8,-12.1\n16.5,5.97\n16.6,-23\n#LINES_END`;
    const persisted = loadPersistedState();
    const input = document.getElementById("inputLog");
    input.value = (persisted && typeof persisted.inputText === "string")
      ? persisted.inputText
      : defaultInput;

    App.state.geoMatrix = sanitizeArray6(persisted && persisted.geoMatrix, App.matIdentity());
    App.state.view = sanitizeView(persisted && persisted.view, { panX: 0, panY: 0, zoom: 1 });

    if (persisted && persisted.flags && typeof persisted.flags === "object") {
      for (const id of ["checkerMode", "coloredMode", "highlightDefects", "showOuter", "showPoints"]) {
        if (typeof persisted.flags[id] === "boolean") {
          App.state.flags[id] = persisted.flags[id];
          const el = document.getElementById(id);
          if (el) el.checked = persisted.flags[id];
        }
      }
    }
    syncColoredModeAvailability();

    applyInputAndRebuild(false);
    schedulePersist();
  };
})();

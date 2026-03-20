(function () {
  const App = (window.App = window.App || {});

  App.state = {
    lines: [],
    intersections: [],
    bbox: null,
    sceneBasePoly: null,
    visibleClipPoly: null,
    cells: [],
    view: { panX: 0, panY: 0, zoom: 1 },
    geoMatrix: [1, 0, 0, 1, 0, 0],
    flags: {
      checkerMode: true,
      coloredMode: false,
      highlightDefects: false,
      showOuter: true,
      showPoints: false,
      showLineNumbers: false,
    },
    pivot: { x: 0, y: 0 },
    _invertColor: false,
  };

  App.EPS = 1e-9;

  App.el = {
    svgRoot: document.getElementById("svgRoot"),
    viewer: document.getElementById("viewer"),
    sceneGroup: document.getElementById("sceneGroup"),
    polyLayer: document.getElementById("polyLayer"),
    lineLayer: document.getElementById("lineLayer"),
    labelLayer: document.getElementById("labelLayer"),
    pointLayer: document.getElementById("pointLayer"),
    clipRect: document.getElementById("clipRect"),
    viewInfoEl: document.getElementById("viewInfo"),
    polyStatsHeadEl: document.getElementById("polyStatsHead"),
    polyStatsBodyEl: document.getElementById("polyStatsBody"),
    geoInfoEl: document.getElementById("geoInfo"),
    statusEl: document.getElementById("status"),
  };

  App.setViewInfo = function setViewInfo(text) {
    if (!App.el.viewInfoEl) return;
    App.el.viewInfoEl.textContent = text;
  };

  App.setGeoInfo = function setGeoInfo(text) {
    if (!App.el.geoInfoEl) return;
    App.el.geoInfoEl.textContent = text;
  };

  function refreshStatus() {
    if (!App.el.statusEl) return;
    const base = typeof App.state._statusBase === "string" ? App.state._statusBase : "";
    const notes = Array.isArray(App.state._statusNotes) ? App.state._statusNotes : [];
    const text = notes.length ? `${base}\n${notes.join("\n")}` : base;
    App.el.statusEl.textContent = text;
  }

  App.setStatusBase = function setStatusBase(text) {
    App.state._statusBase = String(text || "");
    refreshStatus();
  };

  App.appendStatus = function appendStatus(note) {
    if (!App.el.statusEl) return;
    if (!Array.isArray(App.state._statusNotes)) App.state._statusNotes = [];
    App.state._statusNotes.push(String(note || ""));
    refreshStatus();
  };

  App.setStatus = function setStatus(text) {
    App.setStatusBase(text);
  };
})();

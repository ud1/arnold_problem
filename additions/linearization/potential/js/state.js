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
      showOuter: true,
      showPoints: false,
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
    pointLayer: document.getElementById("pointLayer"),
    clipRect: document.getElementById("clipRect"),
    statusEl: document.getElementById("status"),
  };

  App.setStatus = function setStatus(text) {
    App.el.statusEl.textContent = text;
  };
})();

(function () {
  const App = window.App;

  function matIdentity() { return [1, 0, 0, 1, 0, 0]; }
  function matMul(a, b) {
    return [
      a[0] * b[0] + a[2] * b[1],
      a[1] * b[0] + a[3] * b[1],
      a[0] * b[2] + a[2] * b[3],
      a[1] * b[2] + a[3] * b[3],
      a[0] * b[4] + a[2] * b[5] + a[4],
      a[1] * b[4] + a[3] * b[5] + a[5],
    ];
  }
  function matTranslate(tx, ty) { return [1, 0, 0, 1, tx, ty]; }
  function matScale(sx, sy) { return [sx, 0, 0, sy, 0, 0]; }
  function matRotate(rad) {
    const c = Math.cos(rad), s = Math.sin(rad);
    return [c, s, -s, c, 0, 0];
  }
  function matSkew(ax, ay) {
    return [1, Math.tan(ay), Math.tan(ax), 1, 0, 0];
  }
  function matInverse(m) {
    const det = m[0] * m[3] - m[1] * m[2];
    if (Math.abs(det) < 1e-14) return null;
    const invDet = 1 / det;
    const a = m[3] * invDet;
    const b = -m[1] * invDet;
    const c = -m[2] * invDet;
    const d = m[0] * invDet;
    const e = -(a * m[4] + c * m[5]);
    const f = -(b * m[4] + d * m[5]);
    return [a, b, c, d, e, f];
  }

  App.matIdentity = matIdentity;
  App.matMul = matMul;
  App.matTranslate = matTranslate;
  App.matScale = matScale;
  App.matRotate = matRotate;
  App.matSkew = matSkew;
  App.matInverse = matInverse;

  App.applyMat = function applyMat(m, p) {
    return { x: m[0] * p.x + m[2] * p.y + m[4], y: m[1] * p.x + m[3] * p.y + m[5] };
  };

  App.buildGeoMatrix = function buildGeoMatrix() {
    return App.state.geoMatrix || matIdentity();
  };

  App.buildViewMatrix = function buildViewMatrix() {
    const vb = App.el.viewer.getBoundingClientRect();
    const cx = vb.width / 2;
    const cy = vb.height / 2;
    const z = App.state.view.zoom;
    return matMul(matTranslate(cx + App.state.view.panX, cy + App.state.view.panY), matScale(z, -z));
  };

  App.buildScreenMatrix = function buildScreenMatrix() {
    return matMul(App.buildViewMatrix(), App.buildGeoMatrix());
  };

  App.applyGeoDeltaInScreen = function applyGeoDeltaInScreen(deltaScreen) {
    const view = App.buildViewMatrix();
    const invView = matInverse(view);
    if (!invView) return false;
    const deltaGeo = matMul(matMul(invView, deltaScreen), view);
    const next = matMul(deltaGeo, App.buildGeoMatrix());
    const det = next[0] * next[3] - next[1] * next[2];
    if (!Number.isFinite(det) || Math.abs(det) < 1e-10) return false;
    App.state.geoMatrix = next;
    return true;
  };

  App.screenToWorld = function screenToWorld(sx, sy) {
    const inv = matInverse(App.buildScreenMatrix());
    if (!inv) return null;
    return App.applyMat(inv, { x: sx, y: sy });
  };

  App.getViewportWorldPolygon = function getViewportWorldPolygon() {
    const w = App.el.viewer.clientWidth;
    const h = App.el.viewer.clientHeight;
    const poly = [
      App.screenToWorld(0, 0),
      App.screenToWorld(w, 0),
      App.screenToWorld(w, h),
      App.screenToWorld(0, h),
    ];
    if (poly.some(p => !p)) return null;
    return poly;
  };
})();

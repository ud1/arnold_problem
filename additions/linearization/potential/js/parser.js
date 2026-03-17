(function () {
  const App = window.App;
  const EPS = App.EPS;

  App.parseInput = function parseInput(text) {
    const blockMatch = text.match(/#LINES_BEGIN([\s\S]*?)#LINES_END/m);
    const source = blockMatch ? blockMatch[1] : text;
    const lines = source.split(/\r?\n/);
    const pairs = [];

    const num = "[+-]?(?:\\d+(?:\\.\\d*)?|\\.\\d+)(?:[eE][+-]?\\d+)?";
    const csvRe = new RegExp(`^\\s*(${num})\\s*,\\s*(${num})\\s*$`);
    const tupleLineRe = new RegExp(`^\\s*\\(\\s*(${num})\\s*,\\s*(${num})\\s*\\)\\s*,?\\s*$`);
    const yRe = new RegExp(`^\\s*y\\s*=\\s*(${num})\\s*\\*?\\s*x\\s*([+-]\\s*(?:\\d+(?:\\.\\d*)?|\\.\\d+)(?:[eE][+-]?\\d+)?)\\s*$`, "i");

    for (const raw of lines) {
      const s = raw.trim();
      if (!s || s.toLowerCase() === "m,b") continue;

      let m = s.match(csvRe);
      if (m) {
        pairs.push({ m: parseFloat(m[1]), b: parseFloat(m[2]) });
        continue;
      }

      m = s.match(tupleLineRe);
      if (m) {
        pairs.push({ m: parseFloat(m[1]), b: parseFloat(m[2]) });
        continue;
      }

      m = s.match(yRe);
      if (m) {
        pairs.push({ m: parseFloat(m[1]), b: parseFloat(m[2].replace(/\s+/g, "")) });
      }
    }

    // Fallback for compact tuple list formats like: [(m,b), (m,b), ...]
    if (!pairs.length) {
      const tupleGlobalRe = new RegExp(`\\(\\s*(${num})\\s*,\\s*(${num})\\s*\\)`, "g");
      let m;
      while ((m = tupleGlobalRe.exec(source)) !== null) {
        pairs.push({ m: parseFloat(m[1]), b: parseFloat(m[2]) });
      }
    }

    return pairs;
  };

  App.intersectLines = function intersectLines(l1, l2) {
    const dm = l1.m - l2.m;
    if (Math.abs(dm) < EPS) return null;
    const x = (l2.b - l1.b) / dm;
    const y = l1.m * x + l1.b;
    if (!Number.isFinite(x) || !Number.isFinite(y)) return null;
    return { x, y };
  };

  App.computeIntersections = function computeIntersections(lines) {
    const pts = [];
    for (let i = 0; i < lines.length; i++) {
      for (let j = i + 1; j < lines.length; j++) {
        const p = App.intersectLines(lines[i], lines[j]);
        if (p) pts.push(p);
      }
    }
    return pts;
  };

  App.buildBBox = function buildBBox(lines, intersections) {
    if (intersections.length > 0) {
      let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
      for (const p of intersections) {
        minX = Math.min(minX, p.x);
        maxX = Math.max(maxX, p.x);
        minY = Math.min(minY, p.y);
        maxY = Math.max(maxY, p.y);
      }
      const dx = Math.max(1e-3, maxX - minX);
      const dy = Math.max(1e-3, maxY - minY);
      const pad = 0.2 * Math.max(dx, dy);
      return { minX: minX - pad, maxX: maxX + pad, minY: minY - pad, maxY: maxY + pad };
    }

    if (lines.length > 0) {
      let minY = Infinity, maxY = -Infinity;
      for (const l of lines) {
        minY = Math.min(minY, l.b);
        maxY = Math.max(maxY, l.b);
      }
      const spanY = Math.max(10, maxY - minY);
      return { minX: -spanY, maxX: spanY, minY: minY - 0.5 * spanY, maxY: maxY + 0.5 * spanY };
    }

    return { minX: -10, maxX: 10, minY: -10, maxY: 10 };
  };
})();

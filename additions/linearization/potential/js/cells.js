(function () {
  const App = window.App;
  const EPS = App.EPS;

  function sideValue(line, p) {
    return p.y - line.m * p.x - line.b;
  }

  function clipHalfPlane(poly, line, keepPositive) {
    if (poly.length === 0) return [];
    const out = [];
    for (let i = 0; i < poly.length; i++) {
      const a = poly[i];
      const b = poly[(i + 1) % poly.length];
      const va = sideValue(line, a);
      const vb = sideValue(line, b);
      const inA = keepPositive ? va >= -EPS : va <= EPS;
      const inB = keepPositive ? vb >= -EPS : vb <= EPS;

      if (inA && inB) {
        out.push(b);
      } else if (inA && !inB) {
        const t = va / (va - vb);
        out.push({ x: a.x + t * (b.x - a.x), y: a.y + t * (b.y - a.y) });
      } else if (!inA && inB) {
        const t = va / (va - vb);
        out.push({ x: a.x + t * (b.x - a.x), y: a.y + t * (b.y - a.y) });
        out.push(b);
      }
    }
    return out;
  }

  function cleanPolygon(poly) {
    if (!poly || poly.length < 3) return [];
    const tmp = [];
    for (let i = 0; i < poly.length; i++) {
      const p = poly[i];
      const q = poly[(i + 1) % poly.length];
      if (Math.hypot(p.x - q.x, p.y - q.y) > 1e-11) tmp.push(p);
    }
    if (tmp.length < 3) return [];

    const out = [];
    for (let i = 0; i < tmp.length; i++) {
      const a = tmp[(i + tmp.length - 1) % tmp.length];
      const b = tmp[i];
      const c = tmp[(i + 1) % tmp.length];
      const cross = (b.x - a.x) * (c.y - b.y) - (b.y - a.y) * (c.x - b.x);
      if (Math.abs(cross) > 1e-12) out.push(b);
    }
    return out.length >= 3 ? out : [];
  }

  function polygonArea(poly) {
    let s = 0;
    for (let i = 0; i < poly.length; i++) {
      const a = poly[i];
      const b = poly[(i + 1) % poly.length];
      s += a.x * b.y - b.x * a.y;
    }
    return Math.abs(s) * 0.5;
  }

  function polygonSignedArea(poly) {
    let s = 0;
    for (let i = 0; i < poly.length; i++) {
      const a = poly[i];
      const b = poly[(i + 1) % poly.length];
      s += a.x * b.y - b.x * a.y;
    }
    return s * 0.5;
  }

  function bboxOfPoly(poly) {
    let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
    for (const p of poly) {
      minX = Math.min(minX, p.x);
      maxX = Math.max(maxX, p.x);
      minY = Math.min(minY, p.y);
      maxY = Math.max(maxY, p.y);
    }
    return { minX, maxX, minY, maxY };
  }

  function normalizePoly(poly) {
    if (!poly || poly.length < 3) return null;
    let start = 0;
    for (let i = 1; i < poly.length; i++) {
      const a = poly[i];
      const b = poly[start];
      if (a.x < b.x || (Math.abs(a.x - b.x) < 1e-10 && a.y < b.y)) start = i;
    }
    const out = [];
    for (let i = 0; i < poly.length; i++) out.push(poly[(start + i) % poly.length]);
    return out;
  }

  function lineIntersection(a, b, c, d) {
    const r = { x: b.x - a.x, y: b.y - a.y };
    const s = { x: d.x - c.x, y: d.y - c.y };
    const denom = r.x * s.y - r.y * s.x;
    if (Math.abs(denom) < 1e-14) return null;
    const t = ((c.x - a.x) * s.y - (c.y - a.y) * s.x) / denom;
    return { x: a.x + t * r.x, y: a.y + t * r.y };
  }

  function clipPolygonByEdge(subject, a, b, orientationSign) {
    if (subject.length === 0) return [];
    const out = [];
    const inside = (p) => {
      const cross = (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
      return orientationSign * cross >= -1e-10;
    };

    for (let i = 0; i < subject.length; i++) {
      const cur = subject[i];
      const prev = subject[(i + subject.length - 1) % subject.length];
      const curIn = inside(cur);
      const prevIn = inside(prev);

      if (curIn) {
        if (!prevIn) {
          const x = lineIntersection(prev, cur, a, b);
          if (x) out.push(x);
        }
        out.push(cur);
      } else if (prevIn) {
        const x = lineIntersection(prev, cur, a, b);
        if (x) out.push(x);
      }
    }
    return out;
  }

  function dedupeCells(cells) {
    const seen = new Map();
    for (const c of cells) {
      const np = cleanPolygon(normalizePoly(c.poly));
      if (!np) continue;
      const area = polygonArea(np);
      const span = Math.max(
        Math.max(...np.map(p => Math.abs(p.x))),
        Math.max(...np.map(p => Math.abs(p.y))),
        1
      );
      const q = span * 1e-10;
      let key = `${np.length}|${c.parity}|${c.signs.join("")}|`;
      for (const p of np) key += `${Math.round(p.x / q)},${Math.round(p.y / q)};`;
      if (!seen.has(key)) seen.set(key, { poly: np, parity: c.parity, area, signs: c.signs.slice() });
    }
    return Array.from(seen.values());
  }

  function clipByLinearIneq(poly, a, b) {
    if (poly.length === 0) return [];
    const out = [];
    const val = (p) => a * p.x + b * p.y;
    for (let i = 0; i < poly.length; i++) {
      const cur = poly[i];
      const prev = poly[(i + poly.length - 1) % poly.length];
      const vc = val(cur);
      const vp = val(prev);
      const inCur = vc >= -1e-12;
      const inPrev = vp >= -1e-12;
      if (inCur) {
        if (!inPrev) {
          const t = vp / (vp - vc);
          out.push({ x: prev.x + t * (cur.x - prev.x), y: prev.y + t * (cur.y - prev.y) });
        }
        out.push(cur);
      } else if (inPrev) {
        const t = vp / (vp - vc);
        out.push({ x: prev.x + t * (cur.x - prev.x), y: prev.y + t * (cur.y - prev.y) });
      }
    }
    return out;
  }

  function hasUnboundedDirection(signs, lines) {
    // Intersect cone of directions d where each chosen half-plane stays valid as t -> +inf.
    // For side y - m*x - b >= 0: dy - m*dx >= 0. For <= 0: multiply by -1.
    let cone = [
      { x: -1, y: -1 },
      { x: 1, y: -1 },
      { x: 1, y: 1 },
      { x: -1, y: 1 },
    ];
    for (let i = 0; i < lines.length; i++) {
      const s = signs[i] ? 1 : -1;
      const a = s * (-lines[i].m);
      const b = s * 1;
      cone = clipByLinearIneq(cone, a, b);
      if (cone.length === 0) return false;
    }

    const uniq = [];
    for (const p of cone) {
      if (!uniq.some(q => Math.hypot(p.x - q.x, p.y - q.y) < 1e-10)) uniq.push(p);
    }
    if (uniq.length === 0) return false;

    let maxNorm2 = 0;
    for (const p of uniq) {
      const n2 = p.x * p.x + p.y * p.y;
      if (n2 > maxNorm2) maxNorm2 = n2;
    }
    return maxNorm2 > 1e-8;
  }

  function countFiniteSides(poly, lines) {
    let count = 0;
    for (const line of lines) {
      const seg = App.lineSegmentInPoly(line, poly);
      if (!seg) continue;
      const dx = seg[0].x - seg[1].x;
      const dy = seg[0].y - seg[1].y;
      if (dx * dx + dy * dy < 1e-12) continue;
      count++;
    }
    return count;
  }

  App.lineSegmentInPoly = function lineSegmentInPoly(line, poly) {
    if (!poly || poly.length < 3) return null;
    const pts = [];

    function f(p) {
      return p.y - line.m * p.x - line.b;
    }

    for (let i = 0; i < poly.length; i++) {
      const a = poly[i];
      const b = poly[(i + 1) % poly.length];
      const fa = f(a);
      const fb = f(b);
      if (Math.abs(fa) < 1e-9) pts.push({ x: a.x, y: a.y });
      if (Math.abs(fb) < 1e-9) pts.push({ x: b.x, y: b.y });
      const denom = fa - fb;
      if (Math.abs(denom) < 1e-14) continue;
      const t = fa / denom;
      if (t < -1e-9 || t > 1 + 1e-9) continue;
      pts.push({ x: a.x + t * (b.x - a.x), y: a.y + t * (b.y - a.y) });
    }

    const uniq = [];
    for (const p of pts) {
      if (!uniq.some(q => Math.hypot(p.x - q.x, p.y - q.y) < 1e-8)) uniq.push(p);
    }
    if (uniq.length < 2) return null;

    let bestA = uniq[0], bestB = uniq[1], bestD = -1;
    for (let i = 0; i < uniq.length; i++) {
      for (let j = i + 1; j < uniq.length; j++) {
        const d = (uniq[i].x - uniq[j].x) ** 2 + (uniq[i].y - uniq[j].y) ** 2;
        if (d > bestD) {
          bestD = d;
          bestA = uniq[i];
          bestB = uniq[j];
        }
      }
    }

    if (bestD < 1e-16) return null;
    return [bestA, bestB];
  };

  App.clipPolygonByConvex = function clipPolygonByConvex(subject, clipPoly) {
    if (!subject || subject.length < 3 || !clipPoly || clipPoly.length < 3) return [];
    const sign = polygonSignedArea(clipPoly) >= 0 ? 1 : -1;
    let out = subject.slice();
    for (let i = 0; i < clipPoly.length; i++) {
      const a = clipPoly[i];
      const b = clipPoly[(i + 1) % clipPoly.length];
      out = clipPolygonByEdge(out, a, b, sign);
      if (out.length < 3) return [];
    }
    return out;
  };

  App.buildCells = function buildCells(lines, basePoly) {
    if (!lines.length || !basePoly || basePoly.length < 3) return [];
    let cells = [{ poly: normalizePoly(basePoly), parity: 0, signs: [] }];

    for (let lineIdx = 0; lineIdx < lines.length; lineIdx++) {
      const line = lines[lineIdx];
      const next = [];
      for (const cell of cells) {
        const neg = cleanPolygon(clipHalfPlane(cell.poly, line, false));
        const pos = cleanPolygon(clipHalfPlane(cell.poly, line, true));

        const an = neg.length >= 3 ? polygonArea(neg) : 0;
        const ap = pos.length >= 3 ? polygonArea(pos) : 0;

        if (an > 0) {
          const signs = cell.signs.slice();
          signs[lineIdx] = 0;
          next.push({ poly: normalizePoly(neg), parity: cell.parity, signs });
        }
        if (ap > 0) {
          const signs = cell.signs.slice();
          signs[lineIdx] = 1;
          next.push({ poly: normalizePoly(pos), parity: cell.parity ^ 1, signs });
        }
      }

      cells = dedupeCells(next);
      if (cells.length === 0) break;
    }

    return cells.map(c => {
      const external = hasUnboundedDirection(c.signs, lines);
      const finiteSides = countFiniteSides(c.poly, lines);
      const sideCount = external ? (finiteSides + 1) : c.poly.length;
      return {
        poly: c.poly,
        area: c.area,
        parity: c.parity,
        external,
        sideCount,
      };
    });
  };
})();

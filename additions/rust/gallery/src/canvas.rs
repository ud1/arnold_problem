use std::collections::HashMap;

use web_sys::CanvasRenderingContext2d;

use crate::geometry::{Arrangement, Point};

// Минимальный видимый размер полигона в device-пикселях.
const MIN_POLYGON_PX: f64 = 0.1;

/// Единственный источник цветов полигонов: используется канвас-рендерером и
/// download_arrangement. Статичный SVG галереи красит полигоны через CSS-правила
/// polygon.* в styles.css, которые должны им соответствовать (тест polygon_fill_matches_css).
pub fn polygon_fill(b: bool, ext: bool, sz: i32) -> &'static str {
    match (sz, ext, b) {
        (3, false, true) => "#3d635f",
        (3, false, false) => "#6e312f",
        (3, true, true) => "#cccccc",
        (3, true, false) => "#dddddd",
        (4, false, true) => "#888844",
        (4, true, true) => "#eeeecc",
        (4, _, false) => "#ffffdd",
        (_, true, true) => "#eeeeee",
        (_, false, true) => "#eaa2a0",
        _ => "#effefc",
    }
}

struct RenderPoly {
    points: Vec<Point>,
    min_x: f64,
    max_x: f64,
    min_y: f64,
    max_y: f64,
}

struct RenderGroup {
    color: &'static str,
    polys: Vec<RenderPoly>,
}

/// Предварительно сгруппированные по цвету полигоны с их bbox —
/// то, что нужно канвасу для быстрого кадра.
pub struct RenderTarget {
    w: f64,
    h: f64,
    groups: Vec<RenderGroup>,
}

pub fn build_target(arrangement: &Arrangement) -> RenderTarget {
    let mut by_color: HashMap<&'static str, Vec<RenderPoly>> = HashMap::new();

    for polygon in arrangement.polygons.iter() {
        let bbox = crate::geometry::polygon_bbox(&polygon.points);
        let color = polygon_fill(polygon.b, polygon.ext, polygon.sz);
        by_color.entry(color).or_default().push(RenderPoly {
            points: polygon.points.clone(),
            min_x: bbox.0,
            max_x: bbox.1,
            min_y: bbox.2,
            max_y: bbox.3,
        });
    }

    let mut groups: Vec<RenderGroup> = by_color
        .into_iter()
        .map(|(color, polys)| RenderGroup { color, polys })
        .collect();
    groups.sort_by(|a, b| match a.color.cmp(b.color) {
        std::cmp::Ordering::Equal => a.polys.len().cmp(&b.polys.len()),
        ord => ord,
    });

    RenderTarget { w: arrangement.w, h: arrangement.h, groups }
}

/// Преобразует координаты вершины из системы расположения прямых
/// в device-пиксели канваса. Все вычисления в f64: canvas не получает
/// огромных координат, потому что полигоны заранее клипуются окном.
pub fn paint(
    ctx: &CanvasRenderingContext2d,
    target: &RenderTarget,
    canvas_w: f64,
    canvas_h: f64,
    scale: f64,
    shift: Point,
) {
    ctx.clear_rect(0.0, 0.0, canvas_w, canvas_h);

    // Соотношение как у preserveAspectRatio="xMidYMid slice" в SVG-версии.
    let unit_px = (canvas_w / target.w).max(canvas_h / target.h);
    let s_k = scale * unit_px;
    let origin_x = shift.x * target.w * unit_px + canvas_w * 0.5;
    let origin_y = shift.y * target.h * unit_px + canvas_h * 0.5;

    let mut poly_buf: Vec<Point> = Vec::new();
    let mut clip_buf: Vec<Point> = Vec::new();

    for group in target.groups.iter() {
        let mut painted = false;
        ctx.begin_path();

        for p in group.polys.iter() {
            // bbox в экранных координатах (y инвертируется)
            let x_lo = p.min_x * s_k + origin_x;
            let x_hi = p.max_x * s_k + origin_x;
            let y_lo = -p.max_y * s_k + origin_y;
            let y_hi = -p.min_y * s_k + origin_y;

            if x_hi < 0.0 || x_lo > canvas_w || y_hi < 0.0 || y_lo > canvas_h {
                continue;
            }
            if x_hi - x_lo < MIN_POLYGON_PX && y_hi - y_lo < MIN_POLYGON_PX {
                continue;
            }

            poly_buf.clear();
            for pt in p.points.iter() {
                poly_buf.push(Point::new(pt.x * s_k + origin_x, -pt.y * s_k + origin_y));
            }

            // Клиппинг нужен, чтобы на большом зуме гигантские полигоны
            // не передавались в canvas координатами вне window-размера.
            clip_half_plane(&mut poly_buf, &mut clip_buf, canvas_w, canvas_h, ClipEdge::Left);
            clip_half_plane(&mut poly_buf, &mut clip_buf, canvas_w, canvas_h, ClipEdge::Right);
            clip_half_plane(&mut poly_buf, &mut clip_buf, canvas_w, canvas_h, ClipEdge::Top);
            clip_half_plane(&mut poly_buf, &mut clip_buf, canvas_w, canvas_h, ClipEdge::Bottom);

            let len = poly_buf.len();
            if len < 3 {
                continue;
            }

            ctx.move_to(poly_buf[0].x, poly_buf[0].y);
            for pt in poly_buf[1..len].iter() {
                ctx.line_to(pt.x, pt.y);
            }
            ctx.close_path();
            painted = true;
        }

        if painted {
            ctx.set_fill_style_str(group.color);
            ctx.fill();
        }
    }
}

enum ClipEdge {
    Left,
    Right,
    Top,
    Bottom,
}

/// Одна половина алгоритма Сазерленда–Ходжмана. Точки уже в экранной
/// системе координат канваса, клиппинг идёт прямоугольником окна.
fn clip_half_plane(
    src: &mut Vec<Point>,
    dst: &mut Vec<Point>,
    canvas_w: f64,
    canvas_h: f64,
    edge: ClipEdge,
) {
    dst.clear();
    let n = src.len();
    if n == 0 {
        return;
    }

    let inside = |p: &Point| -> bool {
        match edge {
            ClipEdge::Left => p.x >= 0.0,
            ClipEdge::Right => p.x <= canvas_w,
            ClipEdge::Top => p.y >= 0.0,
            ClipEdge::Bottom => p.y <= canvas_h,
        }
    };
    let cross_at = |a: &Point, b: &Point| -> Point {
        match edge {
            ClipEdge::Left | ClipEdge::Right => {
                let c = match edge { ClipEdge::Left => 0.0, _ => canvas_w };
                let t = (c - a.x) / (b.x - a.x);
                Point::new(c, a.y + (b.y - a.y) * t)
            }
            ClipEdge::Top | ClipEdge::Bottom => {
                let c = match edge { ClipEdge::Top => 0.0, _ => canvas_h };
                let t = (c - a.y) / (b.y - a.y);
                Point::new(a.x + (b.x - a.x) * t, c)
            }
        }
    };

    for i in 0..n {
        let a = src[i];
        let b = src[(i + 1) % n];
        let a_in = inside(&a);
        let b_in = inside(&b);

        if a_in {
            dst.push(a);
            if !b_in {
                dst.push(cross_at(&a, &b));
            }
        } else if b_in {
            dst.push(cross_at(&a, &b));
        }
    }

    std::mem::swap(src, dst);
}

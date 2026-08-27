use std::ops::{Add, Div, Mul, Sub};
use serde::{Deserialize, Serialize};

#[derive(Clone, Copy, Debug)]
pub struct Point {
    pub x: f64,
    pub y: f64,
}

impl Point {
    pub fn new(x: f64, y: f64) -> Self {
        Point { x, y }
    }
}

impl Add for Point {
    type Output = Self;

    fn add(self, other: Self) -> Self::Output {
        Point {
            x: self.x + other.x,
            y: self.y + other.y,
        }
    }
}

impl Sub for Point {
    type Output = Self;

    fn sub(self, other: Self) -> Self::Output {
        Point {
            x: self.x - other.x,
            y: self.y - other.y,
        }
    }
}

impl Mul<f64> for Point {
    type Output = Self;
    fn mul(self, other: f64) -> Self::Output {
        Point{x: self.x * other, y: self.y * other}
    }
}

impl Div<f64> for Point {
    type Output = Self;
    fn div(self, other: f64) -> Self::Output {
        Point{x: self.x / other, y: self.y / other}
    }
}

#[derive(Clone)]
pub struct PolygonData {
    pub points: Vec<Point>,
    pub b: bool,
    pub ext: bool,
    pub sz: i32,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct ArrangementConf {
    gens: String,
    lines: Vec<[f64; 2]>, // m,b
    author: Option<String>,
    source: Option<String>,
}

#[derive(Debug, PartialEq, Clone)]
struct Gen {
    g: i32,
    star: bool,
}

fn parse_gens(gens_str: &str) -> Vec<Gen> {
    gens_str.trim().split_whitespace().filter_map(|part| {
        let (g_str, star) = part.strip_suffix('*')
            .map(|s| (s, true))
            .unwrap_or((part, false));

        g_str.parse::<i32>().ok().map(|g| Gen { g, star })
    }).collect()
}

#[derive(Copy, Clone, Debug)]
enum CrossPoint {
    Double(i32, i32),
    Triple(i32, i32, i32),
}

#[derive(Copy, Clone, Debug)]
struct ExtLineSeg {
    l: i32,
    up: bool,
}

#[derive(Debug)]
struct Poly {
    ext_l: Option<ExtLineSeg>,
    ext_r: Option<ExtLineSeg>,
    bottom: bool,
    g: i32,
    left: Vec<CrossPoint>,
    right: Vec<CrossPoint>,
}

fn get_n(gens: &Vec<Gen>) -> i32 {
    gens.iter().map(|g| if g.star {g.g + 3} else {g.g + 2}).max().unwrap()
}

fn make_polygons(gens: &Vec<Gen>) -> Vec<Poly> {
    let mut result: Vec<Poly> = Vec::new();

    let n = get_n(gens);

    let mut a = Vec::new();
    for i in 0..n {
        a.push(i);
    }

    let mut poly = Vec::new();
    for i in 0..=n {
        let p = if i == 0 {
            Poly {
                ext_l: Some(ExtLineSeg{l: n - 1, up: false}),
                ext_r: Some(ExtLineSeg{l: 0, up: true}),
                bottom: false,
                g : i - 1,
                left: Vec::new(),
                right: Vec::new(),
            }
        } else if i == n {
            Poly {
                ext_l: Some(ExtLineSeg{l: n - 1, up: true}),
                ext_r: Some(ExtLineSeg{l: 0, up: false}),
                bottom: false,
                g : i - 1,
                left: Vec::new(),
                right: Vec::new(),
            }
        } else {
            Poly {
                ext_l: Some(ExtLineSeg{l: i - 1, up: true}),
                ext_r: Some(ExtLineSeg{l: i, up: true}),
                bottom: false,
                g : i - 1,
                left: Vec::new(),
                right: Vec::new(),
            }
        };
        poly.push(p);
    }

    for g in gens {
        let cp = if g.star {
            CrossPoint::Triple(a[g.g as usize], a[g.g as usize + 1], a[g.g as usize + 2])
        } else {
            CrossPoint::Double(a[g.g as usize], a[g.g as usize + 1])
        };
        let ln1 = if g.star {2} else {1};

        let p = &mut poly[g.g as usize];
        p.right.push(cp);

        for i in 0..ln1 {
            let p = &mut poly[g.g as usize + i + 1];
            p.right.push(cp);

            let new_p = Poly {
                ext_l: None,
                ext_r: None,
                bottom: false,
                g: g.g + i as i32,
                left: Vec::new(),
                right: vec![cp],
            };
            let old_p = std::mem::replace(p, new_p);
            result.push(old_p);
        }

        let p = &mut poly[g.g as usize + ln1 + 1];
        p.left.push(cp);

        a.swap(g.g as usize, g.g as usize + ln1);

        if g.g == 0 {
            poly[0].ext_l = Some(ExtLineSeg{l: a[0], up: false});
        }
        else if (g.g + ln1 as i32 + 1) == n {
            poly[n as usize].ext_r = Some(ExtLineSeg{l: a[n as usize - 1], up: false});
        }
    }

    for i in 0..=n {
        let p = &mut poly[i as usize];
        p.bottom = true;
        if i > 0 && i < n {
            p.ext_l = Some(ExtLineSeg{l: a[i as usize], up: false});
            p.ext_r = Some(ExtLineSeg{l: a[i as usize - 1], up: false});
        }
    }

    result.extend(poly);
    result
}

fn cross_point(l1: [f64; 2], l2: [f64; 2]) -> Point {
    let db = l1[0] - l2[0];
    let x = (l2[1] - l1[1]) / db;
    let y = (l1[0] * l2[1] - l2[0] * l1[1]) / db;
    Point::new(x, y)
}

#[derive(Clone)]
pub struct Arrangement {
    pub polygons: Vec<PolygonData>,
    pub w: f64,
    pub h: f64,
    pub name: String,
    pub n: i32,
    pub parallel_pairs: i32,
    pub triple_points: i32,
    pub generators_string: String,
    pub tri_count: usize,
    pub original_author: Option<String>,
    pub original_source: Option<String>,
    pub lines: Vec<[f64; 2]>,
}

struct LineClipCrossPoint {
    pub p: Point,
    pub edge: i32,
}

pub fn get_arnold_diff(arrangement: &Arrangement) -> i32 {
    let b_count = arrangement.polygons.iter()
        .filter(|p| p.b).count();
    let w_count = arrangement.polygons.len() - b_count;

    (b_count - w_count) as i32
}

fn arnold(n: i32) -> i32 {
    if n % 2 == 1 {
        return 2 * tri_count(n) - total_internal_areas(n);
    }

    arnold(n - 1)
}

fn arnold_nk(n: i32, k: i32) -> i32 {
    if n == 10 && k == 4 {
        return 14;
    }

    if n == 12 && k == 6 {
        return 21;
    }

    let a = arnold_simple(n, k);
    if n % 2 == 1 {
        return a.min(arnold(n) - k);
    }

    a.min(arnold(n - 1) + k)
}

fn arnold_simple(n: i32, k: i32) -> i32 {
    if n <= 1 {
        return 0;
    }

    if n == 2 && k == 1 {
        return 1;
    }

    let b4 = (n * n + n - 2 * k) % 3;
    let a = (n * n + n - 2 * k - 6 - 4 * b4) / 6;

    a
}

fn tri_count(n: i32) -> i32 {
    if n <= 2 {
        return 0;
    }

    if n == 11 {
        return 32;
    }

    if n == 12 {
        return 38;
    }

    if n % 2 == 1 {
        return n * (n - 2) / 3;
    }

    let mut res = tri_count(n - 1) + n / 2;
    if n % 6 != 2 {
        res -= 1;
    }

    res
}

fn total_internal_areas(n: i32) -> i32 {
    n * (n + 1) / 2 + 1 - 2 * n
}

pub fn is_arnold_solution(arrangement: &Arrangement) -> bool {
    let a_max = arnold_nk(arrangement.n, arrangement.parallel_pairs);
    let a = get_arnold_diff(arrangement);

    a >= a_max
}

pub fn is_kobon_solution(arrangement: &Arrangement) -> bool {
    let max_tris = tri_count(arrangement.n);
    arrangement.tri_count as i32 >= max_tris
}

fn calc_triple_points(gens: &Vec<Gen>) -> i32 {
    gens.iter()
        .filter(|p| p.star)
        .count() as i32
}

pub fn generate_arrangement(conf: &ArrangementConf, name: String) -> Arrangement {
    let gens = parse_gens(&conf.gens);

    let poly = make_polygons(&gens);

    let mut max_p = Point::new(-f64::MAX, -f64::MAX);
    let mut min_p = Point::new(f64::MAX, f64::MAX);

    let mut odd_tri_count = 0;
    let mut even_tri_count = 0;
    let mut internal_tri_count = 0;
    for p in &poly {
        let mut add_pt = |ct: CrossPoint| {
            let (i1, i2) = match ct {
                CrossPoint::Double(i1, i2) => (i1, i2),
                CrossPoint::Triple(i1, _i2, i3) => (i1, i3),
            };
            let l1 = conf.lines[i1 as usize];
            let l2 = conf.lines[i2 as usize];
            let cp = cross_point(l1, l2);
            max_p.x = max_p.x.max(cp.x);
            max_p.y = max_p.y.max(cp.y);
            min_p.x = min_p.x.min(cp.x);
            min_p.y = min_p.y.min(cp.y);
        };
        for pt in p.left.iter() {
            add_pt(*pt);
        }
        for pt in p.right.iter() {
            add_pt(*pt);
        }

        let pt_count = p.left.len() + p.right.len() + if p.ext_l.is_some() { 2 } else { 0 };
        if pt_count == 3 {
            if p.g % 2 == 0 {
                even_tri_count += 1;
            } else {
                odd_tri_count += 1;
            }
            if p.ext_l.is_none() {
                internal_tri_count += 1;
            }
        }
    }

    let odd_prevail = odd_tri_count >= even_tri_count;

    let c = (max_p + min_p) * 0.5;
    let sz = round_size(max_p - min_p);
    let max_size = sz.x.max(sz.y);
    let scale = 1200.0 / max_size;

    let lines: Vec<[f64; 2]> = conf.lines.iter().map(|line| {
        let m = line[0];
        let b = line[1];
        let new_b = m * c.x * scale + b * scale - c.y * scale;
        [m, new_b]
    }).collect();

    let clip_w = (sz.x * scale * 1.2 * 0.5).ceil() * 2.0;
    let clip_h = (sz.y * scale * 1.2 * 0.5).ceil() * 2.0;

    let get_line_seg_cross_point = |l: ExtLineSeg| {
        let lp = lines[l.l as usize];
        let m = lp[0];
        let b = lp[1];

        return if l.up {
            if m > 0.0 {
                let x_r = clip_w * 0.5;
                let y_r = m * x_r + b;
                if y_r >= -clip_h * 0.5 && y_r <= clip_h * 0.5 {
                    return LineClipCrossPoint { p: Point::new(x_r, y_r), edge: 2 };
                }

                let y_u = clip_h * 0.5;
                let x_u = (y_u - b) / m;
                LineClipCrossPoint { p: Point::new(x_u, y_u), edge: 1 }
            } else {
                let x_l = -clip_w * 0.5;
                let y_l = m * x_l + b;
                if y_l >= -clip_h * 0.5 && y_l <= clip_h * 0.5 {
                    return LineClipCrossPoint { p: Point::new(x_l, y_l), edge: 0 };
                }

                let y_u = clip_h * 0.5;
                let x_u = (y_u - b) / m;
                LineClipCrossPoint { p: Point::new(x_u, y_u), edge: 1 }
            }
        } else {
            if m > 0.0 {
                let x_l = -clip_w * 0.5;
                let y_l = m * x_l + b;
                if y_l >= -clip_h * 0.5 && y_l <= clip_h * 0.5 {
                    return LineClipCrossPoint { p: Point::new(x_l, y_l), edge: 0 };
                }

                let y_b = -clip_h * 0.5;
                let x_b = (y_b - b) / m;
                LineClipCrossPoint { p: Point::new(x_b, y_b), edge: 3 }
            } else {
                let x_r = clip_w * 0.5;
                let y_r = m * x_r + b;
                if y_r >= -clip_h * 0.5 && y_r <= clip_h * 0.5 {
                    return LineClipCrossPoint { p: Point::new(x_r, y_r), edge: 2 };
                }

                let y_b = -clip_h * 0.5;
                let x_b = (y_b - b) / m;
                LineClipCrossPoint { p: Point::new(x_b, y_b), edge: 3 }
            }
        }
    };

    let add_ct = |ct: CrossPoint, points: &mut Vec<Point>| {
        let (i1, i2) = match ct {
            CrossPoint::Double(i1, i2) => (i1, i2),
            CrossPoint::Triple(i1, _i2, i3) => (i1, i3),
        };
        let l1 = lines[i1 as usize];
        let l2 = lines[i2 as usize];
        let cp = cross_point(l1, l2);
        points.push(cp);
    };

    let mut polygons = Vec::new();
    for p in &poly {
        let mut points = Vec::new();
        if let (Some(ext_r), Some(ext_l)) = (p.ext_r, p.ext_l) {
            let cp_r = get_line_seg_cross_point(ext_r);
            let cp_l = get_line_seg_cross_point(ext_l);

            points.push(cp_l.p);

            for i in 0..4 {
                let edge = (cp_l.edge + i) % 4;
                if edge == cp_r.edge {
                    break;
                }

                if edge == 0 {
                    points.push(Point::new(-clip_w*0.5, clip_h*0.5));
                }
                else if edge == 1 {
                    points.push(Point::new(clip_w*0.5, clip_h*0.5));
                }
                else if edge == 2 {
                    points.push(Point::new(clip_w*0.5, -clip_h*0.5));
                }
                else {
                    points.push(Point::new(-clip_w*0.5, -clip_h*0.5));
                }
            }

            points.push(cp_r.p);
        }

        if !p.bottom {
            for ct in p.right.iter() {
                add_ct(*ct, &mut points);
            }
            for ct in p.left.iter().rev() {
                add_ct(*ct, &mut points);
            }
        }
        else {
            for ct in p.left.iter().rev() {
                add_ct(*ct, &mut points);
            }
            for ct in p.right.iter() {
                add_ct(*ct, &mut points);
            }
        }

        let is_odd = p.g % 2 != 0;
        let pt_count = p.left.len() + p.right.len() + if p.ext_l.is_some() { 2 } else { 0 };
        polygons.push(PolygonData {
            points,
            b: odd_prevail == is_odd,
            ext: p.ext_r.is_some(),
            sz: pt_count as i32,
        });
    }

    let n = get_n(&gens);
    let triple_points = calc_triple_points(&gens);
    let parallel_pairs = (n * (n - 1) / 2) - gens.len() as i32 - triple_points * 2;

    Arrangement {
        polygons,
        w: clip_w,
        h: clip_h,
        name,
        generators_string: conf.gens.clone(),
        n,
        parallel_pairs,
        triple_points,
        tri_count: internal_tri_count,
        original_author: conf.author.clone(),
        original_source: conf.source.clone(),
        lines
    }
}

fn round_size(sz: Point) -> Point {
    if sz.x > sz.y {
        Point{x: sz.x, y: sz.x}
    }
    else {
        Point{x: sz.y, y: sz.y}
    }
}

pub fn solution_for_names(arrangement: &Arrangement) -> &str {
    let is_arnold = is_arnold_solution(arrangement);
    let is_kobon = is_kobon_solution(arrangement);

    if is_arnold && is_kobon {
        "Arnold and Kobon"
    }
    else if is_arnold {
        "Arnold"
    }
    else if is_kobon {
        "Kobon"
    }
    else {
        "Unknown"
    }
}
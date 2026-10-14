use std::collections::HashMap;
use std::sync::{Mutex, OnceLock};
use web_sys::{DomRect, Event, EventTarget, KeyboardEvent, MouseEvent, PointerEvent, TouchEvent, WheelEvent};
use leptos::prelude::{event_target_value, Get, LocalResource, Memo, NodeRef, RwSignal, Set, Update};
use gloo_net::http::Request;
use leptos::html::{Input};
use leptos::logging::log;
use serde::{Deserialize, Serialize};
use wasm_bindgen::{JsCast, JsValue};
use crate::{geometry};
use crate::geometry::{Arrangement, ArrangementConf, Point};

#[derive(Clone, Copy, Debug)]
struct DragState {
    is_dragging: bool,
    start: Point,
    start_shift: Point,
}

impl Default for DragState {
    fn default() -> Self {
        Self {
            is_dragging: false,
            start: Point::new(0.0, 0.0),
            start_shift: Point::new(0.0, 0.0),
        }
    }
}

#[derive(Clone, Copy, Debug)]
struct PinchState {
    is_active: bool,
    initial_distance: f64,
    initial_scale: f64,
}

impl Default for PinchState {
    fn default() -> Self {
        Self {
            is_active: false,
            initial_distance: 0.0,
            initial_scale: 1.0,
        }
    }
}

#[derive(Clone, Copy)]
pub struct ArrangementState {
    pub scale: RwSignal<f64>,
    pub shift: RwSignal<Point>,
    drag_state: RwSignal<DragState>,
    pinch_state: RwSignal<PinchState>,
}

impl Default for ArrangementState {
    fn default() -> Self {
        ArrangementState {
            scale: RwSignal::new(1.0),
            shift: RwSignal::new(Point { x: 0.0, y: 0.0 }),
            drag_state: RwSignal::new(DragState::default()),
            pinch_state: RwSignal::new(PinchState::default()),
        }
    }
}

fn touch_distance(t1: &web_sys::Touch, t2: &web_sys::Touch) -> f64 {
    let dx = t1.client_x() as f64 - t2.client_x() as f64;
    let dy = t1.client_y() as f64 - t2.client_y() as f64;
    (dx * dx + dy * dy).sqrt()
}

fn touch_center(t1: &web_sys::Touch, t2: &web_sys::Touch, target: &web_sys::SvgElement) -> Point {
    let rect = target.get_bounding_client_rect();
    let cx = (t1.client_x() + t2.client_x()) as f64 / 2.0;
    let cy = (t1.client_y() + t2.client_y()) as f64 / 2.0;
    get_rel_pos(rect, Point::new(cx, cy))
}

fn get_rect(current_target: Option<EventTarget>) -> DomRect {
    current_target
        .expect("Event has no current_target")
        .dyn_into::<web_sys::SvgElement>()
        .expect("Current target is not an SvgElement")
        .get_bounding_client_rect()
}

fn get_rel_mouse_pos(ev: &MouseEvent) -> Point {
    let rect = get_rect(ev.current_target());

    let mouse_x_from_rect_left = ev.client_x() as f64 - rect.left();
    let mouse_y_from_rect_top = ev.client_y() as f64 - rect.top();
    get_rel_pos(rect, Point::new(mouse_x_from_rect_left, mouse_y_from_rect_top))
}

// Размеры svg считаем квадратными
fn get_rel_pos(rect: DomRect, pos: Point) -> Point {
    let rect_w = rect.width();
    let rect_h = rect.height();

    let max_side = rect_w.max(rect_h);

    let offset_x = (rect_w - max_side) * 0.5;
    let offset_y = (rect_h - max_side) * 0.5;

    let mouse_x_from_rect_left = pos.x - rect.left();
    let mouse_y_from_rect_top = pos.y - rect.top();

    let x = (mouse_x_from_rect_left - offset_x) / max_side - 0.5;
    let y = (mouse_y_from_rect_top - offset_y) / max_side - 0.5;

    Point { x, y }
}

impl ArrangementState {
    pub fn on_wheel(&self, ev: WheelEvent) {
        ev.prevent_default();

        let mouse = get_rel_mouse_pos(&*ev);

        let current_scale = self.scale.get();
        let current_shift = self.shift.get();

        let factor = if ev.delta_y() < 0.0 { 1.1 } else { 1.0 / 1.1 };
        let new_scale = (current_scale * factor).clamp(0.1, 1000000.0);

        let d = mouse - current_shift;

        let new_shift = mouse - d * (new_scale / current_scale);

        self.scale.set(new_scale);
        self.shift.set(new_shift);
    }

    pub fn on_pointer_down(&self, ev: PointerEvent) {
        if self.pinch_state.get().is_active {
            return;
        }

        if ev.button() != 0 { return; } // Left button
        ev.prevent_default();
        ev.stop_propagation();

        let current = get_rel_mouse_pos(&*ev);

        self.drag_state.update(|ds| {
            ds.is_dragging = true;
            ds.start = current;
            ds.start_shift = self.shift.get();
        });
    }

    pub fn on_pointer_move(&self, ev: PointerEvent) {
        if self.pinch_state.get().is_active {
            return;
        }

        if !self.drag_state.get().is_dragging {
            return;
        }

        let current = get_rel_mouse_pos(&*ev);

        let drag = self.drag_state.get();
        let delta = current - drag.start;

        self.shift.set(drag.start_shift + delta);
    }

    pub fn on_pointer_up(&self, _ev: PointerEvent) {
        self.drag_state.update(|ds| ds.is_dragging = false);
    }

    pub fn on_touch_start(&self, ev: TouchEvent) {
        if ev.touches().length() != 2 {
            return;
        }
        ev.prevent_default();

        let t0 = ev.touches().get(0).unwrap();
        let t1 = ev.touches().get(1).unwrap();

        let distance = touch_distance(&t0, &t1);

        self.pinch_state.set(PinchState {
            is_active: true,
            initial_distance: distance,
            initial_scale: self.scale.get(),
        });
    }

    pub fn on_touch_move(&self, ev: TouchEvent) {
        let pinch = self.pinch_state.get();
        if !pinch.is_active || ev.touches().length() != 2 {
            return;
        }
        ev.prevent_default();

        let t0 = ev.touches().get(0).unwrap();
        let t1 = ev.touches().get(1).unwrap();

        let target = ev.current_target()
            .unwrap()
            .dyn_into::<web_sys::SvgElement>()
            .unwrap();

        let current_distance = touch_distance(&t0, &t1);
        let center = touch_center(&t0, &t1, &target);

        // Вычисляем коэффициент зума
        let zoom_factor = current_distance / pinch.initial_distance;
        let new_scale = (pinch.initial_scale * zoom_factor).clamp(0.1, 100.0);

        // Применяем зум относительно центральной точки между пальцами
        let d = center - self.shift.get();
        let new_shift = center - d * (new_scale / self.scale.get());

        self.scale.set(new_scale);
        self.shift.set(new_shift);
    }

    pub fn on_touch_end(&self, ev: TouchEvent) {
        // Если осталось меньше 2 пальцев, завершаем pinch-жест
        if ev.touches().length() < 2 {
            self.pinch_state.update(|ps| ps.is_active = false);
        }
    }
}

fn escape_xml(input: &str) -> String {
    let mut escaped = String::with_capacity(input.len());

    for ch in input.chars() {
        match ch {
            '&' => escaped.push_str("&amp;"),
            '<' => escaped.push_str("&lt;"),
            '>' => escaped.push_str("&gt;"),
            '"' => escaped.push_str("&quot;"),
            '\'' => escaped.push_str("&apos;"),
            _ => escaped.push(ch),
        }
    }

    escaped
}

fn get_local_date_str() -> String {
    let date = js_sys::Date::new_0();
    format!(
        "{:04}-{:02}-{:02}",
        date.get_full_year(),
        date.get_month() + 1,
        date.get_date()
    )
}

pub fn download_arrangement(arrangement: &Arrangement) {
    let mut result = String::new();
    result.push_str(&format!("<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"{} {} {} {}\" width=\"{}\" height=\"{}\">\n",
                             -arrangement.w*0.5, -arrangement.h*0.5, arrangement.w, arrangement.h, arrangement.w, arrangement.h));
    result.push_str("  <metadata>\n");
    result.push_str("    <rdf:RDF xmlns:rdf=\"https://www.w3.org/1999/02/22-rdf-syntax-ns#\" xmlns:dc=\"https://purl.org/dc/elements/1.1/\" xmlns:cc=\"https://creativecommons.org/ns#\">\n");
    result.push_str("      <cc:Work rdf:about=\"\">\n");
    result.push_str("        <cc:license rdf:resource=\"https://creativecommons.org/licenses/by/4.0/\" />\n");
    result.push_str("        <dc:format>image/svg+xml</dc:format>\n");
    result.push_str(&format!("        <dc:title>{} solution: {} lines, {} triangles</dc:title>\n", geometry::solution_for_names(arrangement), arrangement.n, arrangement.tri_count));
    result.push_str("        <dc:description>This SVG was programmatically generated based on precise line coefficients calculated by the creator.</dc:description>\n");
    result.push_str(&format!("        <dc:date>{}</dc:date>\n", get_local_date_str()));
    result.push_str("        <dc:creator><cc:Agent><dc:title>Denis Utkin and Roman Parpalak</dc:title></cc:Agent></dc:creator>\n");

    if let Some(author) = &arrangement.original_author {
        result.push_str(&format!("        <dc:contributor><cc:Agent><dc:title>{}</dc:title></cc:Agent><dc:contributor>\n", escape_xml(author)));
    }
    if let Some(source) = &arrangement.original_source {
        result.push_str(&format!("        <dc:source>{}</dc:source>\n", escape_xml(source)));
    }
    result.push_str(&format!("        <dc:identifier>{}</dc:identifier>\n", arrangement.name));

    result.push_str("        <dc:subject>\n");
    result.push_str("          <rdf:Bag>\n");
    result.push_str("            <rdf:li>Kobon triangles</rdf:li>\n");
    result.push_str("            <rdf:li>discrete geometry</rdf:li>\n");
    result.push_str("            <rdf:li>line arrangements</rdf:li>\n");
    result.push_str("          </rdf:Bag>\n");
    result.push_str("        </dc:subject>\n");

    result.push_str("        <dc:source>https://ud1.github.io/kobon-solutions/gallery/</dc:source>\n");
    result.push_str("      </cc:Work>\n");
    result.push_str("    </rdf:RDF>\n");
    result.push_str("  </metadata>\n");
    result.push_str(&format!("  <!-- Generators: {} -->\n", arrangement.generators_string));
    result.push_str("<!-- Lines (y = m*x + b):\n");
    result.push_str("m,b\n");
    for mb in arrangement.lines.iter() {
        result.push_str(&format!("{},{}\n", mb[0], mb[1]));
    }
    result.push_str("-->\n");
    result.push_str("  <style>
    .int.b {fill: #eaa2a0;}
    .ext.b {fill: #eeeeee;}
    .w {fill: #effefc;}
    .s3.int.b {fill: #3d635f;}
    .s3.int.w {fill: #6e312f;}
    .s3.ext.b {fill: #cccccc;}
    .s3.ext.w {fill: #dddddd;}
    .s4.int.b {fill: #888844;}
    .s4.ext.b {fill: #eeeecc;}
    .s4.w {fill: #ffffdd;}
  </style>
  ");

    for p in arrangement.polygons.iter() {
        result.push_str(&format!("  <polygon class=\"{} {} s{}\" points=\"", if p.b {"b"} else {"w"}, if p.ext {"ext"} else {"int"}, p.sz));
        for (i, pt) in p.points.iter().enumerate() {
            if i > 0 {
                result.push_str(" ");
            }
            result.push_str(&format!("{},{}", pt.x, -pt.y));
        }
        result.push_str("\" />\n");
    }

    result.push_str("</svg>");

    let options = web_sys::BlobPropertyBag::new();
    options.set_type("image/svg+xml;charset=utf-8;");

    let parts = js_sys::Array::new();
    parts.push(&JsValue::from_str(&result));

    if let Ok(blob) = web_sys::Blob::new_with_str_sequence_and_options(&parts, &options) {
        if let Ok(url) = web_sys::Url::create_object_url_with_blob(&blob) {
            let window = web_sys::window().unwrap();
            let document = window.document().unwrap();

            if let Ok(el) = document.create_element("a") {
                let anchor: web_sys::HtmlAnchorElement = el.unchecked_into();
                anchor.set_href(&url);
                anchor.set_download(&format!("{}.svg", arrangement.name));
                anchor.click();

                let _ = web_sys::Url::revoke_object_url(&url);
            }
        }
    }
}

fn get_file_name(conf_file: &String) -> String {
    if let Some(idx) = conf_file.rfind('/') {
        return conf_file[idx + 1..].to_owned();
    }

    conf_file.clone()
}

static CACHE: OnceLock<Mutex<HashMap<String, Arrangement>>> = OnceLock::new();

pub fn already_fetched(conf_file: String) -> bool {
    let cache = CACHE.get_or_init(|| Mutex::new(HashMap::new()));
    let lock = cache.lock().unwrap();
    lock.get(&conf_file).is_some()
}

pub async fn fetch_arrangement_config(conf_file: String) -> Option<Arrangement> {
    let cache = CACHE.get_or_init(|| Mutex::new(HashMap::new()));

    {
        let lock = cache.lock().unwrap();
        if let Some(cached_value) = lock.get(&conf_file) {
            return Some(cached_value.clone());
        }
    }

    let url = format!("./data/{}.json", conf_file);
    let arrangement = Request::get(&url)
        .send()
        .await
        .ok()?
        .json::<ArrangementConf>()
        .await
        .ok()
        .map(|conf| geometry::generate_arrangement(&conf, get_file_name(&conf_file)))?;

    let mut lock = cache.lock().unwrap();
    lock.insert(conf_file, arrangement.clone());

    Some(arrangement)
}

pub async fn fetch_index() -> Option<Vec<Series>> {
    Request::get("./data/index.json")
        .send()
        .await
        .ok()?
        .json::<Vec<Series>>()
        .await
        .ok()
}

pub async fn fetch_list(n: String) -> Option<Vec<String>> {
    let url = format!("./data/{}.json", n);
    Request::get(&url)
        .send()
        .await
        .ok()?
        .json::<Vec<String>>()
        .await
        .ok()
}

#[derive(Clone, Debug, PartialEq, Serialize, Deserialize)]
pub struct Series {
    pub id: String,
    #[serde(rename = "d")]
    pub description: String,
    #[serde(rename = "k")]
    pub solves_kobon: bool,
    #[serde(rename = "a")]
    pub solves_arnold: bool,
}

impl Series {
    pub fn solves(&self) -> Vec<&str> {
        let mut res: Vec<&str> = Vec::new();
        if self.solves_kobon {
            res.push("Kobon");
        }
        if self.solves_arnold {
            res.push("Arnold");
        }
        res
    }
}

#[derive(Clone, Copy)]
pub struct ConfigurationsSelectorState {
    pub selected_list_name: RwSignal<String>,
    pub selected_conf: RwSignal<String>,
    pub all_confs: LocalResource<Vec<String>>,

    pub filter: RwSignal<String>,
    pub is_dropdown_open: RwSignal<bool>,
    pub active_list_name: RwSignal<String>,
    pub show_all: RwSignal<bool>, // Show all Series without filter
    pub filtered_series: Memo<Vec<Series>>,
    index: LocalResource<Vec<Series>>,

    pub input_ref: NodeRef<Input>,
}

impl ConfigurationsSelectorState {
    pub fn new() -> Self {
        let (selected_list, selected_conf) = get_params_from_url();
        let selected_list_name = RwSignal::new(selected_list.clone());
        let selected_conf = RwSignal::new(selected_conf);

        let all_confs = LocalResource::new(move || {
            let list_name = selected_list_name.get();
            async move {
                if list_name.is_empty() {
                    Vec::new()
                } else {
                    fetch_list(list_name).await.unwrap_or_default()
                }
            }
        });

        let filter = RwSignal::new(selected_list);
        let is_dropdown_open = RwSignal::new(false);
        let active_list_name = RwSignal::new(String::new());
        let show_all = RwSignal::new(true);
        let index = LocalResource::new(async move || fetch_index().await.unwrap_or_default());

        let filtered_series = Memo::new(move |_| {
            let q = filter.get().trim().to_lowercase();
            let index = index.get().unwrap_or(vec![]);
            if q.is_empty() || show_all.get() {
                return index;
            }
            index.into_iter().filter(|item| {
                item.id.to_lowercase().starts_with(&q)
                    || item.description.to_lowercase().contains(&q)
                    || item.solves_kobon && "kobon".starts_with(&q)
                    || item.solves_kobon && "arnold".starts_with(&q)
            }).collect()
        });

        let input_ref = NodeRef::<Input>::new();

        Self {
            selected_list_name,
            selected_conf,
            all_confs,
            filter,
            is_dropdown_open,
            active_list_name,
            show_all,
            filtered_series,
            index,
            input_ref,
        }
    }

    pub fn move_to_main_page(&self) {
        self.apply_route(String::new(), String::new());
    }

    fn index_of(&self, id: &str) -> Option<usize> {
        self.filtered_series.get().iter().enumerate()
            .filter(|(_, item)| item.id == id)
            .map(|(i, _)| i)
            .next()
    }

    fn move_active(&self, next: bool) {
        let len = self.filtered_series.get().len();
        if len > 0 {
            let index = self.index_of(self.active_list_name.get().as_str());
            if let Some(mut index) = index {
                if next {
                    index = (index + 1) % len;
                }
                else {
                    index = (index + len - 1) % len;
                }
                self.active_list_name.set(self.filtered_series.get()[index].id.clone());
            }
            else {
                self.active_list_name.set(self.filtered_series.get()[0].id.clone());
            }
        }
    }

    pub fn close_dropdown_and_restore(&self) {
        self.filter.set(self.selected_list_name.get());
        self.hide_dropdown_and_blur();
    }

    pub fn select_series(&self, id: String) {
        self.selected_conf.set(String::new());
        self.selected_list_name.set(id.clone());
        self.filter.set(id.clone());
        self.hide_dropdown_and_blur();
        update_url(&id, "");
    }

    pub fn apply_route(&self, list_name: String, conf_name: String) {
        if self.selected_list_name.get() != list_name {
            self.selected_list_name.set(list_name.clone());
        }
        if self.selected_conf.get() != conf_name {
            self.selected_conf.set(conf_name);
        }
        self.filter.set(list_name.clone());
        self.active_list_name.set(list_name);
        self.is_dropdown_open.set(false);
    }

    fn hide_dropdown_and_blur(&self) {
        self.is_dropdown_open.set(false);
        if let Some(target) = self.input_ref.get() {
            let _ = target.blur();
        }
    }

    pub fn open_dropdown(&self, show_all: bool) {
        self.is_dropdown_open.set(true);
        self.show_all.set(show_all);
        self.active_list_name.set(self.selected_list_name.get().clone());
    }

    pub fn handle_input(&self, ev: Event) {
        self.filter.set(event_target_value(&ev));
        self.show_all.set(false);
        self.is_dropdown_open.set(true);
        let filtered_series = self.filtered_series.get();
        let old_id = self.selected_list_name.get();
        let contains_old_value = filtered_series.iter().any(|item| {item.id == old_id});
        if !contains_old_value {
            self.active_list_name.set(if filtered_series.is_empty() {
                String::new()
            } else {
                filtered_series[0].id.clone()
            });
        }
    }

    pub fn series_description(&self) -> String {
        let filter = self.filter.get();
        self.index.get().unwrap_or_default()
            .iter().filter(|item| item.id == filter)
            .next()
            .map(|item| format!(" — {} — {}", item.description.clone(), item.solves().join(" + ")))
            .unwrap_or_default()
    }

    pub fn handle_keydown(&self, ev: KeyboardEvent) {
        match ev.key().as_str() {
            "Tab" => {
                self.hide_dropdown_and_blur();
                self.filter.set(self.selected_list_name.get().clone());
            }
            "ArrowDown" => {
                ev.prevent_default();
                if !self.is_dropdown_open.get() {
                    self.open_dropdown(true);
                }
                else {
                    self.move_active(true);
                }
            }
            "ArrowUp" => {
                ev.prevent_default();
                if !self.is_dropdown_open.get() {
                    self.open_dropdown(true);
                }
                else {
                    self.move_active(false);
                }
            }
            "Enter" => {
                ev.prevent_default();
                if !self.is_dropdown_open.get() {
                    self.open_dropdown(false);
                }
                else {
                    let active_list_name = self.active_list_name.get();
                    if !active_list_name.is_empty() {
                        self.select_series(active_list_name);
                    }
                }
            }
            "Escape" => {
                ev.prevent_default();
                if self.is_dropdown_open.get() {
                    self.is_dropdown_open.set(false);
                    self.filter.set(self.selected_list_name.get().clone());
                }
            }
            _ => {}
        }
    }
}

fn parse_hash(hash: &str) -> (String, String) {
    let hash = hash.strip_prefix('#').unwrap_or(hash);

    if let Some((list_name, conf_name)) = hash.split_once('/') {
        (list_name.to_string(), conf_name.to_string())
    } else {
        (hash.to_string(), String::new())
    }
}

fn route_hash(list_name: &str, conf_name: &str) -> String {
    if conf_name.is_empty() {
        list_name.to_string()
    } else {
        format!("{}/{}", list_name, conf_name)
    }
}

pub fn get_params_from_url() -> (String, String) {
    if let Some(raw_hash) = web_sys::window().and_then(|w| w.location().hash().ok()) {
        let raw_hash = raw_hash.strip_prefix('#').unwrap_or(&raw_hash);
        if let Ok(js_string) = js_sys::decode_uri_component(raw_hash) {
            let hash: String = String::from(js_string);
            return parse_hash(&hash);
        }
    }
    (String::new(), String::new())
}

pub fn update_url(list_name: &str, conf_idx: &str) {
    if let Some(window) = web_sys::window() {
        let new_hash = route_hash(list_name, conf_idx);

        let location = window.location();
        let current_hash = location.hash().unwrap_or_default();
        if current_hash.strip_prefix('#').unwrap_or(&current_hash) != new_hash.as_str() {
            if let Err(e) = location.set_hash(&new_hash) {
                log!("Failed to update URL: {:?}", e);
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::{parse_hash, route_hash};

    #[test]
    fn parses_empty_hash() {
        assert_eq!(parse_hash(""), (String::new(), String::new()));
        assert_eq!(parse_hash("#"), (String::new(), String::new()));
    }

    #[test]
    fn parses_series_route() {
        assert_eq!(parse_hash("#14"), ("14".to_string(), String::new()));
        assert_eq!(parse_hash("#14/"), ("14".to_string(), String::new()));
    }

    #[test]
    fn keeps_slashes_in_configuration_name() {
        assert_eq!(
            parse_hash("#14/14/14-example"),
            ("14".to_string(), "14/14-example".to_string())
        );
    }

    #[test]
    fn formats_routes_without_a_trailing_slash() {
        assert_eq!(route_hash("14", ""), "14");
        assert_eq!(route_hash("14", "14/14-example"), "14/14/14-example");
    }
}

use leptos::{component, IntoView};
use leptos::html::{Button};
use leptos::prelude::*;
use leptos_use::use_intersection_observer;
use wasm_bindgen::JsCast;
use web_sys::{MouseEvent, PointerEvent};
use crate::geometry::{get_arnold_diff, solution_for_names, Arrangement};
use crate::interaction;
use crate::interaction::{ArrangementState, ConfigurationsSelectorState};

#[component]
pub fn ArrangementView(state: ArrangementState, arrangement: Arrangement, #[prop(into)] active: Signal<bool>) -> impl IntoView {
    let polygons_view = arrangement.polygons.iter().map(|polygon| {
        let pts = polygon.points.clone();

        view! {
            <polygon
                points=move || {
                    let s = state.scale.get();
                    let shift_x = state.shift.get().x * arrangement.w;
                    let shift_y = state.shift.get().y * arrangement.h;

                    pts.iter()
                        .map(|&pt| format!("{},{}", pt.x * s + shift_x , -pt.y * s + shift_y))
                        .collect::<Vec<String>>()
                        .join(" ")
                }
                class=format!("{} {} s{}", if polygon.b {"b"} else {"w"}, if polygon.ext {"ext"} else {"int"}, polygon.sz)
            />
        }
    }).collect_view();

    view! {
        <svg class="arrangement"
            viewBox=format!("{} {} {} {}", -arrangement.w*0.5, -arrangement.h*0.5, arrangement.w, arrangement.h)
            style="touch-action: none; overscroll-behavior: none;"
            preserveAspectRatio="xMidYMid slice"

            on:wheel=move |ev| { if active.get() { state.on_wheel(ev); } }
            on:pointerdown=move |ev| { if active.get() { state.on_pointer_down(ev); } }
            on:pointermove=move |ev| { if active.get() { state.on_pointer_move(ev); } }
            on:pointerup=move |ev| { if active.get() { state.on_pointer_up(ev); } }

            on:touchstart=move |ev| { if active.get() { state.on_touch_start(ev); } }
            on:touchmove=move |ev| { if active.get() { state.on_touch_move(ev); } }
            on:touchend=move |ev| { if active.get() { state.on_touch_end(ev); } }
            on:touchcancel=move |ev| { if active.get() { state.on_touch_end(ev); } }
        >
            {polygons_view}
        </svg>
    }
}

#[component]
pub fn GalleryItem(
    #[prop(into)] config_file: Signal<String>,
    #[prop(into)] on_click: Callback<MouseEvent>,
    #[prop(into)] num: Signal<usize>,
    #[prop(into)] count: Signal<usize>,
) -> impl IntoView {
    let node_ref = NodeRef::<Button>::new();
    let (is_visible, set_visible) = signal(false);

    use_intersection_observer(
        node_ref,
        move |entries, _| {
            set_visible.set(entries[0].is_intersecting());
        },
    );

    view! {
        <button class="gallery-item"
                node_ref=node_ref
                on:click=move |e| on_click.run(e)>
            <Show
                when=move || is_visible.get()
            >
                <ArrangementViewWithLoad
                    config_file=config_file
                    active=false
                />
                <span class="item-index">{format!("{}/{}", num.get() + 1, count.get())}</span>
            </Show>
        </button>
    }
}

#[component]
pub fn ArrangementViewWithLoad(
    #[prop(into)] config_file: Signal<String>,
    #[prop(into)] active: Signal<bool>,
) -> impl IntoView {
    let arrangement = LocalResource::new(move || {
        let file = config_file.get();
        async move {
            if file.is_empty() {
                None
            } else {
                interaction::fetch_arrangement_config(file).await
            }
        }
    });

    view! {
        <ArrangementViewWithResource
            arrangement_resource=arrangement
            active=active
        />
    }
}

#[component]
pub fn ArrangementViewWithResource(
    arrangement_resource: LocalResource<Option<Arrangement>>,
    #[prop(into)] active: Signal<bool>,
) -> impl IntoView {
    view! {
        <Transition fallback=move || view! {}>
            {move || {
                arrangement_resource.with(|maybe_res| {
                    maybe_res
                        .as_ref()
                        .and_then(|opt| opt.as_ref())
                        .map(|conf| {
                            let state = ArrangementState::default();

                            view! {
                                <ArrangementView state=state arrangement=conf.clone() active=active/>
                            }
                        })
                })
            }}
        </Transition>
    }
}

#[component]
pub fn ArrangementDetails(
    arrangement_resource: LocalResource<Option<Arrangement>>
) -> impl IntoView {
    view! {
        <Transition fallback=move || view! {}>
            {move || {
                arrangement_resource.with(|maybe_res| {
                    maybe_res
                        .as_ref()
                        .and_then(|opt| opt.as_ref())
                        .map(|conf| {
                            let triple_points = conf.triple_points;
                            let triple_points_visible = triple_points > 0;
                            let parallel_pairs = conf.parallel_pairs;
                            let parallel_pairs_visible = parallel_pairs > 0;

                            view! {
                                <div class="info-panel">
                                    <dl class="info-properties">
                                        <div class="property-row">
                                            <dt>"Number of lines:"</dt>
                                            <dd>{format!("{}", conf.n)}</dd>
                                        </div>

                                        <div class="property-row">
                                            <dt>"Triangles:"</dt>
                                            <dd>{format!("{}", conf.tri_count)}</dd>
                                        </div>

                                        <Show when=move || triple_points_visible >
                                            <div class="property-row">
                                                <dt>"Triple points:"</dt>
                                                <dd>{format!("{}", triple_points)}</dd>
                                            </div>
                                        </Show>

                                        <Show when=move || parallel_pairs_visible >
                                            <div class="property-row">
                                                <dt>"Parallel pairs:"</dt>
                                                <dd>{format!("{}", parallel_pairs)}</dd>
                                            </div>
                                        </Show>

                                        <div class="property-row">
                                            <dt>"Black − white:"</dt>
                                            <dd>{format!("{}", get_arnold_diff(conf))}</dd>
                                        </div>

                                        <div class="property-row">
                                            <dt>"Solution for:"</dt>
                                            <dd>{solution_for_names(conf).to_string()}</dd>
                                        </div>

                                        <div class="property-row">
                                            <dt>"ID:"</dt>
                                            <dd>{format!("{}", conf.name)}</dd>
                                        </div>

                                        <div class="property-row generators">
                                            <dt>"Generators:"</dt>
                                            <dd>{format!("{}", conf.generators_string)}</dd>
                                        </div>
                                    </dl>
                                </div>
                            }
                        })
                })
            }}
        </Transition>
    }
}

#[component]
pub fn MiniBadge(name: String) -> impl IntoView {
    view! {
        <span class=format!("mini-badge {}", &name.to_lowercase())>{name.clone()}</span>
    }
}

#[component]
pub fn ConfigurationsSelector(state: ConfigurationsSelectorState) -> impl IntoView {
    view! {
        <div class="combobox-wrap" on:focusout=move |_| state.close_dropdown_and_restore()>
            <div class="series-control" class:open=state.is_dropdown_open id="series-control"
                on:pointerdown=move |e: PointerEvent| {
                    if let (Some(target), Some(input)) = (e.target(), state.input_ref.get()) {
                        if let Some(target_node) = target.dyn_ref::<web_sys::Node>() {
                            if target_node.clone() != input.clone().into() {
                                let _ = input.focus();
                            }
                        }
                    }
                }>
                <label class="sr-only" for="series-input">"Series"</label>
                <svg class="search-icon" viewBox="0 0 20 20" aria-hidden="true">
                    <circle cx="8.5" cy="8.5" r="5.5" fill="none" stroke="currentColor" stroke-width="1.7"/>
                    <path d="m12.5 12.5 4 4" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round"/>
                </svg>
                <div class="sr-div">
                    <div class="sr-back">
                        <span class="sr-back-hidden">{move || state.filter.get()}</span>
                        <span class="sr-back-visible">{move || state.series_description()}</span>
                    </div>
                    <input id="series-input"
                        node_ref=state.input_ref
                        type="text"
                        maxlength="10"
                        prop:value=move || state.filter.get()
                        on:input=move |e| state.handle_input(e)
                        on:keydown=move |e| state.handle_keydown(e)
                        on:focus=move |_| {
                            state.open_dropdown(true);
                            if let Some(target) = state.input_ref.get() {
                                // select all text
                                request_animation_frame(move || {
                                    target.select();
                                });
                            }
                        }
                        inputmode="text"
                        autocomplete="off"
                        autocapitalize="off"
                        spellcheck="false"
                        role="combobox"
                        aria-autocomplete="list"
                        aria-expanded=move || state.is_dropdown_open.get().to_string()
                        aria-controls="series-options"
                    />
                </div>
                <svg class="chevron" viewBox="0 0 20 20" aria-hidden="true">
                    <path d="m5 7.5 5 5 5-5" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"/>
                </svg>
            </div>

            <section class="series-dropdown" id="series-dropdown" hidden=move || !state.is_dropdown_open.get()>
                <div class="dropdown-heading">
                    <strong>"Available series"</strong>
                    <span id="result-count">
                        {move || {
                            let len = state.filtered_series.get().len();
                            format!("{} {}", len, if len == 1 { "variant" } else { "variants" })
                        }}
                    </span>
                </div>
                <div class="option-grid" role="listbox" aria-label="Available series">
                    <For each=move || state.filtered_series.get()
                        key=|item| item.id.clone()
                        children=move |item| {
                            let item_id1 = item.id.clone();
                            let item_id2 = item_id1.clone();
                            let item_id3 = item_id1.clone();
                            view! {
                                <button
                                    type="button"
                                    class=move || {
                                        let is_selected = state.selected_list_name.get() == item_id1;
                                        let is_active = state.active_list_name.get() == item_id1;

                                        format!(
                                            "series-option{}{}",
                                            if is_selected { " selected" } else { "" },
                                            if is_active { " active" } else { "" }
                                        )
                }
                                    tabindex="-1"
                                    role="option"
                                    aria-selected=move || (state.selected_list_name.get() == item_id2).to_string()
                                    aria-label=move || format!("Series {}: {}, solution for {}", item.id, item.description, item.solves().join(" and "))
                                    on:mousedown=|ev| ev.prevent_default() // prevent focusout
                                    on:click=move |_| state.select_series(item_id3.clone())
                                >
                                    <strong class="option-code">{item.id.clone()}</strong>
                                    <span class="option-detail">{format!("{}", item.description)}</span>
                                    <span class="option-badges">
                                        <Show when=move || item.solves_kobon>
                                            <MiniBadge name="Kobon".to_string()/>
                                        </Show>
                                        <Show when=move || item.solves_arnold>
                                            <MiniBadge name="Arnold".to_string()/>
                                        </Show>
                                    </span>
                                </button>
                            }
                        }
                    />
                </div>
                <p class="empty-state" hidden=move || !state.filtered_series.get().is_empty()>"No matching series"</p>
                <p class="dropdown-hint">
                    "Type a number · " <kbd>"Enter"</kbd> " chooses the first match · " <kbd>"↑"</kbd><kbd>"↓"</kbd> " navigate"
                </p>
            </section>
        </div>
    }
}

// https://heroicons.com/
#[component]
pub fn DownloadIcon() -> impl IntoView {
    view! {
        <svg fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="white">
          <path stroke-linecap="round" stroke-linejoin="round" d="M3 16.5v2.25A2.25 2.25 0 0 0 5.25 21h13.5A2.25 2.25 0 0 0 21 18.75V16.5M16.5 12 12 16.5m0 0L7.5 12m4.5 4.5V3" />
        </svg>
    }
}

#[component]
pub fn CloseIcon() -> impl IntoView {
    view! {
        <svg fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="white">
          <path stroke-linecap="round" stroke-linejoin="round" d="M6 18 18 6M6 6l12 12" />
        </svg>
    }
}

#[component]
pub fn InformationCircleIcon() -> impl IntoView {
    view! {
        <svg fill="none" viewBox="0 0 24 24" stroke-width="1.5" stroke="white">
          <path stroke-linecap="round" stroke-linejoin="round" d="m11.25 11.25.041-.02a.75.75 0 0 1 1.063.852l-.708 2.836a.75.75 0 0 0 1.063.853l.041-.021M21 12a9 9 0 1 1-18 0 9 9 0 0 1 18 0Zm-9-3.75h.008v.008H12V8.25Z" />
        </svg>
    }
}

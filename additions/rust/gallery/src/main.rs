mod geometry;
mod interaction;
mod ui;

use crate::ui::{
    ArrangementDetails, ArrangementViewWithResource, CloseIcon, ConfigurationsSelector,
    DownloadIcon, VirtualGallery, InformationCircleIcon,
};
use interaction::ConfigurationsSelectorState;
use leptos::ev;
use leptos::prelude::*;
use web_sys::KeyboardEvent;

#[component]
fn Brand(state: ConfigurationsSelectorState) -> impl IntoView {
    view! {
        <a on:click=move |e| {
            state.move_to_main_page();
            e.prevent_default();
        } class="brand" aria-label="Kobon and Arnold gallery" href="#">
            <span class="brand-mark" aria-hidden="true">{"K+A"}</span>
            <span class="brand-copy">
                <strong>{"Solution gallery"}</strong>
                <small>{"Kobon & Arnold"}</small>
            </span>
        </a>
    }
}

#[component]
fn GitHubLink() -> impl IntoView {
    view! {
        <div class="github-link">
            <a href="https://github.com/ud1/kobon-solutions">
                <img src="images/github_logo.svg" alt="GitHub"/>
            </a>
        </div>
    }
}

fn main() {
    console_error_panic_hook::set_once();

    mount_to_body(move || {
        let selector_state = ConfigurationsSelectorState::new();
        let selected_conf = selector_state.selected_conf;
        let info_opened = RwSignal::new(false);

        let close_lightbox = move || {
            if !selected_conf.get().is_empty() {
                selected_conf.set(String::new());
                interaction::update_url(&selector_state.selected_list_name.get(), "");
            }
        };

        let open_lightbox = move |conf: String| {
            selected_conf.set(conf.clone());
            interaction::update_url(&selector_state.selected_list_name.get(), &conf);
        };

        let _ = window_event_listener(ev::keydown, move |ev: KeyboardEvent| {
            if ev.key() == "Escape" && !selected_conf.get().is_empty() {
                close_lightbox();
            }
        });

        let _ = window_event_listener(ev::hashchange, move |_| {
            let (list_name, conf_name) = interaction::get_params_from_url();
            selector_state.apply_route(list_name, conf_name);
        });

        let on_click_close = move |_ev| close_lightbox();
        let lightbox_arrangement = LocalResource::new(move || {
            let file = selected_conf.get();
            async move {
                if file.is_empty() {
                    None
                } else {
                    interaction::fetch_arrangement_config(file).await
                }
            }
        });
        let on_click_download = move |_ev| {
            if let Some(Some(arr)) = lightbox_arrangement.get() {
                interaction::download_arrangement(&arr);
            }
        };
        let on_click_toggle_info = move |_ev| {
            info_opened.update(|v| *v = !*v);
        };

        view! {
            <header>
                <Brand state=selector_state.clone()/>
                <ConfigurationsSelector state=selector_state.clone() />
                <GitHubLink/>
            </header>

            <VirtualGallery state=selector_state on_open=open_lightbox/>

            <div class="lightbox" class=("show", move || !selected_conf.get().is_empty())>
                <Show when=move || !selected_conf.get().is_empty()>
                    <ArrangementViewWithResource
                        arrangement_resource=lightbox_arrangement
                        active=move || !selected_conf.get().is_empty()
                    />
                    <div class="header-actions">
                        <span class="icon" on:click=on_click_toggle_info><InformationCircleIcon/></span>
                        <span class="icon" on:click=on_click_download><DownloadIcon/></span>
                        <span class="icon" on:click=on_click_close><CloseIcon/></span>
                    </div>

                    <Show when=move || info_opened.get()>
                        <ArrangementDetails arrangement_resource=lightbox_arrangement/>
                    </Show>
                </Show>
            </div>

            <footer class="center-container">
                <span>{"Solutions to the Kobon and Arnold problems. Denis Utkin and Roman Parpalak, 2026"}</span>
            </footer>
        }
    })
}

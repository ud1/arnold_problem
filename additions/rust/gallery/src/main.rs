mod geometry;
mod interaction;
mod ui;

use leptos::prelude::*;
use web_sys::KeyboardEvent;
use leptos::ev;
use interaction::{ConfigurationsSelectorState, SelectedConf};
use crate::ui::{ArrangementDetails, ArrangementViewWithResource, CloseIcon, ConfigurationsSelector, DownloadIcon, InformationCircleIcon, GalleryItem};

#[component]
fn Brand() -> impl IntoView {
    view! {
        <div class="brand" aria-label="Kobon and Arnold gallery">
            <span class="brand-mark" aria-hidden="true">{"K+A"}</span>
            <span class="brand-copy">
                <strong>{"Solution gallery"}</strong>
                <small>{"Kobon & Arnold"}</small>
            </span>
        </div>
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
        let selected_conf_id = selector_state.selected_conf.get_untracked();
        let selected_conf = RwSignal::new(if selected_conf_id.is_empty() {SelectedConf::None} else {SelectedConf::Visible(selected_conf_id)});
        let info_opened = RwSignal::new(false);

        let close_lightbox = move || {
            if selected_conf.get().is_visible() {
                selected_conf.update(|v| v.hide());
                selector_state.selected_conf.set("".to_string());
            }
        };

        let open_lightbox = move |conf: String| {
            selected_conf.set(SelectedConf::Visible(conf.clone()));
            selector_state.selected_conf.set(conf);
        };

        let _ = window_event_listener(ev::keydown, move |ev: KeyboardEvent| {
            if ev.key() == "Escape" && selected_conf.get().is_visible() {
                close_lightbox();
            }
        });

        let on_click_close = move |_ev| close_lightbox();
        let lightbox_arrangement = LocalResource::new(move || {
            let file = match selected_conf.get() {
                SelectedConf::Visible(name) | SelectedConf::Hidden(name) => name,
                SelectedConf::None => String::new(),
            };
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
                <Brand/>
                <ConfigurationsSelector state=selector_state.clone() />
                <GitHubLink/>
            </header>

            <div class="gallery-wrapper">
                <div class="gallery-container">
                    <ForEnumerate
                        each=move || selector_state.all_configurations()
                        key=|conf_name| conf_name.clone()
                        children={move |index: ReadSignal<usize>, conf_name: String| {
                            let conf_name_clone = conf_name.clone();
                            view! {
                                <GalleryItem
                                    num=index
                                    count=selector_state.all_configurations().len()
                                    on_click=move |_ev| open_lightbox(conf_name_clone.clone())
                                    config_file=conf_name.clone() />
                            }
                        }}
                    />
                </div>
            </div>

            <div class="lightbox" class=("show", move || selected_conf.get().is_visible())>
                <Show when=move || selected_conf.get().is_visible()>
                    <ArrangementViewWithResource
                        arrangement_resource=lightbox_arrangement
                        active=move || selected_conf.get().is_visible()
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

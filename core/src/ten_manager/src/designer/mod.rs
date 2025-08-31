//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
pub mod apps;
pub mod builtin_function;
pub mod common;
pub mod dir_list;
pub mod doc_link;
pub mod env;
pub mod env_var;
pub mod exec;
pub mod extensions;
pub mod file_content;
pub mod frontend;
pub mod graphs;
pub mod help_text;
pub mod locale;
pub mod log_watcher;
pub mod manifest;
pub mod messages;
pub mod preferences;
pub mod property;
pub mod registry;
pub mod response;
pub mod storage;
pub mod template_pkgs;
pub mod terminal;
pub mod version;

use std::{collections::HashMap, sync::Arc};

use actix_web::web;
use jsonschema::Validator;
use storage::in_memory::TmanStorageInMemory;
use ten_rust::{base_dir_pkg_info::PkgsInfoInApp, graph::graph_info::GraphInfo};
use uuid::Uuid;

use crate::{home::config::TmanConfig, output::TmanOutput};

pub struct DesignerState {
    pub tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
    pub storage_in_memory: Arc<tokio::sync::RwLock<TmanStorageInMemory>>,
    pub out: Arc<Box<dyn TmanOutput>>,
    pub pkgs_cache: tokio::sync::RwLock<HashMap<String, PkgsInfoInApp>>,
    pub graphs_cache: tokio::sync::RwLock<HashMap<Uuid, GraphInfo>>,
    pub persistent_storage_schema: Arc<tokio::sync::RwLock<Option<Validator>>>,
}

pub fn configure_routes(cfg: &mut web::ServiceConfig, state: web::Data<Arc<DesignerState>>) {
    cfg.service(
        web::scope("/api/designer/v1")
            .app_data(state)
            // Version endpoints.
            .service(web::resource("/version").route(web::get().to(version::get_version_endpoint)))
            .service(web::resource("/check-update").route(web::get().to(version::check_update_endpoint)))
            // Apps endpoints.
            .service(
                web::scope("/apps")
                    .service(web::resource("").route(web::get().to(apps::get::get_apps_endpoint)))
                    .service(web::resource("/load").route(web::post().to(apps::load::load_app_endpoint)))
                    .service(web::resource("/unload").route(web::post().to(apps::unload::unload_app_endpoint)))
                    .service(web::resource("/reload").route(web::post().to(apps::reload::reload_app_endpoint)))
                    .service(web::resource("/create").route(web::post().to(apps::create::create_app_endpoint)))
                    .service(web::resource("/addons").route(web::post().to(apps::addons::get_app_addons_endpoint)))
                    .service(web::resource("/scripts").route(web::post().to(apps::scripts::get_app_scripts_endpoint)))
                    .service(web::resource("/schema").route(web::post().to(apps::schema::get_app_schema_endpoint)))
            )
            // Extension endpoints.
            .service(
                web::scope("/extensions")
                    .service(web::resource("/create").route(web::post().to(extensions::create::create_extension_endpoint)))
                    .service(web::resource("/schema").route(web::post().to(extensions::schema::get_extension_schema_endpoint)))
                    .service(
                        web::scope("/property")
                            .service(web::resource("/get").route(web::post().to(extensions::property::get_extension_property_endpoint)))
                    )
            )
            // Manifest validation endpoints.
            .service(
                web::scope("/manifest")
                    .service(web::resource("/validate").route(web::post().to(manifest::validate::validate_manifest_endpoint)))
            )
            // Property validation endpoints.
            .service(
                web::scope("/property")
                    .service(web::resource("/validate").route(web::post().to(property::validate::validate_property_endpoint)))
            )
            // Template packages endpoint.
            .service(web::resource("/template-pkgs").route(web::post().to(template_pkgs::get_template_endpoint)))
            // Graphs endpoints.
            .service(
                web::scope("/graphs")
                    .service(web::resource("").route(web::post().to(graphs::get::get_graphs_endpoint)))
                    .service(web::resource("/update").route(web::post().to(graphs::update::update_graph_endpoint)))
                    .service(web::resource("/auto-start").route(web::post().to(graphs::auto_start::update_graph_auto_start_endpoint)))
                    .service(
                        web::scope("/nodes")
                            .service(web::resource("/add").route(web::post().to(graphs::nodes::add::add_graph_node_endpoint)))
                            .service(web::resource("/delete").route(web::post().to(graphs::nodes::delete::delete_graph_node_endpoint)))
                            .service(web::resource("/replace").route(web::post().to(graphs::nodes::replace::replace_graph_node_endpoint)))
                            .service(
                                web::scope("/property")
                                    .service(web::resource("/update").route(web::post().to(graphs::nodes::property::update::update_graph_node_property_endpoint)))
                            )
                    )
                    .service(
                        web::scope("/connections")
                            .service(web::resource("/add").route(web::post().to(graphs::connections::add::add_graph_connection_endpoint)))
                            .service(web::resource("/delete").route(web::post().to(graphs::connections::delete::delete_graph_connection_endpoint)))
                            .service(
                                web::scope("/msg_conversion")
                                    .service(web::resource("/update").route(web::post().to(graphs::connections::msg_conversion::update::update_graph_connection_msg_conversion_endpoint)))
                            )
                    )
            )
            // Messages endpoints.
            .service(
                web::scope("/messages")
                    .service(web::resource("/compatible").route(web::post().to(messages::compatible::get_compatible_messages_endpoint)))
            )
            // Preferences endpoints.
            .service(
                web::scope("/preferences")
                    .service(
                        web::resource("/logviewer_line_size")
                            .route(web::get().to(preferences::logviewer_line_size::get_logviewer_line_size_endpoint))
                            .route(web::put().to(preferences::logviewer_line_size::update_logviewer_line_size_endpoint))
                    )
                    .service(
                        web::resource("/locale")
                            .route(web::get().to(preferences::locale::get_locale_endpoint))
                            .route(web::put().to(preferences::locale::update_locale_endpoint))
                    )
            )
            // Storage (in-memory) endpoints.
            .service(
                web::scope("/storage")
                    .service(
                        web::scope("/in-memory")
                            .service(web::resource("/set").route(web::post().to(storage::in_memory::set::set_in_memory_storage_endpoint)))
                            .service(web::resource("/get").route(web::post().to(storage::in_memory::get::get_in_memory_storage_endpoint)))
                    )
                    .service(
                        web::scope("/persistent")
                            .service(web::resource("/set").route(web::post().to(storage::persistent::set::set_persistent_storage_endpoint)))
                            .service(web::resource("/get").route(web::post().to(storage::persistent::get::get_persistent_storage_endpoint)))
                            .service(web::resource("/schema").route(web::post().to(storage::persistent::schema::set_persistent_storage_schema_endpoint)))
                    )
            )
            // File system endpoints.
            .service(web::resource("/dir-list").route(web::post().to(dir_list::list_dir_endpoint)))
            .service(
                web::resource("/file-content")
                    .route(web::post().to(file_content::get_file_content_endpoint))
                    .route(web::put().to(file_content::save_file_content_endpoint))
            )
            // Websocket endpoints.
            .service(
                web::scope("/ws")
                    .service(web::resource("/builtin-function").route(web::get().to(builtin_function::builtin_function_endpoint)))
                    .service(web::resource("/exec").route(web::get().to(exec::exec_endpoint)))
                    .service(web::resource("/terminal").route(web::get().to(terminal::ws_terminal_endpoint)))
                    .service(web::resource("/log-watcher").route(web::get().to(log_watcher::log_watcher_endpoint)))
            )
            // Doc endpoints.
            .service(web::resource("/help-text").route(web::post().to(help_text::get_help_text_endpoint)))
            .service(web::resource("/doc-link").route(web::post().to(doc_link::get_doc_link_endpoint)))
            // Registry endpoints.
            .service(
                web::scope("/registry")
                    .service(
                        web::scope("/packages")
                            .service(web::resource("").route(web::get().to(registry::packages::get_packages_endpoint)))
                            .service(web::resource("/search").route(web::post().to(registry::search::search_packages_endpoint)))
                    )
            )
            // Environment endpoints.
            .service(web::resource("/env").route(web::get().to(env::get_env_endpoint)))
            .service(web::resource("/env-var").route(web::post().to(env_var::get_env_var_endpoint))),
    );
}

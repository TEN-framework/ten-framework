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
pub mod metadata;
pub mod preferences;
pub mod property;
pub mod registry;
pub mod response;
pub mod template_pkgs;
pub mod terminal;
pub mod version;

use std::{collections::HashMap, sync::Arc};

use actix_web::web;
use uuid::Uuid;

use ten_rust::{
    base_dir_pkg_info::PkgsInfoInApp, graph::graph_info::GraphInfo,
};

use crate::config::{metadata::TmanMetadata, TmanConfig};
use crate::output::TmanOutput;

pub struct DesignerState {
    pub tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
    pub tman_metadata: Arc<tokio::sync::RwLock<TmanMetadata>>,
    pub out: Arc<Box<dyn TmanOutput>>,
    pub pkgs_cache: tokio::sync::RwLock<HashMap<String, PkgsInfoInApp>>,
    pub graphs_cache: tokio::sync::RwLock<HashMap<Uuid, GraphInfo>>,
}

pub fn configure_routes(
    cfg: &mut web::ServiceConfig,
    state: web::Data<Arc<DesignerState>>,
) {
    cfg.service(
        web::scope("/api/designer/v1")
            .app_data(state)
            // Version endpoints.
            .service(web::resource("/version").route(web::get().to(version::get_version_endpoint)))
            .service(web::resource("/check-update").route(web::get().to(version::check_update_endpoint)))
            // Apps endpoints.
            .service(web::resource("/apps").route(web::get().to(apps::get::get_apps_endpoint)))
            .service(web::resource("/apps/load").route(web::post().to(apps::load::load_app_endpoint)))
            .service(web::resource("/apps/unload").route(web::post().to(apps::unload::unload_app_endpoint)))
            .service(web::resource("/apps/reload").route(web::post().to(apps::reload::reload_app_endpoint)))
            .service(web::resource("/apps/create").route(web::post().to(apps::create::create_app_endpoint)))
            .service(web::resource("/apps/addons").route(web::post().to(apps::addons::get_app_addons_endpoint)))
            .service(web::resource("/apps/scripts").route(web::post().to(apps::scripts::get_app_scripts_endpoint)))
            .service(web::resource("/apps/schema").route(web::post().to(apps::schema::get_app_schema_endpoint)))
            // Extension endpoints.
            .service(web::resource("/extensions/create").route(web::post().to(extensions::create::create_extension_endpoint)))
            .service(web::resource("/extensions/schema").route(web::post().to(extensions::schema::get_extension_schema_endpoint)))
            .service(web::resource("/extensions/property/get").route(web::post().to(extensions::property::get_extension_property_endpoint)))
            // Manifest validation endpoints.
            .service(web::resource("/manifest/validate").route(web::post().to(manifest::validate::validate_manifest_endpoint)))
            // Property validation endpoints.
            .service(web::resource("/property/validate").route(web::post().to(property::validate::validate_property_endpoint)))
            // Template packages endpoint.
            .service(web::resource("/template-pkgs").route(web::post().to(template_pkgs::get_template_endpoint)))
            // Graphs endpoints.
            .service(web::resource("/graphs").route(web::post().to(graphs::get::get_graphs_endpoint)))
            .service(web::resource("/graphs/update").route(web::post().to(graphs::update::update_graph_endpoint)))
            // Graph nodes endpoints.
            .service(web::resource("/graphs/nodes").route(web::post().to(graphs::nodes::get::get_graph_nodes_endpoint)))
            .service(web::resource("/graphs/nodes/add").route(web::post().to(graphs::nodes::add::add_graph_node_endpoint)))
            .service(web::resource("/graphs/nodes/delete").route(web::post().to(graphs::nodes::delete::delete_graph_node_endpoint)))
            .service(web::resource("/graphs/nodes/replace").route(web::post().to(graphs::nodes::replace::replace_graph_node_endpoint)))
            .service(web::resource("/graphs/nodes/property/update").route(web::post().to(graphs::nodes::property::update::update_graph_node_property_endpoint)))
            // Graph connections endpoints.
            .service(web::resource("/graphs/connections").route(web::post().to(graphs::connections::get::get_graph_connections_endpoint)))
            .service(web::resource("/graphs/connections/add").route(web::post().to(graphs::connections::add::add_graph_connection_endpoint)))
            .service(web::resource("/graphs/connections/delete").route(web::post().to(graphs::connections::delete::delete_graph_connection_endpoint)))
            .service(web::resource("/graphs/connections/msg_conversion/update").route(web::post().to(graphs::connections::msg_conversion::update::update_graph_connection_msg_conversion_endpoint)))
            // Messages endpoints.
            .service(web::resource("/messages/compatible").route(web::post().to(messages::compatible::get_compatible_messages_endpoint)))
            // Preferences endpoints.
            .service(web::resource("/preferences/schema").route(web::get().to(preferences::get_schema::get_preferences_schema_endpoint)))
            .service(
                web::resource("/preferences")
                    .route(web::get().to(preferences::get::get_preferences_endpoint))
                    .route(web::put().to(preferences::update::update_preferences_endpoint))
            )
            .service(web::resource("/preferences/field").route(web::patch().to(preferences::update_field::update_preferences_field_endpoint)))
            // Internal config endpoints.
            .service(web::resource("/internal-config/graph-ui/set").route(web::post().to(metadata::graph_ui::set::set_graph_ui_endpoint)))
            .service(web::resource("/internal-config/graph-ui/get").route(web::post().to(metadata::graph_ui::get::get_graph_ui_endpoint)))
            // File system endpoints.
            .service(web::resource("/dir-list").route(web::post().to(dir_list::list_dir_endpoint)))
            .service(
                web::resource("/file-content")
                    .route(web::post().to(file_content::get_file_content_endpoint))
                    .route(web::put().to(file_content::save_file_content_endpoint))
            )
            // Websocket endpoints.
            .service(web::resource("/ws/builtin-function").route(web::get().to(builtin_function::builtin_function_endpoint)))
            .service(web::resource("/ws/exec").route(web::get().to(exec::exec_endpoint)))
            .service(web::resource("/ws/terminal").route(web::get().to(terminal::ws_terminal_endpoint)))
            .service(web::resource("/ws/log-watcher").route(web::get().to(log_watcher::log_watcher_endpoint)))
            // Doc endpoints.
            .service(web::resource("/help-text").route(web::post().to(help_text::get_help_text_endpoint)))
            .service(web::resource("/doc-link").route(web::post().to(doc_link::get_doc_link_endpoint)))
            // Registry endpoints.
            .service(web::resource("/registry/packages").route(web::get().to(registry::packages::get_packages_endpoint)))
            // Environment endpoints.
            .service(web::resource("/env").route(web::get().to(env::get_env_endpoint)))
            .service(web::resource("/env-var").route(web::post().to(env_var::get_env_var_endpoint))),
    );
}

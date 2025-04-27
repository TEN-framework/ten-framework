//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::collections::HashMap;

use anyhow::Result;

use ten_rust::{
    base_dir_pkg_info::PkgsInfoInApp,
    graph::node::GraphNode,
    pkg_info::{
        get_pkg_info_for_extension_addon,
        message::{MsgDirection, MsgType},
    },
    schema::store::{are_msg_schemas_compatible, TenMsgSchema},
};

pub struct CompatibleExtensionAndMsg<'a> {
    pub extension: &'a GraphNode,
    pub msg_type: MsgType,
    pub msg_direction: MsgDirection,
    pub msg_name: String,
}

#[allow(clippy::too_many_arguments)]
pub fn get_compatible_msg_extension<'a>(
    extension_graph_nodes: &'a [GraphNode],
    app_base_dir: &Option<String>,
    pkgs_cache: &'a HashMap<String, PkgsInfoInApp>,
    desired_msg_dir: &MsgDirection,
    pivot: Option<&TenMsgSchema>,
    msg_type: &MsgType,
    msg_name: String,
) -> Result<Vec<CompatibleExtensionAndMsg<'a>>> {
    let mut result = Vec::new();

    for target_extension_graph_node in extension_graph_nodes {
        let target_extension_pkg_info = get_pkg_info_for_extension_addon(
            pkgs_cache,
            app_base_dir,
            &target_extension_graph_node.app,
            &target_extension_graph_node.addon,
        );

        match target_extension_pkg_info {
            Some(target_extension_pkg_info) => {
                let target_msg_schema = target_extension_pkg_info
                    .schema_store
                    .as_ref()
                    .and_then(|schema_store| {
                        let msg_name = msg_name.as_str();
                        match msg_type {
                            MsgType::Cmd => match desired_msg_dir {
                                MsgDirection::In => {
                                    schema_store.cmd_in.get(msg_name)
                                }
                                MsgDirection::Out => {
                                    schema_store.cmd_out.get(msg_name)
                                }
                            },
                            MsgType::Data => match desired_msg_dir {
                                MsgDirection::In => {
                                    schema_store.data_in.get(msg_name)
                                }
                                MsgDirection::Out => {
                                    schema_store.data_out.get(msg_name)
                                }
                            },
                            MsgType::AudioFrame => match desired_msg_dir {
                                MsgDirection::In => {
                                    schema_store.audio_frame_in.get(msg_name)
                                }
                                MsgDirection::Out => {
                                    schema_store.audio_frame_out.get(msg_name)
                                }
                            },
                            MsgType::VideoFrame => match desired_msg_dir {
                                MsgDirection::In => {
                                    schema_store.video_frame_in.get(msg_name)
                                }
                                MsgDirection::Out => {
                                    schema_store.video_frame_out.get(msg_name)
                                }
                            },
                        }
                    });

                let compatible = match desired_msg_dir {
                    MsgDirection::In => are_msg_schemas_compatible(
                        pivot,
                        target_msg_schema,
                        false,
                        false,
                    ),
                    MsgDirection::Out => are_msg_schemas_compatible(
                        target_msg_schema,
                        pivot,
                        false,
                        false,
                    ),
                };

                if compatible.is_ok() {
                    result.push(CompatibleExtensionAndMsg {
                        extension: target_extension_graph_node,
                        msg_type: msg_type.clone(),
                        msg_name: msg_name.to_string(),
                        msg_direction: desired_msg_dir.clone(),
                    });
                }
            }
            None => continue,
        }
    }

    Ok(result)
}

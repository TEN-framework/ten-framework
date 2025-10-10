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
    graph::{
        connection::GraphLoc, msg_conversion::MsgAndResultConversion, node::GraphNodeType, Graph,
    },
    pkg_info::{
        find_pkgs_cache_entry_by_app_uri, get_pkg_info_for_extension_addon,
        manifest::api::{ManifestApiProperty, ManifestApiPropertyAttributes},
        message::{MsgDirection, MsgType},
        pkg_type::PkgType,
        PkgInfo,
    },
    schema::store::{
        are_msg_schemas_compatible, are_ten_schemas_compatible,
        create_c_schema_from_properties_and_required, find_c_msg_schema_from_pkg_info,
        TenMsgSchema,
    },
};

use crate::graph::msg_conversion::{
    msg_conversion_get_dest_msg_name, msg_conversion_get_final_target_schema,
};

type PkgInfoTuple<'a> = (Option<&'a PkgInfo>, Option<&'a PkgInfo>);

pub struct MsgConversionValidateInfo<'a> {
    pub src: &'a GraphLoc,
    pub dest: &'a GraphLoc,
    pub msg_type: &'a MsgType,
    pub msg_names: &'a Vec<String>,
    pub msg_conversion: &'a Option<MsgAndResultConversion>,
}

#[allow(clippy::too_many_arguments)]
fn validate_msg_conversion_c_schema_oneway(
    graph_app_base_dir: &Option<String>,
    msg_conversion_validate_info: &MsgConversionValidateInfo,
    pkgs_cache: &HashMap<String, PkgsInfoInApp>,
    compared_schema_properties: &Option<HashMap<String, ManifestApiPropertyAttributes>>,
    compared_schema_required: &Option<Vec<String>>,
    target_app: &Option<String>,
    target_extension_addon: &String,
    target_msg_name: &str,
    msg_direction: MsgDirection,
) -> Result<()> {
    // Create a temporary ManifestApiProperty from the separate properties and
    // required
    let temp_property =
        if compared_schema_properties.is_some() || compared_schema_required.is_some() {
            Some(ManifestApiProperty {
                properties: compared_schema_properties.clone(),
                required: compared_schema_required.clone(),
            })
        } else {
            None
        };

    if let Ok(compared_c_schema) = create_c_schema_from_properties_and_required(&temp_property) {
        if let Some(target_extension_pkg_info) = get_pkg_info_for_extension_addon(
            pkgs_cache,
            graph_app_base_dir,
            target_app,
            target_extension_addon,
        ) {
            if let Some(target_c_msg_schema) = find_c_msg_schema_from_pkg_info(
                target_extension_pkg_info,
                msg_conversion_validate_info.msg_type,
                target_msg_name,
                &msg_direction,
            ) {
                let target_c_schema = if msg_direction == MsgDirection::In {
                    target_c_msg_schema.msg.as_ref()
                } else {
                    target_c_msg_schema.result.as_ref()
                };

                are_ten_schemas_compatible(
                    compared_c_schema.as_ref(),
                    target_c_schema,
                    true,
                    true,
                )?;
            }
        }
    }

    Ok(())
}

async fn validate_msg_conversion_schema<'a>(
    pkgs_cache: &'a HashMap<String, PkgsInfoInApp>,
    graph: &'a Graph,
    graph_app_base_dir: &'a Option<String>,
    msg_conversion_validate_info: &'a MsgConversionValidateInfo<'a>,
) -> Result<()> {
    assert!(msg_conversion_validate_info.msg_conversion.is_some());
    assert!(msg_conversion_validate_info.msg_names.len() == 1);

    let src_extension_addon_name = graph
        .get_addon_name_of_node(
            graph_app_base_dir,
            msg_conversion_validate_info.src,
            msg_conversion_validate_info.msg_type,
            msg_conversion_validate_info.msg_names[0].as_str(),
            MsgDirection::Out,
        )
        .await?;

    let dest_extension_addon_name = graph
        .get_addon_name_of_node(
            graph_app_base_dir,
            msg_conversion_validate_info.dest,
            msg_conversion_validate_info.msg_type,
            msg_conversion_validate_info.msg_names[0].as_str(),
            MsgDirection::In,
        )
        .await?;

    // Default to using `src_msg_name` as the `dest_msg_name`, but check if
    // there's a special rule for `ten.name` to determine the `dest_msg_name`.
    let (dest_msg_name, ten_name_rule_index) = msg_conversion_get_dest_msg_name(
        msg_conversion_validate_info.msg_names[0].as_str(),
        msg_conversion_validate_info.msg_conversion.as_ref().unwrap(),
    )?;

    let src_app = match msg_conversion_validate_info.src.get_node_type()? {
        GraphNodeType::Extension => msg_conversion_validate_info.src.app.clone(),
        GraphNodeType::Subgraph => {
            let extension_node = graph
                .get_extension_node_from_subgraph_using_exposed_message(
                    graph_app_base_dir,
                    msg_conversion_validate_info.src.subgraph.as_ref().unwrap(),
                    msg_conversion_validate_info.msg_type,
                    msg_conversion_validate_info.msg_names[0].as_str(),
                    MsgDirection::Out,
                )
                .await?;
            extension_node.app
        }
        GraphNodeType::Selector => None,
    };

    let dest_app = match msg_conversion_validate_info.dest.get_node_type()? {
        GraphNodeType::Extension => msg_conversion_validate_info.dest.app.clone(),
        GraphNodeType::Subgraph => {
            let extension_node = graph
                .get_extension_node_from_subgraph_using_exposed_message(
                    graph_app_base_dir,
                    msg_conversion_validate_info.dest.subgraph.as_ref().unwrap(),
                    msg_conversion_validate_info.msg_type,
                    msg_conversion_validate_info.msg_names[0].as_str(),
                    MsgDirection::In,
                )
                .await?;
            extension_node.app
        }
        GraphNodeType::Selector => None,
    };

    let (converted_schema, converted_result_schema) = msg_conversion_get_final_target_schema(
        graph_app_base_dir,
        pkgs_cache,
        &src_app,
        &src_extension_addon_name,
        &dest_app,
        &dest_extension_addon_name,
        msg_conversion_validate_info.msg_type,
        msg_conversion_validate_info.msg_names[0].as_str(),
        &dest_msg_name,
        ten_name_rule_index,
        msg_conversion_validate_info.msg_conversion.as_ref().unwrap(),
    )
    .await?;

    if let Some(converted_schema) = converted_schema {
        #[cfg(test)]
        {
            eprintln!(
                "msg_conversion converted_schema: {}",
                serde_json::to_string_pretty(&converted_schema).unwrap()
            );
        }

        validate_msg_conversion_c_schema_oneway(
            graph_app_base_dir,
            msg_conversion_validate_info,
            pkgs_cache,
            &converted_schema.property.as_ref().and_then(|p| p.properties().cloned()),
            &converted_schema.property.as_ref().and_then(|p| p.required.clone()),
            &dest_app,
            &dest_extension_addon_name,
            &converted_schema.name,
            MsgDirection::In,
        )?;
    }

    if let Some(converted_result_schema) = converted_result_schema {
        #[cfg(test)]
        {
            eprintln!(
                "msg_conversion converted_result_schema: {}",
                serde_json::to_string_pretty(&converted_result_schema).unwrap()
            );
        }

        validate_msg_conversion_c_schema_oneway(
            graph_app_base_dir,
            msg_conversion_validate_info,
            pkgs_cache,
            &converted_result_schema.property.as_ref().and_then(|p| p.properties().cloned()),
            &converted_result_schema.property.as_ref().and_then(|p| p.required.clone()),
            &src_app,
            &src_extension_addon_name,
            msg_conversion_validate_info.msg_names[0].as_str(),
            MsgDirection::Out,
        )?;
    } else {
        // No result conversion, so directly check if the original source result
        // schema and destination result schema are compatible.
        let (src_c_msg_schema, dest_c_msg_schema, error_message) = get_src_and_dest_c_msg_schema(
            pkgs_cache,
            graph,
            graph_app_base_dir,
            msg_conversion_validate_info,
        )
        .await?;

        if src_c_msg_schema.is_none() || dest_c_msg_schema.is_none() {
            return Ok(());
        }

        let src_c_msg_schema = src_c_msg_schema.unwrap();
        let dest_c_msg_schema = dest_c_msg_schema.unwrap();

        if let Err(err) = are_ten_schemas_compatible(
            src_c_msg_schema.result.as_ref(),
            dest_c_msg_schema.result.as_ref(),
            true,
            true,
        ) {
            assert!(error_message.is_some());
            return Err(anyhow::anyhow!("{}: {}", error_message.unwrap(), err));
        }
    }

    Ok(())
}

/// Finds package info for source and destination apps and extensions.
fn find_pkg_infos<'a>(
    pkgs_cache: &'a HashMap<String, PkgsInfoInApp>,
    src_app: &Option<String>,
    src_extension_addon_name: &str,
    dest_app: &Option<String>,
    dest_extension_addon_name: &str,
) -> Result<PkgInfoTuple<'a>> {
    // Helper function to find extension package info.
    let find_extension_pkg = |app_uri: &Option<String>,
                              extension_name: &str,
                              is_source: bool|
     -> Result<Option<&'a PkgInfo>> {
        let entity_type = if is_source { "Source" } else { "Destination" };

        let Some((_, base_dir_pkg_info)) = find_pkgs_cache_entry_by_app_uri(pkgs_cache, app_uri)
        else {
            return Err(anyhow::anyhow!(
                "{} app '{:?}' not found in the installed packages",
                entity_type,
                app_uri
            ));
        };

        // Check if app exists.
        if base_dir_pkg_info.app_pkg_info.is_none() {
            return Err(anyhow::anyhow!(
                "{} app '{:?}' found in map but app_pkg_info is None",
                entity_type,
                app_uri
            ));
        }

        // Find extension in extension_pkg_info.
        if let Some(extension_pkgs_info) = &base_dir_pkg_info.extension_pkgs_info {
            let found_extension_pkg_info = extension_pkgs_info.iter().find(|pkg| {
                assert!(pkg.manifest.type_and_name.pkg_type == PkgType::Extension);

                pkg.manifest.type_and_name.name == extension_name
            });

            if found_extension_pkg_info.is_none() {
                return Err(anyhow::anyhow!(
                    "{} extension '{}' not found in the installed packages for app '{:?}'",
                    entity_type,
                    extension_name,
                    app_uri
                ));
            }

            return Ok(found_extension_pkg_info);
        }

        // If we reach here, no package was found.
        Err(anyhow::anyhow!(
            "{} extension '{}' not found in the installed packages for app '{:?}'",
            entity_type,
            extension_name,
            app_uri
        ))
    };

    // Find both source and destination package info.
    let src_extension_pkg_info = find_extension_pkg(src_app, src_extension_addon_name, true)?;

    let dest_extension_pkg_info = find_extension_pkg(dest_app, dest_extension_addon_name, false)?;

    Ok((src_extension_pkg_info, dest_extension_pkg_info))
}

async fn get_src_and_dest_c_msg_schema<'a>(
    pkgs_cache: &'a HashMap<String, PkgsInfoInApp>,
    graph: &Graph,
    graph_app_base_dir: &Option<String>,
    msg_conversion_validate_info: &MsgConversionValidateInfo<'a>,
) -> Result<(Option<&'a TenMsgSchema>, Option<&'a TenMsgSchema>, Option<String>)> {
    let src_extension_addon = graph
        .get_addon_name_of_node(
            graph_app_base_dir,
            msg_conversion_validate_info.src,
            msg_conversion_validate_info.msg_type,
            msg_conversion_validate_info.msg_names[0].as_str(),
            MsgDirection::Out,
        )
        .await?;

    let dest_extension_addon = graph
        .get_addon_name_of_node(
            graph_app_base_dir,
            msg_conversion_validate_info.dest,
            msg_conversion_validate_info.msg_type,
            msg_conversion_validate_info.msg_names[0].as_str(),
            MsgDirection::In,
        )
        .await?;

    let src_app = match msg_conversion_validate_info.src.get_node_type()? {
        GraphNodeType::Extension => msg_conversion_validate_info.src.app.clone(),
        GraphNodeType::Subgraph => {
            let extension_node = graph
                .get_extension_node_from_subgraph_using_exposed_message(
                    graph_app_base_dir,
                    msg_conversion_validate_info.src.subgraph.as_ref().unwrap(),
                    msg_conversion_validate_info.msg_type,
                    msg_conversion_validate_info.msg_names[0].as_str(),
                    MsgDirection::Out,
                )
                .await?;
            extension_node.app
        }
        GraphNodeType::Selector => None,
    };

    let dest_app = match msg_conversion_validate_info.dest.get_node_type()? {
        GraphNodeType::Extension => msg_conversion_validate_info.dest.app.clone(),
        GraphNodeType::Subgraph => {
            let extension_node = graph
                .get_extension_node_from_subgraph_using_exposed_message(
                    graph_app_base_dir,
                    msg_conversion_validate_info.dest.subgraph.as_ref().unwrap(),
                    msg_conversion_validate_info.msg_type,
                    msg_conversion_validate_info.msg_names[0].as_str(),
                    MsgDirection::In,
                )
                .await?;
            extension_node.app
        }
        GraphNodeType::Selector => None,
    };

    let (src_extension_pkg_info, dest_extension_pkg_info) = find_pkg_infos(
        pkgs_cache,
        &src_app,
        &src_extension_addon,
        &dest_app,
        &dest_extension_addon,
    )?;

    if src_extension_pkg_info.is_none() || dest_extension_pkg_info.is_none() {
        return Ok((None, None, None));
    }

    let src_extension_pkg_info = src_extension_pkg_info.unwrap();
    let dest_extension_pkg_info = dest_extension_pkg_info.unwrap();

    // Get source and destination schemas based on message type.
    let (src_schema, dest_schema, error_message) = match msg_conversion_validate_info.msg_type {
        MsgType::Cmd => {
            let src = src_extension_pkg_info.schema_store.as_ref().and_then(|store| {
                store.cmd_out.get(msg_conversion_validate_info.msg_names[0].as_str())
            });
            let dest = dest_extension_pkg_info.schema_store.as_ref().and_then(|store| {
                store.cmd_in.get(msg_conversion_validate_info.msg_names[0].as_str())
            });
            (src, dest, "Command schema incompatibility between source and destination")
        }
        MsgType::Data => {
            let src = src_extension_pkg_info.schema_store.as_ref().and_then(|store| {
                store.data_out.get(msg_conversion_validate_info.msg_names[0].as_str())
            });
            let dest = dest_extension_pkg_info.schema_store.as_ref().and_then(|store| {
                store.data_in.get(msg_conversion_validate_info.msg_names[0].as_str())
            });
            (src, dest, "Data schema incompatibility between source and destination")
        }
        MsgType::AudioFrame => {
            let src = src_extension_pkg_info.schema_store.as_ref().and_then(|store| {
                store.audio_frame_out.get(msg_conversion_validate_info.msg_names[0].as_str())
            });
            let dest = dest_extension_pkg_info.schema_store.as_ref().and_then(|store| {
                store.audio_frame_in.get(msg_conversion_validate_info.msg_names[0].as_str())
            });
            (src, dest, "Audio frame schema incompatibility between source and destination")
        }
        MsgType::VideoFrame => {
            let src = src_extension_pkg_info.schema_store.as_ref().and_then(|store| {
                store.video_frame_out.get(msg_conversion_validate_info.msg_names[0].as_str())
            });
            let dest = dest_extension_pkg_info.schema_store.as_ref().and_then(|store| {
                store.video_frame_in.get(msg_conversion_validate_info.msg_names[0].as_str())
            });
            (src, dest, "Video frame schema incompatibility between source and destination")
        }
    };

    Ok((src_schema, dest_schema, Some(error_message.to_string())))
}

/// Checks schema compatibility between source and destination.
async fn check_schema_compatibility<'a>(
    pkgs_cache: &'a HashMap<String, PkgsInfoInApp>,
    graph: &Graph,
    graph_app_base_dir: &Option<String>,
    msg_conversion_validate_info: &MsgConversionValidateInfo<'a>,
) -> Result<()> {
    let (src_c_msg_schema, dest_c_msg_schema, error_message) = get_src_and_dest_c_msg_schema(
        pkgs_cache,
        graph,
        graph_app_base_dir,
        msg_conversion_validate_info,
    )
    .await?;

    if src_c_msg_schema.is_none() || dest_c_msg_schema.is_none() {
        return Ok(());
    }

    // Check schema compatibility.
    if let Err(err) = are_msg_schemas_compatible(src_c_msg_schema, dest_c_msg_schema, true, true) {
        assert!(error_message.is_some());
        return Err(anyhow::anyhow!("{}: {}", error_message.unwrap(), err));
    }

    Ok(())
}

pub async fn validate_connection_schema(
    pkgs_cache: &HashMap<String, PkgsInfoInApp>,
    graph: &Graph,
    graph_app_base_dir: &Option<String>,
    msg_conversion_validate_info: &MsgConversionValidateInfo<'_>,
) -> Result<()> {
    // Break down the selector nodes into individual nodes
    let mut src_nodes = Vec::new();
    let mut dest_nodes = Vec::new();

    if msg_conversion_validate_info.src.get_node_type()? == GraphNodeType::Selector {
        let node_name = msg_conversion_validate_info.src.get_node_name()?;
        if let Some(nodes) = graph.get_nodes_by_selector_node_name(node_name) {
            let locs: Vec<GraphLoc> = nodes.iter().map(|node| node.get_loc()).collect();
            src_nodes.extend(locs);
        }
    } else {
        src_nodes.push(msg_conversion_validate_info.src.clone());
    }

    if msg_conversion_validate_info.dest.get_node_type()? == GraphNodeType::Selector {
        let node_name = msg_conversion_validate_info.dest.get_node_name()?;
        if let Some(nodes) = graph.get_nodes_by_selector_node_name(node_name) {
            let locs: Vec<GraphLoc> = nodes.iter().map(|node| node.get_loc()).collect();
            dest_nodes.extend(locs);
        }
    } else {
        dest_nodes.push(msg_conversion_validate_info.dest.clone());
    }

    assert!(!src_nodes.is_empty());
    assert!(!dest_nodes.is_empty());

    // Validate the connection schema for each combination of src and dest nodes
    for src_node in src_nodes.clone() {
        for dest_node in dest_nodes.clone() {
            // Selector nodes should all be broken down into extension/subgraph nodes by now
            assert!(src_node.get_node_type()? != GraphNodeType::Selector);
            assert!(dest_node.get_node_type()? != GraphNodeType::Selector);

            let each_node_info = MsgConversionValidateInfo {
                src: &src_node,
                dest: &dest_node,
                msg_type: msg_conversion_validate_info.msg_type,
                msg_names: msg_conversion_validate_info.msg_names,
                msg_conversion: msg_conversion_validate_info.msg_conversion,
            };

            if each_node_info.msg_names.len() == 1 && each_node_info.msg_conversion.is_some() {
                validate_msg_conversion_schema(
                    pkgs_cache,
                    graph,
                    graph_app_base_dir,
                    &each_node_info,
                )
                .await?;
            } else if each_node_info.msg_conversion.is_none() {
                check_schema_compatibility(pkgs_cache, graph, graph_app_base_dir, &each_node_info)
                    .await?;
            } else {
                // msg_conversion is present but msg_names.len() != 1
                return Err(anyhow::anyhow!(
                    "One and only one message name is allowed when msg_conversion is present"
                ));
            }
        }
    }

    Ok(())
}

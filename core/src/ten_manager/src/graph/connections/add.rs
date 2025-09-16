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
        connection::{GraphConnection, GraphDestination, GraphLoc, GraphMessageFlow},
        msg_conversion::MsgAndResultConversion,
        node::GraphNodeType,
        Graph,
    },
    pkg_info::message::MsgType,
};

use super::validate::{validate_connection_schema, MsgConversionValidateInfo};

/// Helper function to add a message flow to a specific flow collection.
fn add_to_flow(
    flow_collection: &mut Option<Vec<GraphMessageFlow>>,
    message_flow: GraphMessageFlow,
) {
    if flow_collection.is_none() {
        *flow_collection = Some(Vec::new());
    }

    // Check if a message flow with the same name already exists.
    let flows = flow_collection.as_mut().unwrap();
    if let Some(existing_flow) = flows.iter_mut().find(|flow| flow.name == message_flow.name) {
        // Add the destination to the existing flow if it doesn't already
        // exist.
        if !existing_flow.dest.iter().any(|dest| {
            dest.loc.extension == message_flow.dest[0].loc.extension
                && dest.loc.app == message_flow.dest[0].loc.app
        }) {
            existing_flow.dest.push(message_flow.dest[0].clone());
        }
    } else {
        // Add the new message flow.
        flows.push(message_flow);
    }
}

/// Adds a message flow to a connection based on message type.
fn add_message_flow_to_connection(
    connection: &mut GraphConnection,
    msg_type: &MsgType,
    message_flow: GraphMessageFlow,
) -> Result<()> {
    // Add the message flow to the appropriate vector based on message type.
    match msg_type {
        MsgType::Cmd => add_to_flow(&mut connection.cmd, message_flow),
        MsgType::Data => add_to_flow(&mut connection.data, message_flow),
        MsgType::AudioFrame => add_to_flow(&mut connection.audio_frame, message_flow),
        MsgType::VideoFrame => add_to_flow(&mut connection.video_frame, message_flow),
    }
    Ok(())
}

fn node_matches(loc: &GraphLoc, node_type: &GraphNodeType, node_name: &str, node_app: &Option<String>) -> bool {
    if *node_type == GraphNodeType::Extension {
        return loc.extension.as_ref().is_some_and(|ext| ext == node_name) && loc.app == *node_app;
    }
    else if *node_type == GraphNodeType::Subgraph {
        return loc.subgraph.as_ref().is_some_and(|subgraph| subgraph == node_name);
    }
    else if *node_type == GraphNodeType::Selector {
        return loc.selector.as_ref().is_some_and(|selector| selector == node_name);
    }

    false
}

/// Checks if the connection already exists.
#[allow(clippy::too_many_arguments)]
#[allow(clippy::ptr_arg)]
fn check_connection_exists(
    graph: &Graph,
    src_node_type: &GraphNodeType,
    src_node_name: &String,
    src_node_app: &Option<String>,
    msg_type: &MsgType,
    msg_name: &Vec<String>,
    dest_node_type: &GraphNodeType,
    dest_node_name: &String,
    dest_node_app: &Option<String>,
) -> Result<()> {
    if let Some(connections) = &graph.connections {
        for conn in connections.iter() {
            // Check if source matches.
            if node_matches(&conn.loc, src_node_type, src_node_name, src_node_app)
            {
                // Check for duplicate message flows based on message type.
                let msg_flows = match msg_type {
                    MsgType::Cmd => conn.cmd.as_ref(),
                    MsgType::Data => conn.data.as_ref(),
                    MsgType::AudioFrame => conn.audio_frame.as_ref(),
                    MsgType::VideoFrame => conn.video_frame.as_ref(),
                };

                if let Some(flows) = msg_flows {
                    for flow in flows {
                        // Check if message name matches.
                        for name in msg_name.iter() {
                            if flow.name.as_deref() == Some(name) {
                                // Check if destination already exists.
                                for dest in &flow.dest {
                                    if node_matches(&dest.loc, dest_node_type, dest_node_name, dest_node_app)
                                    {
                                        return Err(anyhow::anyhow!(
                                            "Connection already exists: src: {:?} '{}', \
                                            msg_type:{:?}, msg_name:{}, dest: {:?} '{}'",
                                            src_node_type,
                                            src_node_name,
                                            msg_type,
                                            name,
                                            dest_node_type,
                                            dest_node_name,
                                        ));
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    Ok(())
}


/// Adds a new connection between two extension nodes in the graph.
#[allow(clippy::too_many_arguments)]
pub async fn graph_add_connection(
    graph: &mut Graph,
    graph_app_base_dir: &Option<String>,
    pkgs_cache: &HashMap<String, PkgsInfoInApp>,
    src: GraphLoc,
    dest: GraphLoc,
    msg_type: MsgType,
    msg_name: Vec<String>,
    msg_conversion: Option<MsgAndResultConversion>,
) -> Result<()> {
    // Store the original state in case validation fails.
    let original_graph = graph.clone();

    //store node_type: extension, subgraph, selector
    let src_node_type = src.get_node_type()?;
    let dest_node_type = dest.get_node_type()?;
    let src_node_name = src.get_node_name()?;
    let dest_node_name = dest.get_node_name()?;

    // Check if nodes exist.
    GraphLoc::check_node_exists(&src, graph)?;
    GraphLoc::check_node_exists(&dest, graph)?;

    // Check if connection already exists.
    check_connection_exists(
        graph,
        &src_node_type,
        src_node_name,
        &src.app,
        &msg_type,
        &msg_name,
        &dest_node_type,
        dest_node_name,
        &dest.app,
    )?;

    // Validate connection schema ONLY for Extension nodes
    //TODO：Validation for Extension and Selector nodes
    if src_node_type == GraphNodeType::Extension && dest_node_type == GraphNodeType::Extension {
        validate_connection_schema(
            pkgs_cache,
            graph,
            graph_app_base_dir,
            &MsgConversionValidateInfo {
                src_app: &src.app,
                src_extension: src_node_name,
                msg_type: &msg_type,
                msg_name: &msg_name[0],
                dest_app: &dest.app,
                dest_extension: dest_node_name,
                msg_conversion: &msg_conversion,
            },
        )
        .await?;
    }

    // Create destination object.
    let destination = GraphDestination {
        loc: dest,
        msg_conversion,
    };

    // Initialize connections if None.
    if graph.connections.is_none() {
        graph.connections = Some(Vec::new());
    }

    // Create a message flow.
    if msg_name.is_empty() {
        return Err(anyhow::anyhow!("Message name is empty"));
    }

    let message_flow : GraphMessageFlow = if msg_name.len() == 1 {
        GraphMessageFlow::new(Some(msg_name[0].clone()), None, vec![destination], vec![])
    }
    else {
        GraphMessageFlow::new(None, Some(msg_name), vec![destination], vec![])
    };

    // Get or create a connection for the source node and add the message
    // flow.
    {
        let connections = graph.connections.as_mut().unwrap();

        // Find or create connection.
        let connection_idx = if let Some((idx, _)) =
            connections.iter().enumerate().find(|(_, conn)| {
                conn.loc.matches(&src)
            }) {
            idx
        } else {
            // Create a new connection for the source node.
            connections.push(GraphConnection {
                loc: src,
                cmd: None,
                data: None,
                audio_frame: None,
                video_frame: None,
            });
            connections.len() - 1
        };

        // Add the message flow to the appropriate collection.
        let connection = &mut connections[connection_idx];
        add_message_flow_to_connection(connection, &msg_type, message_flow)?;
    }

    // Validate the updated graph.
    match graph.validate_and_complete(None) {
        Ok(_) => Ok(()),
        Err(e) => {
            // Restore the original graph if validation fails.
            *graph = original_graph;
            Err(e)
        }
    }
}

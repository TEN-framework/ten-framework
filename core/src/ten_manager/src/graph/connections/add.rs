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

/// Checks if the connection already exists.
#[allow(clippy::too_many_arguments)]
#[allow(clippy::ptr_arg)]
fn check_connection_exists(
    graph: &Graph,
    src: &GraphLoc,
    dest: &GraphLoc,
    msg_type: &MsgType,
    msg_names: &Vec<String>,
) -> Result<()> {
    if let Some(connections) = &graph.connections {
        for conn in connections.iter() {
            // Check if source matches.
            if conn.loc.matches(src) {
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
                        for name in msg_names.iter() {
                            if flow.name.as_deref() == Some(name) {
                                // Check if destination already exists.
                                for dest_item in &flow.dest {
                                    if dest_item.loc.matches(dest) {
                                        return Err(anyhow::anyhow!(
                                            "Connection already exists: src: {:?} '{}', \
                                             msg_type:{:?}, msg_name:{}, dest: {:?} '{}'",
                                            src.get_node_type()?,
                                            src.get_node_name()?,
                                            msg_type,
                                            name,
                                            dest.get_node_type()?,
                                            dest.get_node_name()?,
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
    msg_names: Vec<String>,
    msg_conversion: Option<MsgAndResultConversion>,
) -> Result<()> {
    // Store the original state in case validation fails.
    let original_graph = graph.clone();

    // Check if nodes exist.
    GraphLoc::check_node_exists(&src, graph)?;
    GraphLoc::check_node_exists(&dest, graph)?;

    // Check if connection already exists.
    check_connection_exists(graph, &src, &dest, &msg_type, &msg_names)?;

    validate_connection_schema(
        pkgs_cache,
        graph,
        graph_app_base_dir,
        &MsgConversionValidateInfo {
            src: &src,
            dest: &dest,
            msg_type: &msg_type,
            msg_names: &msg_names,
            msg_conversion: &msg_conversion,
        },
    )
    .await?;

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
    if msg_names.is_empty() {
        return Err(anyhow::anyhow!("Message name is empty"));
    }

    let message_flow: GraphMessageFlow = if msg_names.len() == 1 {
        GraphMessageFlow::new(Some(msg_names[0].clone()), None, vec![destination], vec![])
    } else {
        GraphMessageFlow::new(None, Some(msg_names), vec![destination], vec![])
    };

    // Get or create a connection for the source node and add the message
    // flow.
    {
        let connections = graph.connections.as_mut().unwrap();

        // Find or create connection.
        let connection_idx = if let Some((idx, _)) =
            connections.iter().enumerate().find(|(_, conn)| conn.loc.matches(&src))
        {
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

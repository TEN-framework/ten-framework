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
        connection::{
            GraphConnection, GraphDestination, GraphLoc, GraphMessageFlow,
        },
        msg_conversion::MsgAndResultConversion,
        node::GraphNode,
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
    if let Some(existing_flow) =
        flows.iter_mut().find(|flow| flow.name == message_flow.name)
    {
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
        MsgType::AudioFrame => {
            add_to_flow(&mut connection.audio_frame, message_flow)
        }
        MsgType::VideoFrame => {
            add_to_flow(&mut connection.video_frame, message_flow)
        }
    }
    Ok(())
}

/// Checks if the connection already exists.
fn check_connection_exists(
    graph: &Graph,
    src_app: &Option<String>,
    src_extension: &str,
    msg_type: &MsgType,
    msg_name: &str,
    dest_app: &Option<String>,
    dest_extension: &str,
) -> Result<()> {
    if let Some(connections) = &graph.connections {
        for conn in connections.iter() {
            // Check if source matches.
            if conn
                .loc
                .extension
                .as_ref()
                .is_some_and(|ext| ext == src_extension)
                && conn.loc.app == *src_app
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
                        if flow.name == msg_name {
                            // Check if destination already exists.
                            for dest in &flow.dest {
                                if dest.loc.extension.as_deref()
                                    == Some(dest_extension)
                                    && dest.loc.app == *dest_app
                                {
                                    return Err(anyhow::anyhow!(
                                        "Connection already exists: \
                                         src:({:?}, {}), msg_type:{:?}, \
                                         msg_name:{}, dest:({:?}, {})",
                                        src_app,
                                        src_extension,
                                        msg_type,
                                        msg_name,
                                        dest_app,
                                        dest_extension
                                    ));
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

/// Checks if source and destination nodes exist in the graph.
fn check_nodes_exist(
    graph: &Graph,
    src_app: &Option<String>,
    src_extension: &str,
    dest_app: &Option<String>,
    dest_extension: &str,
) -> Result<()> {
    // Validate that source node exists.
    let src_node_exists = graph.nodes.iter().any(|node| match node {
        GraphNode::Extension { content } => {
            content.name == src_extension && content.app == *src_app
        }
        _ => false,
    });

    if !src_node_exists {
        return Err(anyhow::anyhow!(
            "Source node with extension '{}' and app '{:?}' not found in the \
             graph",
            src_extension,
            src_app
        ));
    }

    // Validate that destination node exists.
    let dest_node_exists = graph.nodes.iter().any(|node| match node {
        GraphNode::Extension { content } => {
            content.name == dest_extension && content.app == *dest_app
        }
        _ => false,
    });

    if !dest_node_exists {
        return Err(anyhow::anyhow!(
            "Destination node with extension '{}' and app '{:?}' not found in \
             the graph",
            dest_extension,
            dest_app
        ));
    }

    Ok(())
}

/// Adds a new connection between two extension nodes in the graph.
#[allow(clippy::too_many_arguments)]
pub async fn graph_add_connection(
    graph: &mut Graph,
    graph_app_base_dir: &Option<String>,
    src_app: Option<String>,
    src_extension: String,
    msg_type: MsgType,
    msg_name: String,
    dest_app: Option<String>,
    dest_extension: String,
    pkgs_cache: &HashMap<String, PkgsInfoInApp>,
    msg_conversion: Option<MsgAndResultConversion>,
) -> Result<()> {
    // Store the original state in case validation fails.
    let original_graph = graph.clone();

    // Check if nodes exist.
    check_nodes_exist(
        graph,
        &src_app,
        &src_extension,
        &dest_app,
        &dest_extension,
    )?;

    // Check if connection already exists.
    check_connection_exists(
        graph,
        &src_app,
        &src_extension,
        &msg_type,
        &msg_name,
        &dest_app,
        &dest_extension,
    )?;

    // Validate connection schema.
    validate_connection_schema(
        pkgs_cache,
        graph,
        graph_app_base_dir,
        &MsgConversionValidateInfo {
            src_app: &src_app,
            src_extension: &src_extension,
            msg_type: &msg_type,
            msg_name: &msg_name,
            dest_app: &dest_app,
            dest_extension: &dest_extension,
            msg_conversion: &msg_conversion,
        },
    )
    .await?;

    // Create destination object.
    let destination = GraphDestination {
        loc: GraphLoc {
            app: dest_app,
            extension: Some(dest_extension),
            subgraph: None,
            selector: None,
        },
        msg_conversion,
    };

    // Initialize connections if None.
    if graph.connections.is_none() {
        graph.connections = Some(Vec::new());
    }

    // Create a message flow.
    let message_flow =
        GraphMessageFlow::new(msg_name, vec![destination], vec![]);

    // Get or create a connection for the source node and add the message
    // flow.
    {
        let connections = graph.connections.as_mut().unwrap();

        // Find or create connection.
        let connection_idx = if let Some((idx, _)) =
            connections.iter().enumerate().find(|(_, conn)| {
                (conn.loc.extension.as_ref() == Some(&src_extension))
                    && conn.loc.app == src_app
            }) {
            idx
        } else {
            // Create a new connection for the source node.
            connections.push(GraphConnection {
                loc: GraphLoc {
                    app: src_app.clone(),
                    extension: Some(src_extension),
                    subgraph: None,
                    selector: None,
                },
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
    match graph.validate_and_complete_and_flatten(None).await {
        Ok(_) => Ok(()),
        Err(e) => {
            // Restore the original graph if validation fails.
            *graph = original_graph;
            Err(e)
        }
    }
}

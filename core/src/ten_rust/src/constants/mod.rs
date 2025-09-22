//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
pub mod service_hub;
pub use service_hub::*;

pub const ERR_MSG_GRAPH_LOCALHOST_FORBIDDEN_IN_SINGLE_APP_MODE: &str =
    "'localhost' is not allowed in graph definition, and the graph seems to be a single-app \
     graph, just remove the 'app' field";

pub const ERR_MSG_GRAPH_LOCALHOST_FORBIDDEN_IN_MULTI_APP_MODE: &str =
    "'localhost' is not allowed in graph definition, change the content of 'app' field to be \
     consistent with 'ten::uri'";

pub const ERR_MSG_GRAPH_MIXED_APP_DECLARATIONS: &str =
    "Either all nodes should have 'app' declared, or none should, but not a mix of both.";

pub const ERR_MSG_GRAPH_APP_FIELD_EMPTY: &str =
    "the 'app' field can not be empty, remove the field if the graph is a single-app graph";

pub const ERR_MSG_GRAPH_APP_FIELD_SHOULD_NOT_BE_DECLARED: &str =
    "the 'app' field should not be declared, as not any node has declared it";

pub const ERR_MSG_GRAPH_APP_FIELD_SHOULD_BE_DECLARED: &str =
    "the 'app' field should be declared, as it has been declared in nodes";

pub const ERR_MSG_UNKNOWN_GRAPH_NODE_TYPE: &str =
    "the type of the node must be one of extension/subgraph/selector";

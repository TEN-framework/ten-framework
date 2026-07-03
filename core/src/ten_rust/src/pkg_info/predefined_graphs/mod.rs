//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::collections::HashMap;

use uuid::Uuid;

use crate::graph::graph_info::GraphInfo;

pub fn graphs_cache_find<F>(
    graphs_cache: &HashMap<Uuid, GraphInfo>,
    predicate: F,
) -> Option<&GraphInfo>
where
    F: Fn(&GraphInfo) -> bool,
{
    graphs_cache.iter().find_map(|(_, graph)| if predicate(graph) { Some(graph) } else { None })
}

pub fn graphs_cache_find_mut<F>(
    graphs_cache: &mut HashMap<Uuid, GraphInfo>,
    predicate: F,
) -> Option<&mut GraphInfo>
where
    F: Fn(&mut GraphInfo) -> bool,
{
    graphs_cache.iter_mut().find_map(|(_, graph)| if predicate(graph) { Some(graph) } else { None })
}

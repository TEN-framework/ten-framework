//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

use regex::Regex;
use serde::{Deserialize, Serialize};

use crate::registry::found_result::PkgRegistryInfo;

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct PkgSearchFilter {
    #[serde(flatten)]
    pub filter: FilterNode,
}

#[derive(Debug, Serialize, Deserialize, Clone)]
#[serde(untagged)]
pub enum FilterNode {
    Atomic(AtomicFilter),
    Logic(LogicFilter),
}

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct AtomicFilter {
    pub field: String,
    pub operator: String,
    pub value: String,
}

#[derive(Debug, Serialize, Deserialize, Clone)]
#[serde(untagged)]
pub enum LogicFilter {
    And { and: Vec<FilterNode> },
    Or { or: Vec<FilterNode> },
}

pub fn matches_filter(info: &PkgRegistryInfo, node: &FilterNode) -> bool {
    match node {
        FilterNode::Atomic(atomic) => {
            // For local match, we only support regex for now.
            if atomic.operator != "regex" {
                return false;
            }
            let re = match Regex::new(&atomic.value) {
                Ok(r) => r,
                Err(_) => return false,
            };
            match atomic.field.as_str() {
                "name" => re.is_match(&info.basic_info.type_and_name.name),
                "tag" => info.tags.as_ref().is_some_and(|tags| tags.iter().any(|t| re.is_match(t))),
                "display_name" => info.display_name.as_ref().is_some_and(|dn| {
                    dn.locales
                        .iter()
                        .any(|(_, v)| re.is_match(&v.content.clone().unwrap_or_default()))
                }),
                _ => false,
            }
        }
        FilterNode::Logic(logic) => match logic {
            LogicFilter::And {
                and,
            } => and.iter().all(|n| matches_filter(info, n)),
            LogicFilter::Or {
                or,
            } => or.iter().any(|n| matches_filter(info, n)),
        },
    }
}

//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use std::sync::Arc;

    use ten_manager::{
        home::config::TmanConfig,
        output::cli::TmanOutputCli,
        registry::{
            search::{AtomicFilter, FilterNode, LogicFilter, PkgSearchFilter},
            search_packages,
        },
    };

    #[tokio::test]
    async fn test_search_packages() {
        let tman_config = Arc::new(tokio::sync::RwLock::new(TmanConfig {
            verbose: true,
            ..TmanConfig::default()
        }));

        let filter = PkgSearchFilter {
            filter: FilterNode::Atomic(AtomicFilter {
                field: "name".to_string(),
                operator: "regex".to_string(),
                value: "^ten_runtime_.*$".to_string(),
            }),
        };

        let result = search_packages(
            tman_config,
            &filter,
            Some(10),
            Some(1),
            Some("version"),
            Some("desc"),
            None,
            &Arc::new(Box::new(TmanOutputCli)),
        )
        .await;

        println!("result: {result:?}");

        assert!(result.is_ok());
        let result = result.unwrap();
        assert_eq!(result.1.len(), 10);
    }

    #[tokio::test]
    async fn test_search_packages_with_and_filter() {
        let tman_config = Arc::new(tokio::sync::RwLock::new(TmanConfig {
            verbose: true,
            ..TmanConfig::default()
        }));

        let filter = PkgSearchFilter {
            filter: FilterNode::Logic(LogicFilter::And {
                and: vec![
                    FilterNode::Atomic(AtomicFilter {
                        field: "name".to_string(),
                        operator: "regex".to_string(),
                        value: "^ten_runtime_.*$".to_string(),
                    }),
                    FilterNode::Atomic(AtomicFilter {
                        field: "type".to_string(),
                        operator: "exact".to_string(),
                        value: "system".to_string(),
                    }),
                    FilterNode::Atomic(AtomicFilter {
                        field: "version".to_string(),
                        operator: "exact".to_string(),
                        value: "0.10.20".to_string(),
                    }),
                ],
            }),
        };

        let result = search_packages(
            tman_config,
            &filter,
            None,
            None,
            None,
            None,
            None,
            &Arc::new(Box::new(TmanOutputCli)),
        )
        .await;

        println!("result: {result:?}");

        assert!(result.is_ok());
        let result = result.unwrap();
        assert!(!result.1.is_empty());
    }
}

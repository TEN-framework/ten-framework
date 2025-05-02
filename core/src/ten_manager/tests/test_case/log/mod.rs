//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use std::collections::HashMap;

    use ten_manager::log::{parse_graph_resources_log, GraphResourcesLog};

    #[test]
    fn test_parse_graph_resources_log_extension_context() {
        let log_message = "05-02 20:29:25.168 1565912(1565914) M \
                           ten_extension_context_log_graph_resources@\
                           extension_context.c:352 [graph resources] \
                           {\"app_uri\": \"msgpack://127.0.0.1:8001/\", \
                           \"graph name\": \"\", \"graph id\": \
                           \"b99a15fb-1db6-4257-a13f-f3584e892e29\" }";

        let mut graph_resources_log = GraphResourcesLog {
            graph_id: String::new(),
            graph_name: String::new(),
            apps: HashMap::new(),
        };

        let result =
            parse_graph_resources_log(log_message, &mut graph_resources_log);
        assert!(result.is_ok());

        assert_eq!(
            graph_resources_log.graph_id,
            "b99a15fb-1db6-4257-a13f-f3584e892e29"
        );
        assert_eq!(graph_resources_log.graph_name, "");

        let app_uri = Some("msgpack://127.0.0.1:8001/".to_string());
        assert!(graph_resources_log.apps.contains_key(&app_uri));
    }

    #[test]
    fn test_parse_graph_resources_log_with_extension_threads() {
        let log_message =
            "05-02 20:29:25.233 1565912(1565927) M \
             ten_extension_thread_log_graph_resources@extension_thread.c:550 \
             [graph resources] {\"app_uri\": \"msgpack://127.0.0.1:8001/\", \
             \"graph name\": \"\", \"graph id\": \
             \"b99a15fb-1db6-4257-a13f-f3584e892e29\", \"extension_threads\": \
             {\"1565927\": {\"extensions\": [\"test_extension\"]}}}";

        let mut graph_resources_log = GraphResourcesLog {
            graph_id: String::new(),
            graph_name: String::new(),
            apps: HashMap::new(),
        };

        let result =
            parse_graph_resources_log(log_message, &mut graph_resources_log);
        assert!(result.is_ok());

        assert_eq!(
            graph_resources_log.graph_id,
            "b99a15fb-1db6-4257-a13f-f3584e892e29"
        );
        assert_eq!(graph_resources_log.graph_name, "");

        let app_uri = Some("msgpack://127.0.0.1:8001/".to_string());
        assert!(graph_resources_log.apps.contains_key(&app_uri));

        let app_info = graph_resources_log.apps.get(&app_uri).unwrap();
        assert!(app_info.extension_threads.contains_key("1565927"));

        let thread_info = app_info.extension_threads.get("1565927").unwrap();
        assert_eq!(thread_info.extensions.len(), 1);
        assert_eq!(thread_info.extensions[0], "test_extension");
    }

    #[test]
    fn test_parse_non_graph_resources_log() {
        let log_message = "05-02 20:29:25.168 1565912(1565914) D \
                           ten_extension_context_log_graph_resources@\
                           extension_context.c:352 This is a debug log";

        let mut graph_resources_log = GraphResourcesLog {
            graph_id: String::new(),
            graph_name: String::new(),
            apps: HashMap::new(),
        };

        let result =
            parse_graph_resources_log(log_message, &mut graph_resources_log);
        assert!(result.is_ok());

        // The log should be ignored, so the graph_resources_log should remain
        // unchanged
        assert_eq!(graph_resources_log.graph_id, "");
        assert_eq!(graph_resources_log.graph_name, "");
        assert!(graph_resources_log.apps.is_empty());
    }
}

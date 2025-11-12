//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use std::collections::HashMap;

    use ten_manager::log::{parse_log_line, GraphResourcesLog};

    #[test]
    fn test_parse_graph_resources_log_extension_context() {
        let log_message = "05-02 20:29:25.168 1565912(1565914) M \
                           ten_extension_context_log_graph_resources@extension_context.c:352 \
                           [graph resources] {\"app_base_dir\": \"xxx\", \"app_uri\": \
                           \"msgpack://127.0.0.1:8001/\", \"graph_id\": \
                           \"b99a15fb-1db6-4257-a13f-f3584e892e29\" }";

        let mut graph_resources_log = GraphResourcesLog {
            app_base_dir: String::new(),
            app_uri: None,
            graph_id: String::new(),
            graph_name: None,
            extension_threads: HashMap::new(),
        };

        let result = parse_log_line(log_message, &mut graph_resources_log);
        assert!(result.is_none()); // Graph resources log returns None

        assert_eq!(graph_resources_log.graph_id, "b99a15fb-1db6-4257-a13f-f3584e892e29");
        assert_eq!(graph_resources_log.graph_name, None);
        assert_eq!(graph_resources_log.app_uri, Some("msgpack://127.0.0.1:8001/".to_string()));
    }

    #[test]
    fn test_parse_graph_resources_log_with_extension_threads() {
        let log_message = "05-02 20:29:25.233 1565912(1565927) M \
                           ten_extension_thread_log_graph_resources@extension_thread.c:550 [graph \
                           resources] {\"app_base_dir\": \"xxx\", \"app_uri\": \
                           \"msgpack://127.0.0.1:8001/\", \"graph_id\": \
                           \"b99a15fb-1db6-4257-a13f-f3584e892e29\", \"extension_threads\": \
                           {\"1565927\": {\"extensions\": [\"test_extension\"]}}}";

        let mut graph_resources_log = GraphResourcesLog {
            app_base_dir: String::new(),
            app_uri: None,
            graph_id: String::new(),
            graph_name: None,
            extension_threads: HashMap::new(),
        };

        let result = parse_log_line(log_message, &mut graph_resources_log);
        assert!(result.is_none()); // Graph resources log returns None

        assert_eq!(graph_resources_log.graph_id, "b99a15fb-1db6-4257-a13f-f3584e892e29");
        assert_eq!(graph_resources_log.graph_name, None);
        assert_eq!(graph_resources_log.app_uri, Some("msgpack://127.0.0.1:8001/".to_string()));

        assert!(graph_resources_log.extension_threads.contains_key("1565927"));

        let thread_info = graph_resources_log.extension_threads.get("1565927").unwrap();
        assert_eq!(thread_info.extensions.len(), 1);
        assert_eq!(thread_info.extensions[0], "test_extension");
    }

    #[test]
    fn test_parse_non_graph_resources_log() {
        let log_message = "05-02 20:29:25.168 1565912(1565914) D \
                           ten_extension_context_log_graph_resources@extension_context.c:352 This \
                           is a debug log";

        let mut graph_resources_log = GraphResourcesLog {
            app_base_dir: String::new(),
            app_uri: None,
            graph_id: String::new(),
            graph_name: None,
            extension_threads: HashMap::new(),
        };

        let result = parse_log_line(log_message, &mut graph_resources_log);
        assert!(result.is_none()); // Non-graph resources log without valid thread returns None

        // The log should be ignored, so the graph_resources_log should remain
        // unchanged
        assert_eq!(graph_resources_log.graph_id, "");
        assert_eq!(graph_resources_log.graph_name, None);
        assert!(graph_resources_log.extension_threads.is_empty());
    }

    #[test]
    fn test_parse_log_with_extension_metadata() {
        let log_message = "05-02 22:23:37.460 1713000(1713045) D \
                           ten_extension_on_configure_done@on_xxx.c:95 [test_extension] \
                           on_configure() done";

        let extension_name = log_message
            .split_once('[')
            .and_then(|(_, rest)| rest.split_once(']'))
            .map(|(extension_name, _)| extension_name.trim().to_string());

        assert_eq!(extension_name, Some("test_extension".to_string()));
    }

    #[test]
    fn test_parse_log_without_extension_metadata() {
        let log_message = "05-02 22:23:37.329 1713000(1713045) W pthread_routine@thread.c:114 \
                           Failed to set thread name: ";

        let extension_name = log_message
            .split_once('[')
            .and_then(|(_, rest)| rest.split_once(']'))
            .map(|(extension_name, _)| extension_name.trim().to_string());

        assert_eq!(extension_name, None);
    }

    #[test]
    fn test_parse_multiple_logs_with_extension_metadata() {
        let log_messages = [
            "05-02 22:23:37.460 1713000(1713045) D ten_extension_on_configure_done@on_xxx.c:95 \
             [test_extension] on_configure() done",
            "05-02 22:23:37.460 1713000(1713045) I \
             ten_extension_handle_ten_namespace_properties@metadata.c:314 [test_extension] `ten` \
             section is not found in the property, skip",
            "05-02 22:23:37.366 1713000(1713045) D ten_extension_group_on_init_done@on_xxx.c:78 \
             [] on_init() done",
            "05-02 22:23:37.329 1713000(1713045) W pthread_routine@thread.c:114 Failed to set \
             thread name: ",
        ];

        let extension_names: Vec<Option<String>> = log_messages
            .iter()
            .map(|log| {
                log.split_once('[')
                    .and_then(|(_, rest)| rest.split_once(']'))
                    .map(|(extension_name, _)| extension_name.trim().to_string())
            })
            .collect();

        assert_eq!(
            extension_names,
            vec![
                Some("test_extension".to_string()),
                Some("test_extension".to_string()),
                Some("".to_string()),
                None
            ]
        );
    }
}

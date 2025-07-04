//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use ten_rust::graph::Graph;

    #[test]
    fn test_graph_with_selector() {
        let graph: Graph = serde_json::from_str(include_str!(
            "../../test_data/graph_with_selector/graph_with_selector_1.json"
        ))
        .unwrap();

        let selector_flattened = graph.flatten_selectors().unwrap().unwrap();

        println!(
            "selector flattened graph: {}",
            serde_json::to_string_pretty(&selector_flattened).unwrap()
        );

        assert_eq!(selector_flattened.connections.as_ref().unwrap().len(), 1);
        assert_eq!(
            selector_flattened.connections.as_ref().unwrap()[0].loc.app,
            Some("msgpack://127.0.0.1:8001/".to_string())
        );
        assert_eq!(
            selector_flattened.connections.as_ref().unwrap()[0].loc.extension,
            Some("test_extension_3".to_string())
        );

        assert_eq!(
            selector_flattened.connections.as_ref().unwrap()[0]
                .cmd
                .as_ref()
                .unwrap()
                .len(),
            1
        );

        let cmd_flow = selector_flattened.connections.as_ref().unwrap()[0]
            .cmd
            .as_ref()
            .unwrap()[0]
            .clone();

        assert_eq!(cmd_flow.dest.len(), 2);
        // Check the two destinations.
        // One is test_extension_1, the other is test_extension_2.
        // The order is not guaranteed.
        assert!(
            cmd_flow.dest[0].loc.extension
                == Some("test_extension_1".to_string())
                || cmd_flow.dest[0].loc.extension
                    == Some("test_extension_2".to_string())
        );
        assert!(
            cmd_flow.dest[1].loc.extension
                == Some("test_extension_1".to_string())
                || cmd_flow.dest[1].loc.extension
                    == Some("test_extension_2".to_string())
        );

        assert_eq!(cmd_flow.dest[0].msg_conversion, None);
        assert_eq!(cmd_flow.dest[1].msg_conversion, None);

        assert_eq!(
            cmd_flow.dest[0].loc.app,
            Some("msgpack://127.0.0.1:8001/".to_string())
        );
        assert_eq!(
            cmd_flow.dest[1].loc.app,
            Some("msgpack://127.0.0.1:8001/".to_string())
        );
    }
}

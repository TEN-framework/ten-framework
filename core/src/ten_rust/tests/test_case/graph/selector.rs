//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use ten_rust::graph::graph_info::GraphContent;

    #[tokio::test]
    async fn test_graph_with_selector() {
        let mut graph_content = serde_json::from_str::<GraphContent>(include_str!(
            "../../test_data/graph_with_selector/graph_with_selector_1.json"
        ))
        .unwrap();

        graph_content.validate_and_complete_and_flatten(None).await.unwrap();

        let graph = graph_content.flattened_graph.as_ref().unwrap();

        // test_extension_1,2,3 --data--> test_extension_4
        // test_extension_3     --cmd --> test_extension_1,2
        // ----merged---
        // test_extension_1     --data--> test_extension_4
        // test_extension_2     --data--> test_extension_4
        // test_extension_3     --cmd --> test_extension_1,2
        //                      --data--> test_extension_4

        assert_eq!(graph.connections.as_ref().unwrap().len(), 3);

        // Get the connection of test_extension_1
        let connection = graph
            .connections
            .as_ref()
            .unwrap()
            .iter()
            .find(|c| c.loc.extension == Some("test_extension_1".to_string()))
            .unwrap();

        assert!(connection.data.is_some());
        let data = connection.data.as_ref().unwrap();
        assert_eq!(data.len(), 1);
        assert_eq!(data[0].name.as_deref(), Some("hi"));
        assert_eq!(data[0].dest.len(), 1);
        assert_eq!(data[0].dest[0].loc.extension, Some("test_extension_4".to_string()));

        let connection = graph
            .connections
            .as_ref()
            .unwrap()
            .iter()
            .find(|c| c.loc.extension == Some("test_extension_2".to_string()))
            .unwrap();
        assert!(connection.data.is_some());
        let data = connection.data.as_ref().unwrap();
        assert_eq!(data.len(), 1);
        assert_eq!(data[0].name.as_deref(), Some("hi"));
        assert_eq!(data[0].dest.len(), 1);
        assert_eq!(data[0].dest[0].loc.extension, Some("test_extension_4".to_string()));

        let connection = graph
            .connections
            .as_ref()
            .unwrap()
            .iter()
            .find(|c| c.loc.extension == Some("test_extension_3".to_string()))
            .unwrap();

        assert!(connection.cmd.is_some());
        let cmd = connection.cmd.as_ref().unwrap();
        assert_eq!(cmd.len(), 1);
        assert_eq!(cmd[0].name.as_deref(), Some("hello_world"));
        assert_eq!(cmd[0].dest.len(), 2);
        assert!(cmd[0]
            .dest
            .iter()
            .any(|d| d.loc.extension == Some("test_extension_1".to_string())));
        assert!(cmd[0]
            .dest
            .iter()
            .any(|d| d.loc.extension == Some("test_extension_2".to_string())));

        assert!(connection.data.is_some());
        let data = connection.data.as_ref().unwrap();
        assert_eq!(data.len(), 1);
        assert_eq!(data[0].name.as_deref(), Some("hi"));
        assert_eq!(data[0].dest.len(), 1);
        assert_eq!(data[0].dest[0].loc.extension, Some("test_extension_4".to_string()));
    }

    #[tokio::test]
    async fn test_get_nodes_by_selector_node_name() {
        use ten_rust::graph::Graph;

        let graph_str =
            include_str!("../../test_data/graph_with_selector/graph_with_selector_1.json");
        let graph = serde_json::from_str::<Graph>(graph_str).unwrap();
        let nodes = graph.get_nodes_by_selector_node_name("selector_for_ext_1_and_2").unwrap();
        assert_eq!(nodes.len(), 2);
        assert_eq!(nodes[0].get_name(), "test_extension_1");
        assert_eq!(nodes[1].get_name(), "test_extension_2");

        let nodes =
            graph.get_nodes_by_selector_node_name("selector_for_ext_1_and_2_and_3").unwrap();
        assert_eq!(nodes.len(), 3);
        assert_eq!(nodes[0].get_name(), "test_extension_1");
        assert_eq!(nodes[1].get_name(), "test_extension_2");
        assert_eq!(nodes[2].get_name(), "test_extension_3");

        let nodes = graph.get_nodes_by_selector_node_name("selector_for_ext_1_or_3").unwrap();
        assert_eq!(nodes.len(), 2);
        assert_eq!(nodes[0].get_name(), "test_extension_1");
        assert_eq!(nodes[1].get_name(), "test_extension_3");
    }
}

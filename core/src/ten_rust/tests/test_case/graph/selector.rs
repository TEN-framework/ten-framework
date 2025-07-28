//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use ten_rust::graph::Graph;

    #[tokio::test]
    async fn test_graph_with_selector() {
        let graph = Graph::from_str_with_base_dir(
            include_str!(
                "../../test_data/graph_with_selector/graph_with_selector_1.\
                 json"
            ),
            None,
        )
        .await
        .unwrap();

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
        assert_eq!(data[0].name, "hi");
        assert_eq!(data[0].dest.len(), 1);
        assert_eq!(
            data[0].dest[0].loc.extension,
            Some("test_extension_4".to_string())
        );

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
        assert_eq!(data[0].name, "hi");
        assert_eq!(data[0].dest.len(), 1);
        assert_eq!(
            data[0].dest[0].loc.extension,
            Some("test_extension_4".to_string())
        );

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
        assert_eq!(cmd[0].name, "hello_world");
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
        assert_eq!(data[0].name, "hi");
        assert_eq!(data[0].dest.len(), 1);
        assert_eq!(
            data[0].dest[0].loc.extension,
            Some("test_extension_4".to_string())
        );
    }
}

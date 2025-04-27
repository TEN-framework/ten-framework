//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use std::collections::HashMap;

    use ten_rust::pkg_info::property::parse_property_from_str;

    #[test]
    fn test_property_predefined_graph_deserialize() {
        let mut graphs_cache = HashMap::new();

        let property_json_str =
            include_str!("../../../test_data/property.json");
        let property = parse_property_from_str(
            property_json_str,
            &mut graphs_cache,
            None,
            None,
            None,
        )
        .unwrap();
        assert!(property.ten.is_some());

        assert!(graphs_cache.len() == 1);

        let (_, graph_info) = graphs_cache.into_iter().next().unwrap();

        let property_json_value = serde_json::to_value(&graph_info).unwrap();
        assert_eq!(
            property_json_value.as_object().unwrap()["connections"]
                .as_array()
                .unwrap()
                .len(),
            1
        );
    }
}

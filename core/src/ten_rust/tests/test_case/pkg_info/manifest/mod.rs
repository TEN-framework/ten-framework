//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use anyhow::Result;

    use ten_rust::pkg_info::{manifest::Manifest, pkg_type::PkgType};

    #[test]
    fn test_extension_manifest_from_str() {
        let manifest_str =
            include_str!("../../../test_data/test_extension_manifest.json");

        let result: Result<Manifest> = manifest_str.parse();
        assert!(result.is_ok());

        let manifest = result.unwrap();
        assert_eq!(manifest.type_and_name.pkg_type, PkgType::Extension);

        let cmd_in = manifest.api.unwrap().cmd_in.unwrap();
        assert_eq!(cmd_in.len(), 1);

        let required = cmd_in[0].required.as_ref();
        assert!(required.is_some());
        assert_eq!(required.unwrap().len(), 1);
    }
}

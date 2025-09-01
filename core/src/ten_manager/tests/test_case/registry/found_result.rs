//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use ten_manager::registry::found_result::*;
    use ten_rust::pkg_info::pkg_type::PkgType;

    #[test]
    fn test_found_result_deserialize() {
        let json_str = r#"
        {
          "type": "addon_loader",
          "name": "python_addon_loader",
          "version": "0.10.18",
          "display_name": {
            "locales": {
              "en-US": {
                "content": "Python Addon Loader"
              },
              "zh-CN": {
                "content": "Python 插件加载器"
              },
              "zh-TW": {
                "content": "Python 外掛載入器"
              },
              "ja-JP": {
                "content": "Python アドオンローダー"
              },
              "ko-KR": {
                "content": "Python 애드온 로더"
              }
            }
          },
          "supports": [
            {
              "os": "linux",
              "arch": "x64"
            }
          ],
          "dependencies": [
            {
              "type": "system",
              "name": "ten_runtime",
              "version": "^0.10.18"
            },
            {
              "type": "system",
              "name": "ten_runtime_python",
              "version": "^0.10.18"
            }
          ],
          "hash": "9b0c57cb0032857e3c16a5d30b04fbc93bd90f608ffda5d6525de83f10739e85",
          "downloadUrl": "https://rte-store.s3.amazonaws.com/ten-packages/addon_loader-python_addon_loader-0.10.189b0c57cb0032857e3c16a5d30b04fbc93bd90f608ffda5d6525de83f10739e85.tpkg"
        }
        "#;

        let found_result: PkgRegistryInfo = serde_json::from_str(json_str).unwrap();
        assert_eq!(found_result.basic_info.type_and_name.pkg_type, PkgType::AddonLoader);
        assert_eq!(found_result.basic_info.type_and_name.name, "python_addon_loader");
        assert_eq!(found_result.dependencies.len(), 2);
        assert_eq!(found_result.display_name.as_ref().unwrap().locales.len(), 5);
        assert_eq!(
            found_result.hash,
            "9b0c57cb0032857e3c16a5d30b04fbc93bd90f608ffda5d6525de83f10739e85"
        );
        assert_eq!(found_result.download_url, "https://rte-store.s3.amazonaws.com/ten-packages/addon_loader-python_addon_loader-0.10.189b0c57cb0032857e3c16a5d30b04fbc93bd90f608ffda5d6525de83f10739e85.tpkg");
    }

    #[test]
    fn test_found_result_deserialize_with_unknown_fields() {
        let json_str = r#"
        {
          "type": "addon_loader",
          "name": "python_addon_loader",
          "version": "0.10.18",
          "display_name": {
            "locales": {
              "en-US": {
                "content": "Python Addon Loader"
              },
              "zh-CN": {
                "content": "Python 插件加载器"
              },
              "zh-TW": {
                "content": "Python 外掛載入器"
              },
              "ja-JP": {
                "content": "Python アドオンローダー"
              },
              "ko-KR": {
                "content": "Python 애드온 로더"
              }
            }
          },
          "supports": [
            {
              "os": "linux",
              "arch": "x64"
            }
          ],
          "dependencies": [
            {
              "type": "system",
              "name": "ten_runtime",
              "version": "^0.10.18"
            },
            {
              "type": "system",
              "name": "ten_runtime_python",
              "version": "^0.10.18"
            }
          ],
          "hash": "9b0c57cb0032857e3c16a5d30b04fbc93bd90f608ffda5d6525de83f10739e85",
          "downloadUrl": "https://rte-store.s3.amazonaws.com/ten-packages/addon_loader-python_addon_loader-0.10.189b0c57cb0032857e3c16a5d30b04fbc93bd90f608ffda5d6525de83f10739e85.tpkg",
          "unknown_field": "unknown_value"
        }
        "#;

        let found_result: PkgRegistryInfo = serde_json::from_str(json_str).unwrap();
        assert_eq!(found_result.basic_info.type_and_name.pkg_type, PkgType::AddonLoader);
        assert_eq!(found_result.basic_info.type_and_name.name, "python_addon_loader");
        assert_eq!(found_result.dependencies.len(), 2);
        assert_eq!(found_result.display_name.as_ref().unwrap().locales.len(), 5);
        assert_eq!(
            found_result.hash,
            "9b0c57cb0032857e3c16a5d30b04fbc93bd90f608ffda5d6525de83f10739e85"
        );
        assert_eq!(found_result.download_url, "https://rte-store.s3.amazonaws.com/ten-packages/addon_loader-python_addon_loader-0.10.189b0c57cb0032857e3c16a5d30b04fbc93bd90f608ffda5d6525de83f10739e85.tpkg");
    }
}

//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use ten_rust::json_schema::{
        ten_validate_manifest_json_string, ten_validate_property_json_string,
        validate_manifest_lock_json_string,
    };

    #[test]
    fn test_validate_default_cpp_app() {
        let manifest = r#"
        {
          "type": "app",
          "name": "default_app_cpp",
          "version": "0.1.0",
          "dependencies": []
        }
        "#;
        let result = ten_validate_manifest_json_string(manifest);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_dependencies_normal() {
        let manifest = r#"
        {
          "type": "extension",
          "name": "default_extension_cpp",
          "version": "0.1.0",
          "dependencies": [{
            "type": "system",
            "name": "ten_runtime",
            "version": "0.6.0"
          }],
          "api": {}
        }
        "#;
        let result = ten_validate_manifest_json_string(manifest);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_dev_dependencies_normal() {
        let manifest = r#"
        {
          "type": "extension",
          "name": "default_extension_cpp",
          "version": "0.1.0",
          "dev_dependencies": [{
            "type": "system",
            "name": "googletest",
            "version": "1.7.0-rc2"
          }],
          "api": {}
        }
        "#;
        let result = ten_validate_manifest_json_string(manifest);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_mixed_dependencies_normal() {
        let manifest = r#"
        {
          "type": "extension",
          "name": "default_extension_cpp",
          "version": "0.1.0",
          "dev_dependencies": [{
            "type": "system",
            "name": "ten_runtime",
            "version": "0.6.0"
          },{
            "type": "system",
            "name": "googletest",
            "version": "1.7.0-rc2"
          }],
          "api": {}
        }
        "#;
        let result = ten_validate_manifest_json_string(manifest);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_dependencies_with_path() {
        let manifest = r#"
        {
          "type": "app",
          "name": "default_app_cpp",
          "version": "0.1.0",
          "dependencies": [{
            "path": "path/to/dependency"
          }]
        }
        "#;
        let result = ten_validate_manifest_json_string(manifest);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_dependencies_normal_and_with_path() {
        let manifest = r#"
        {
          "type": "app",
          "name": "default_app_cpp",
          "version": "0.1.0",
          "dependencies": [{
            "type": "system",
            "name": "ten_runtime",
            "version": "0.6.0"
          },{
            "path": "path/to/dependency"
          }]
        }
        "#;
        let result = ten_validate_manifest_json_string(manifest);
        assert!(result.is_ok());
    }

    #[test]
    fn test_only_app_needs_predefined_graphs() {
        let manifest = r#"
        {
          "type": "extension",
          "name": "default_extension_cpp",
          "version": "0.1.0",
          "dependencies": []
        }
        "#;
        let result = ten_validate_manifest_json_string(manifest);
        assert!(result.is_ok());
    }

    #[test]
    fn test_only_app_needs_predefined_graphs_supports() {
        let manifest = r#"
        {
          "type": "extension",
          "name": "default_extension_cpp",
          "version": "0.1.0",
          "dependencies": [],
          "supports": []
        }
        "#;
        let result = ten_validate_manifest_json_string(manifest);
        assert!(result.is_ok());
    }

    #[test]
    fn test_only_app_needs_predefined_graphs_supports_with_content() {
        let manifest = r#"
        {
          "type": "extension",
          "name": "default_extension_cpp",
          "version": "0.1.0",
          "dependencies": [],
          "supports": [{"os": "linux", "arch": "x64"}]
        }
        "#;
        let result = ten_validate_manifest_json_string(manifest);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_invalid_type() {
        let manifest = r#"
        {
          "type": "invalid",
          "name": "default_app_cpp",
          "version": "0.1.0",
          "dependencies": [],
          "api": {}
        }
        "#;

        let result = ten_validate_manifest_json_string(manifest);
        assert!(result.is_err());
    }

    #[test]
    fn test_validate_invalid_graph_invalid_additional_property() {
        let property = r#"
        {
          "ten": {
            "predefined_graphs": [
              {
                "name": "default",
                "nodes": [
                  {
                    "type": "extension",
                    "name": "default_extension_cpp",
                    "addon": "default_extension_cpp",
                    "extension_group": "default_extension_group",
                    "should_not_present": "aa"
                  }
                ]
              }
            ]
          }
        }
        "#;

        let result = ten_validate_property_json_string(property);
        assert!(result.is_err());

        let msg = result.unwrap_err().to_string();
        assert!(msg.contains("Additional properties are not allowed"));
    }

    #[test]
    fn test_validate_invalid_command_name_empty() {
        let property = r#"{
  "ten": {
    "predefined_graphs": [
      {
        "name": "default",
        "graph": {
          "nodes": [
            {
              "type": "extension",
              "name": "default_extension_cpp",
              "addon": "default_extension_cpp",
              "extension_group": "default_extension_group"
            }
          ],
          "connections": [
            {
              "extension": "default_extension_cpp",
              "cmd": [
                {
                  "name": "",
                  "dest": [
                    {
                      "extension": "default_extension_cpp"
                    }
                  ]
                }
              ]
            }
          ]
        }
      }
    ]
  }
}"#;

        let result = ten_validate_property_json_string(property);
        assert!(result.is_err());

        let msg = result.unwrap_err().to_string();
        assert!(msg.contains("is shorter than 1 character"));
    }

    #[test]
    fn test_validate_invalid_cmd_no_dest() {
        let property = r#"{
  "ten": {
    "predefined_graphs": [
      {
        "name": "default",
        "graph": {
          "nodes": [
            {
              "type": "extension",
              "name": "default_extension_cpp",
              "addon": "default_extension_cpp",
              "extension_group": "default_extension_group"
            }
          ],
          "connections": [
            {
              "extension": "default_extension_cpp",
              "cmd": [
                {
                  "name": "demo",
                  "dest": []
                }
              ]
            }
          ]
        }
      }
    ]
  }
}"#;

        let result = ten_validate_property_json_string(property);
        assert!(result.is_err());

        let msg = result.unwrap_err().to_string();
        assert!(msg.contains("[] has less than 1 item"));
    }

    #[test]
    fn test_validate_extension_property() {
        let property = r#"{
  "ten": {
    "predefined_graphs": [
      {
        "name": "default",
        "graph": {
          "nodes": [
            {
              "type": "extension",
              "name": "default_extension_cpp",
              "addon": "default_extension_cpp",
              "extension_group": "default_extension_group",
              "property": {
                "a": 1
              }
            }
          ]
        }
      }
    ]
  }
}"#;

        let result = ten_validate_property_json_string(property);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_extension_no_cmds() {
        let property = r#"{
  "ten": {
    "predefined_graphs": [
      {
        "name": "default",
        "graph": {
          "nodes": [
            {
              "type": "extension",
              "name": "default_extension_cpp",
              "addon": "default_extension_cpp",
              "extension_group": "default_extension_group"
            }
          ]
        }
      }
    ]
  }
}"#;

        let result = ten_validate_property_json_string(property);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_api_cmd_in_success_1() {
        let manifest = r#"
        {
          "type": "extension",
          "name": "default_extension_cpp",
          "version": "0.1.0",
          "dependencies": [],
          "api": {
            "cmd_in": [
              {
                "name": "foo",
                "property": {
                  "properties": {
                    "a": {
                      "type": "int8"
                    },
                    "b": {
                      "type": "uint8"
                    },
                    "c": {
                      "type": "array",
                      "items": {
                        "type": "string"
                      }
                    },
                    "d": {
                      "type": "object",
                      "properties": {
                        "e": {
                          "type": "float32"
                        }
                      }
                    }
                  }
                },
                "result": {
                  "property": {
                    "properties": {
                      "a": {
                        "type": "buf"
                      },
                      "detail": {
                        "type": "buf"
                      }
                    }
                  }
                }
              }
            ]
          }
        }
        "#;

        let result = ten_validate_manifest_json_string(manifest);
        println!("result: {result:?}");
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_api_cmd_in_success_2() {
        let manifest = r#"
        {
          "type": "extension",
          "name": "default_extension_cpp",
          "version": "0.1.0",
          "dependencies": [],
          "api": {
            "cmd_in": [
              {
                "name": "foo",
                "property": {
                  "properties": {
                    "a": {
                      "type": "int8"
                    },
                    "b": {
                      "type": "uint8"
                    },
                    "c": {
                      "type": "array",
                      "items": {
                        "type": "string"
                      }
                    },
                    "d": {
                      "type": "object",
                      "properties": {
                        "e": {
                          "type": "float32"
                        }
                      }
                    }
                  },
                  "required": ["a","b"]
                },
                "result": {
                  "property": {
                    "properties": {
                      "a": {
                        "type": "buf"
                      },
                      "detail": {
                        "type": "buf"
                      }
                    }
                  }
                }
              }
            ]
          }
        }
        "#;

        let result = ten_validate_manifest_json_string(manifest);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_api_cmd_in_success_3() {
        let manifest = r#"
        {
          "type": "extension",
          "name": "default_extension_cpp",
          "version": "0.1.0",
          "dependencies": [],
          "api": {
            "cmd_in": [
              {
                "name": "foo",
                "property": {
                  "properties": {
                    "a": {
                      "type": "int8"
                    },
                    "b": {
                      "type": "uint8"
                    },
                    "c": {
                      "type": "array",
                      "items": {
                        "type": "string"
                      }
                    },
                    "d": {
                      "type": "object",
                      "properties": {
                        "e": {
                          "type": "float32"
                        }
                      }
                    }
                  },
                  "required": ["a","b"]
                },
                "result": {
                  "property": {
                    "properties": {
                      "a": {
                        "type": "buf"
                      },
                      "detail": {
                        "type": "buf"
                      }
                    },
                    "required": ["a"]
                  }
                }
              }
            ]
          }
        }
        "#;

        let result = ten_validate_manifest_json_string(manifest);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_api_cmd_in_has_nested_object_required() {
        let manifest = r#"
        {
          "type": "extension",
          "name": "default_extension_cpp",
          "version": "0.1.0",
          "dependencies": [],
          "api": {
            "cmd_in": [
              {
                "name": "foo",
                "property": {
                  "properties": {
                    "a": {
                      "type": "int8"
                    },
                    "b": {
                      "type": "uint8"
                    },
                    "c": {
                      "type": "array",
                      "items": {
                        "type": "string"
                      }
                    },
                    "d": {
                      "type": "object",
                      "properties": {
                        "e": {
                          "type": "float32"
                        },
                        "f": {
                          "type": "string"
                        }
                      },
                      "required": ["e"]
                    }
                  },
                  "required": ["a","b"]
                },
                "result": {
                  "property": {
                    "properties": {
                      "a": {
                        "type": "buf"
                      },
                      "detail": {
                        "type": "buf"
                      }
                    }
                  }
                }
              }
            ]
          }
        }
        "#;

        let result = ten_validate_manifest_json_string(manifest);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_api_cmd_in_fail_1() {
        let manifest = r#"
        {
          "type": "app",
          "name": "default_app_cpp",
          "version": "0.1.0",
          "dependencies": [],
          "api": {
            "cmd_in": [
              {
                "name": "foo",
                "property": {
                  "//a": {
                    "type": "int8"
                  },
                  "b": {
                    "type": "uint8"
                  },
                  "c": {
                    "type": "array",
                    "items": {
                      "type": "string"
                    }
                  },
                  "d": {
                    "type": "object",
                    "properties": {
                      "e": {
                        "type": "float32"
                      }
                    }
                  }
                },
                "result": {
                  "property": {
                    "a": {
                      "type": "buf"
                    },
                    "detail": {
                      "type": "buf"
                    }
                  }
                }
              }
            ]
          }
        }
        "#;

        let result = ten_validate_manifest_json_string(manifest);
        assert!(result.is_err());
    }

    #[test]
    fn test_validate_api_cmd_in_fail_2() {
        let manifest = r#"
        {
          "type": "app",
          "name": "default_app_cpp",
          "version": "0.1.0",
          "dependencies": [],
          "api": {
            "cmd_in": [
              {
                "name": "foo",
                "property": {
                  "a": {
                    "type": "int8"
                  },
                  "b": {
                    "type": "uint8"
                  },
                  "c": {
                    "type": "array",
                    "items": {
                      "type": "string"
                    }
                  },
                  "d": {
                    "type": "object",
                    "properties": {
                      "//e": {
                        "type": "float32"
                      }
                    }
                  }
                },
                "result": {
                  "property": {
                    "a": {
                      "type": "buf"
                    },
                    "detail": {
                      "type": "buf"
                    }
                  }
                }
              }
            ]
          }
        }
        "#;

        let result = ten_validate_manifest_json_string(manifest);
        assert!(result.is_err());
    }

    #[test]
    fn test_validate_api_cmd_in_fail_3() {
        let manifest = r#"
        {
          "type": "app",
          "name": "default_app_cpp",
          "version": "0.1.0",
          "dependencies": [],
          "api": {
            "cmd_in": [
              {
                "name": "foo",
                "property": {
                  "a": {
                    "type": "int8"
                  },
                  "b": {
                    "type": "uint8"
                  },
                  "c": {
                    "type": "array",
                    "items": {
                      "type": "string"
                    }
                  },
                  "d": {
                    "type": "object",
                    "properties": {
                      "e": {
                        "type": "float32"
                      }
                    }
                  }
                },
                "result": {
                  "property": {
                    "//a": {
                      "type": "buf"
                    },
                    "detail": {
                      "type": "buf"
                    }
                  }
                }
              }
            ]
          }
        }
        "#;

        let result = ten_validate_manifest_json_string(manifest);
        assert!(result.is_err());
    }

    #[test]
    fn test_manifest_validate_api_cmd_in_result_must_have_prop() {
        let manifest = r#"
        {
          "type": "extension",
          "name": "default_extension_cpp",
          "version": "0.1.0",
          "dependencies": [],
          "api": {
            "cmd_in": [
              {
                "name": "foo",
                "property": {
                  "a": {
                    "type": "int8"
                  },
                  "b": {
                    "type": "uint8"
                  },
                  "c": {
                    "type": "array",
                    "items": {
                      "type": "string"
                    }
                  },
                  "d": {
                    "type": "object",
                    "properties": {
                      "e": {
                        "type": "float32"
                      }
                    }
                  }
                },
                "result": {
                }
              }
            ]
          }
        }
        "#;

        let result = ten_validate_manifest_json_string(manifest);
        assert!(result.is_err());
    }

    #[test]
    fn test_manifest_validate_api_cmd_in_has_additional_prop() {
        let manifest = r#"
        {
          "type": "extension",
          "name": "embedding",
          "version": "0.1.0",
  "dependencies": [
            {
              "type": "system",
              "name": "ten_runtime_python",
              "version": "0.2.2"
            }
          ],
          "api": {
            "property": {
              "api_key": {
                "type": "string"
              },
              "model": {
                "type": "string"
              }
            },
            "cmd_in": [
              {
                "name": "embed",
                "property": {
                  "input": {
                    "type": "string"
                  }
                },
                "status": {
                  "property": {
                    "output": {
                      "type": "string"
                    },
                    "code": {
                      "type": "string"
                    },
                    "message": {
                      "type": "string"
                    }
                  }
                }
              },
              {
                "name": "embed_batch",
                "property": {
                  "inputs": {
                    "type": "array",
                    "items": {
                      "type": "string"
                    }
                  }
                },
                "status": {
                  "property": {
                    "output": {
                      "type": "string"
                    },
                    "code": {
                      "type": "string"
                    },
                    "message": {
                      "type": "string"
                    }
                  }
                }
              }
            ],
            "cmd_out": []
          }
        }
        "#;

        let result = ten_validate_manifest_json_string(manifest);
        assert!(result.is_err());

        let msg = result.unwrap_err().to_string();
        assert!(msg.contains(
            "Additional properties are not allowed ('status' was unexpected) @ /api/cmd_in/0"
        ));
    }

    #[test]
    fn test_validate_api_only_array_has_items() {
        let manifest = r#"
        {
          "type": "app",
          "name": "default_app_cpp",
          "version": "0.1.0",
          "dependencies": [],
          "api": {
            "cmd_in": [
              {
                "name": "foo",
                "property": {
                  "properties": {
                    "a": {
                      "type": "int8",
                      "items": {
                        "type": "string"
                      }
                    }
                  }
                }
              }
            ]
          }
        }
        "#;

        let result = ten_validate_manifest_json_string(manifest);
        assert!(result.is_err());

        assert!(result
            .unwrap_err()
            .to_string()
            .contains("{\"required\":[\"items\"]} is not allowed for"));
    }

    #[test]
    fn test_validate_api_only_object_has_properties() {
        let manifest = r#"
        {
          "type": "app",
          "name": "default_app_cpp",
          "version": "0.1.0",
          "dependencies": [],
          "api": {
            "cmd_in": [
              {
                "name": "foo",
                "property": {
                  "properties": {
                    "a": {
                      "type": "string",
                      "properties": {
                        "a": {
                          "type": "string"
                        }
                      }
                    }
                  }
                }
              }
            ]
          }
        }
        "#;

        let result = ten_validate_manifest_json_string(manifest);
        assert!(result.is_err());

        assert!(result
            .unwrap_err()
            .to_string()
            .contains("{\"required\":[\"properties\"]} is not allowed for"));
    }

    #[test]
    fn test_validate_interface_empty() {
        let manifest = r#"
        {
          "type": "extension",
          "name": "default_extension_cpp",
          "version": "0.1.0",
          "dependencies": [],
          "api": {
            "interface": []
          }
        }
        "#;

        let result = ten_validate_manifest_json_string(manifest);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_additional_properties() {
        let manifest = r#"
        {
          "type": "extension",
          "name": "default_extension_cpp",
          "version": "0.1.0",
          "dependencies": [],
          "interface": []
        }
        "#;

        let result = ten_validate_manifest_json_string(manifest);
        assert!(result.is_err());

        assert!(result.unwrap_err().to_string().contains("Additional properties are not allowed"));
    }

    #[test]
    fn test_validate_interface_wrong_type() {
        let manifest = r#"
        {
          "type": "extension",
          "name": "default_extension_cpp",
          "version": "0.1.0",
          "dependencies": [],
          "api": {
            "interface": "interface.json"
          }
        }
        "#;

        let result = ten_validate_manifest_json_string(manifest);
        assert!(result.is_err());

        assert!(result.unwrap_err().to_string().contains("is not of type"));
    }

    #[test]
    fn test_validate_interface_with_relative_path_import_uri() {
        let manifest = r#"
        {
          "type": "extension",
          "name": "default_extension_cpp",
          "version": "0.1.0",
          "dependencies": [],
          "api": {
            "interface": [
              {
                "import_uri": "interface.json"
              }
            ]
          }
        }
        "#;

        let result = ten_validate_manifest_json_string(manifest);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_interface_with_absolute_path_import_uri() {
        let manifest = r#"
        {
          "type": "extension",
          "name": "default_extension_cpp",
          "version": "0.1.0",
          "dependencies": [],
          "api": {
            "interface": [
              {
                "import_uri": "file:///tmp/interface.json"
              }
            ]
          }
        }
        "#;

        let result = ten_validate_manifest_json_string(manifest);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_interface_with_remote_url() {
        let manifest = r#"
        {
          "type": "extension",
          "name": "default_extension_cpp",
          "version": "0.1.0",
          "dependencies": [],
          "api": {
            "interface": [
              {
                "import_uri": "https://example.com/interface.json"
              }
            ]
          }
        }
        "#;

        let result = ten_validate_manifest_json_string(manifest);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_interface_combined_with_cmd_in() {
        let manifest = r#"
        {
          "type": "extension",
          "name": "default_extension_cpp",
          "version": "0.1.0",
          "dependencies": [],
          "api": {
            "interface": [
              {
                "import_uri": "https://example.com/interface.json"
              }
            ],
            "cmd_in": [
              {
                "name": "foo",
                "property": {}
              }
            ]
          }
        }
        "#;

        let result = ten_validate_manifest_json_string(manifest);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_interface_with_extra_fields() {
        let manifest = r#"
        {
          "type": "extension",
          "name": "default_extension_cpp",
          "version": "0.1.0",
          "dependencies": [],
          "api": {
            "interface": [
              {
                "import_uri": "https://example.com/interface.json",
                "extra": "extra"
              }
            ]
          }
        }
        "#;

        let result = ten_validate_manifest_json_string(manifest);
        assert!(result.is_err());

        assert!(result.unwrap_err().to_string().contains("Additional properties are not allowed"));
    }

    #[test]
    fn test_validate_interface_without_import_uri() {
        let manifest = r#"
        {
          "type": "extension",
          "name": "default_extension_cpp",
          "version": "0.1.0",
          "dependencies": [],
          "api": {
            "interface": [
              {}
            ]
          }
        }
        "#;

        let result = ten_validate_manifest_json_string(manifest);
        assert!(result.is_err());

        assert!(result.unwrap_err().to_string().contains("is a required property"));
    }

    #[test]
    fn test_graph_msg_conversions() {
        let property = r#"
        {
          "ten": {
            "predefined_graphs": [{
              "name": "default",
              "auto_start": false,
              "graph": {
                "nodes": [{
                  "type": "extension",
                  "name": "test_extension_1",
                  "addon": "result_mapping_1__test_extension_1",
                  "extension_group": "result_mapping_1__extension_group"
                },{
                  "type": "extension",
                  "name": "test_extension_2",
                  "addon": "result_mapping_1__test_extension_2",
                  "extension_group": "result_mapping_1__extension_group"
                }],
                "connections": [{
                  "app": "msgpack://127.0.0.1:8001/",
                  "extension": "test_extension_1",
                  "cmd": [{
                    "name": "hello_world",
                    "dest": [{
                      "app": "msgpack://127.0.0.1:8001/",
                      "extension": "test_extension_2",
                      "msg_conversion": {
                        "type": "per_property",
                        "rules": [{
                          "path": "ten.name",
                          "conversion_mode": "fixed_value",
                          "value": "hello mapping"
                        },{
                          "path": "test_group.test_property_name",
                          "conversion_mode": "from_original",
                          "original_path": "test_property"
                        }],
                        "result": {
                          "type": "per_property",
                          "rules": [{
                            "path": "resp_group.resp_property_name",
                            "conversion_mode": "from_original",
                            "original_path": "resp_property"
                          }]
                        }
                      }
                    }]
                  }]
                }]
              }
            }]
          }
        }
        "#;

        let result = ten_validate_property_json_string(property);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_property_cmd_must_be_alphanumeric() {
        let property = r#"{
  "ten": {
    "predefined_graphs": [
      {
        "name": "default",
        "graph": {
          "nodes": [
            {
              "type": "extension",
              "name": "default_extension_cpp",
              "addon": "default_extension_cpp",
              "extension_group": "default_extension_group"
            }
          ],
          "connections": [
            {
              "extension": "default_extension_cpp",
              "cmd": [
                {
                  "name": "invalid command",
                  "dest": [
                    {
                      "extension": "default_extension_cpp"
                    }
                  ]
                }
              ]
            }
          ]
        }
      }
    ]
  }
}"#;

        let result = ten_validate_property_json_string(property);
        assert!(result.is_err());

        assert!(result.unwrap_err().to_string().contains("does not match"));
    }

    #[test]
    fn test_validate_property_json_valid() {
        let property = r#"
        {
          "ten": {
            "log": {
              "handlers": [
                {
                  "matchers": [
                    {
                      "level": "info"
                    }
                  ],
                  "formatter": {
                    "type": "json",
                    "colored": false
                  },
                  "emitter": {
                    "type": "file",
                    "config": {
                      "path": "api.log"
                    }
                  }
                }
              ]
            }
          },
          "a": 1,
          "b": "2",
          "c": [1, 2, 3],
          "d": {
            "e": 1.0
          }
        }
        "#;

        let result = ten_validate_property_json_string(property);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_property_key_can_be_any_string() {
        // Test that property keys with hyphens are now allowed
        let property = r#"
        {
          "invalid-key": 1
        }
        "#;

        let result = ten_validate_property_json_string(property);
        if let Err(e) = &result {
            println!("Error: {}", e);
        }
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_property_key_with_special_characters() {
        // Test property key with hyphens and special characters
        let property = r#"
        {
          "x-amzn-sagemaker-custom-attributes": "X-Path:/v1/chat/completions",
          "api-key": "test-value",
          "content-type": "application/json"
        }
        "#;

        let result = ten_validate_property_json_string(property);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_property_key_starting_with_number() {
        // Test property key starting with a number
        let property = r#"
        {
          "123": "value",
          "456abc": "another-value"
        }
        "#;

        let result = ten_validate_property_json_string(property);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_property_nested_object_with_non_alphanumeric_keys() {
        // Test nested objects with non-alphanumeric keys
        let property = r#"
        {
          "outer-key": {
            "inner-key": "value",
            "123": 456,
            "kebab-case-key": {
              "deep-nested-key": "deep-value"
            }
          }
        }
        "#;

        let result = ten_validate_property_json_string(property);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_graph_node_property_with_non_alphanumeric_keys() {
        // Test that graphNode property can have non-alphanumeric keys
        let property = r#"
        {
          "ten": {
            "predefined_graphs": [{
              "name": "default",
              "auto_start": true,
              "graph": {
                "nodes": [{
                  "type": "extension",
                  "name": "test_extension",
                  "addon": "test_addon",
                  "property": {
                    "server-port": 8080,
                    "x-custom-header": "header-value",
                    "123": "numeric-key",
                    "nested-object": {
                      "inner-key": "inner-value",
                      "456": 789
                    }
                  }
                }]
              }
            }]
          }
        }
        "#;

        let result = ten_validate_property_json_string(property);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_real_world_property_example() {
        // Test the actual property.json from the example directory
        let property = r#"
{
  "ten": {
    "log": {
      "handlers": [
        {
          "matchers": [
            {
              "level": "info"
            }
          ],
          "formatter": {
            "type": "plain",
            "colored": true
          },
          "emitter": {
            "type": "console",
            "config": {
              "stream": "stdout"
            }
          }
        }
      ]
    },
    "predefined_graphs": [
      {
        "name": "default",
        "auto_start": true,
        "graph": {
          "nodes": [
            {
              "type": "extension",
              "name": "aio_http_server_python",
              "addon": "aio_http_server_python",
              "extension_group": "test",
              "property": {
                "server_port": 8002,
                "header": {
                  "x-amzn-sagemaker-custom-attributes": "X-Path:/v1/chat/completions"
                },
                "123": "ahfiaos"
              }
            },
            {
              "type": "extension",
              "name": "simple_echo_cpp",
              "addon": "simple_echo_cpp",
              "extension_group": "default_extension_group"
            }
          ],
          "connections": [
            {
              "extension": "aio_http_server_python",
              "cmd": [
                {
                  "name": "test",
                  "dest": [
                    {
                      "extension": "simple_echo_cpp"
                    }
                  ]
                }
              ]
            }
          ]
        }
      }
    ]
  }
}
        "#;

        let result = ten_validate_property_json_string(property);
        if let Err(e) = &result {
            println!("Error: {}", e);
        }
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_manifest_lock_empty() {
        let manifest_lock = r#"{}"#;

        // The 'packages' field is required in the manifest lock file.
        let result = validate_manifest_lock_json_string(manifest_lock);
        assert!(result.is_err());
    }

    #[test]
    fn test_validate_manifest_lock_empty_packages() {
        let manifest_lock = r#"{
          "version": 1,
          "packages": []
        }"#;

        let result = validate_manifest_lock_json_string(manifest_lock);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_manifest_lock_invalid_lock_version() {
        let manifest_lock = r#"{
          "version": 0,
          "packages": []
        }"#;

        let result = validate_manifest_lock_json_string(manifest_lock);
        assert!(result.is_err());
    }

    #[test]
    fn test_validate_manifest_lock_duplicated_packages() {
        // NOTE: The 'type' combined with 'name' must be unique. But the json
        // schema check does not support this. The check should be done
        // in the code.
        let manifest_lock = r#"{
          "version": 1,
          "packages": [
            {
              "type": "extension",
              "name": "ext_a",
              "version": "1.0.0",
              "hash": "e8dc07a47927e9a650d23f77676b798e0856dd169fea70e7db57d57095261a68"
            },
            {
              "type": "extension",
              "name": "ext_a",
              "version": "1.0.0",
              "hash": "e8dc07a47927e9a650d23f77676b798e0856dd169fea70e7db57d57095261a68"
            }
          ]
        }"#;

        let result = validate_manifest_lock_json_string(manifest_lock);
        assert!(result.is_err());

        let err_reason = result.unwrap_err().to_string();
        assert!(err_reason.contains("has non-unique elements"));
    }

    #[test]
    fn test_validate_manifest_lock_missing_field_in_pkg() {
        // Miss hash field in the package.
        let manifest_lock = r#"{
          "version": 1,
          "packages": [
            {
              "type": "extension",
              "name": "ext_a",
              "version": "1.0.0"
            }
          ]
        }"#;

        let result = validate_manifest_lock_json_string(manifest_lock);
        assert!(result.is_err());

        let err_reason = result.unwrap_err().to_string();
        assert!(err_reason.contains("is a required property"));
    }

    #[test]
    fn test_validate_manifest_lock_invalid_version() {
        // The version field must be a fixed version but not a range.
        let manifest_lock = r#"{
          "version": 1,
          "packages": [
            {
              "type": "extension",
              "name": "ext_a",
              "version": ">1.0.0",
              "hash": "e8dc07a47927e9a650d23f77676b798e0856dd169fea70e7db57d57095261a68"
            }
          ]
        }"#;

        let result = validate_manifest_lock_json_string(manifest_lock);
        assert!(result.is_err());

        let err_reason = result.unwrap_err().to_string();
        assert!(err_reason.contains("does not match"));
    }

    #[test]
    fn test_validate_manifest_lock_pre_release_version() {
        // The pre-release version is allowed.
        let manifest_lock = r#"{
          "version": 1,
          "packages": [
            {
              "type": "extension",
              "name": "ext_a",
              "version": "0.1.0-rc.1",
              "hash": "e8dc07a47927e9a650d23f77676b798e0856dd169fea70e7db57d57095261a68"
            }
          ]
        }"#;

        let result = validate_manifest_lock_json_string(manifest_lock);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_manifest_lock_invalid_type() {
        // The type field must be one of the following: extension, system,
        // protocol, addon_loader.
        let manifest_lock = r#"{
          "version": 1,
          "packages": [
            {
              "type": "invalid_type",
              "name": "ext_a",
              "version": "0.1.0-rc.1",
              "hash": "e8dc07a47927e9a650d23f77676b798e0856dd169fea70e7db57d57095261a68"
            }
          ]
        }"#;

        let result = validate_manifest_lock_json_string(manifest_lock);
        assert!(result.is_err());

        let err_reason = result.unwrap_err().to_string();
        assert!(err_reason.contains("is not one of"));
    }

    #[test]
    fn test_validate_manifest_lock_addon_loader_type() {
        // The type field must be addon_loader.
        let manifest_lock = r#"{
          "version": 1,
          "packages": [
            {
              "type": "addon_loader",
              "name": "addon_loader_a",
              "version": "0.1.0-rc.1",
              "hash": "e8dc07a47927e9a650d23f77676b798e0856dd169fea70e7db57d57095261a68"
            }
          ]
        }"#;

        let result = validate_manifest_lock_json_string(manifest_lock);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_manifest_lock_empty_dependency() {
        let manifest_lock = r#"{
          "version": 1,
          "packages": [
            {
              "type": "extension",
              "name": "ext_a",
              "version": "1.0.0",
              "hash": "e8dc07a47927e9a650d23f77676b798e0856dd169fea70e7db57d57095261a68",
      "dependencies": []
            }
          ]
        }"#;

        let result = validate_manifest_lock_json_string(manifest_lock);
        assert!(result.is_err());

        let err_reason = result.unwrap_err().to_string();
        assert!(err_reason.contains("has less than 1 item"));
    }

    #[test]
    fn test_validate_manifest_lock_invalid_supports() {
        let manifest_lock = r#"{
          "version": 1,
          "packages": [
            {
              "type": "extension",
              "name": "ext_a",
              "version": "1.0.0",
              "hash": "e8dc07a47927e9a650d23f77676b798e0856dd169fea70e7db57d57095261a68",
              "supports": [
                {
                  "os": "linux",
                  "arch": "x63"
                }
              ]
            }
          ]
        }"#;

        let result = validate_manifest_lock_json_string(manifest_lock);
        assert!(result.is_err());

        let err_reason = result.unwrap_err().to_string();
        assert!(err_reason.contains("is not one of"));
    }

    #[test]
    fn test_validate_manifest_lock_duplicated_dependencies() {
        let manifest_lock = r#"{
          "version": 1,
          "packages": [
            {
              "type": "extension",
              "name": "ext_a",
              "version": "1.0.0",
              "hash": "e8dc07a47927e9a650d23f77676b798e0856dd169fea70e7db57d57095261a68",
      "dependencies": [
                {
                  "type": "extension",
                  "name": "ext_a"
                },
                {
                  "type": "extension",
                  "name": "ext_a"
                }
              ]
            }
          ]
        }"#;

        let result = validate_manifest_lock_json_string(manifest_lock);
        assert!(result.is_err());

        let err_reason = result.unwrap_err().to_string();
        assert!(err_reason.contains("has non-unique elements"));
    }

    #[test]
    fn test_validate_manifest_lock_success() {
        let manifest_lock = r#"{
        "version": 1,
        "packages": [
          {
            "type": "extension",
            "name": "ext_b",
            "version": "1.0.0",
            "hash": "e6d7ff0aefd1a0618c9f7d8c154b1b90c917390bd00d2a107eae616a36a99391",
    "dependencies": [
              {
                "type": "extension",
                "name": "ext_a"
              }
            ]
          },
          {
            "type": "extension",
            "name": "ext_1",
            "version": "2.0.0",
            "hash": "e64b5c3c66a3014f2c58299d663533eb984ac9e71aa50067f6f3c3b9d409ccdf",
    "dependencies": [
              {
                "type": "extension",
                "name": "ext_3"
              }
            ]
          },
          {
            "type": "extension",
            "name": "ext_3",
            "version": "1.0.0",
            "hash": "2c61c0fc8b3cd2da7457a779b2db423ae93c3b6cae7347cb979453f1ac17ccc0"
          },
          {
            "type": "extension",
            "name": "ext_a",
            "version": "1.2.2",
            "hash": "6fee978cd201b108211b52323a078c5222fd0bc545468b5989d7e42d0f5b7395"
          }
        ]
      }"#;

        let result = validate_manifest_lock_json_string(manifest_lock);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_exposed_messages_extension_subgraph_mutual_exclusion() {
        // Test that exposed_messages with both extension and subgraph fields
        // fails
        let property_json_with_both_fields = r#"
        {
            "ten": {
                "predefined_graphs": [
                    {
                        "name": "test_graph",
                        "graph": {
                            "exposed_messages": [
                                {
                                    "type": "cmd_in",
                                    "name": "test_cmd",
                                    "extension": "ext_a",
                                    "subgraph": "subgraph_1"
                                }
                            ]
                        }
                    }
                ]
            }
        }
        "#;

        let result = ten_validate_property_json_string(property_json_with_both_fields);
        assert!(result.is_err());
        assert!(result.unwrap_err().to_string().contains("oneOf"));

        // Test that exposed_messages with neither extension nor subgraph fields
        // fails
        let property_json_with_neither_field = r#"{
  "ten": {
    "predefined_graphs": [
      {
        "name": "test_graph",
        "graph": {
          "exposed_messages": [
            {
              "type": "cmd_in",
              "name": "test_cmd"
            }
          ]
        }
      }
    ]
  }
}"#;

        let result = ten_validate_property_json_string(property_json_with_neither_field);
        assert!(result.is_err());
        assert!(result.unwrap_err().to_string().contains("oneOf"));

        // Test that exposed_messages with only extension field succeeds
        let property_json_with_extension = r#"{
  "ten": {
    "predefined_graphs": [
      {
        "name": "test_graph",
        "graph": {
          "exposed_messages": [
            {
              "type": "cmd_in",
              "name": "test_cmd",
              "extension": "ext_a"
            }
          ]
        }
      }
    ]
  }
}"#;

        let result = ten_validate_property_json_string(property_json_with_extension);
        assert!(result.is_ok());

        // Test that exposed_messages with only subgraph field succeeds
        let property_json_with_subgraph = r#"{
  "ten": {
    "predefined_graphs": [
      {
        "name": "test_graph",
        "graph": {
          "exposed_messages": [
            {
              "type": "cmd_in",
              "name": "test_cmd",
              "subgraph": "subgraph_1"
            }
          ]
        }
      }
    ]
  }
}"#;

        let result = ten_validate_property_json_string(property_json_with_subgraph);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_exposed_properties_extension_subgraph_mutual_exclusion() {
        // Test that exposed_properties with both extension and subgraph fields
        // fails
        let property_json_with_both_fields = r#"{
  "ten": {
    "predefined_graphs": [
      {
        "name": "test_graph",
        "graph": {
          "exposed_properties": [
            {
              "name": "test_prop",
              "extension": "ext_a",
              "subgraph": "subgraph_1"
            }
          ]
        }
      }
    ]
  }
}"#;

        let result = ten_validate_property_json_string(property_json_with_both_fields);
        assert!(result.is_err());
        assert!(result.unwrap_err().to_string().contains("oneOf"));

        // Test that exposed_properties with neither extension nor subgraph
        // fields fails
        let property_json_with_neither_field = r#"{
  "ten": {
    "predefined_graphs": [
      {
        "name": "test_graph",
        "graph": {
          "exposed_properties": [
            {
              "name": "test_prop"
            }
          ]
        }
      }
    ]
  }
}"#;

        let result = ten_validate_property_json_string(property_json_with_neither_field);
        assert!(result.is_err());
        assert!(result.unwrap_err().to_string().contains("oneOf"));

        // Test that exposed_properties with only extension field succeeds
        let property_json_with_extension = r#"{
  "ten": {
    "predefined_graphs": [
      {
        "name": "test_graph",
        "graph": {
          "exposed_properties": [
            {
              "name": "test_prop",
              "extension": "ext_a"
            }
          ]
        }
      }
    ]
  }
}"#;

        let result = ten_validate_property_json_string(property_json_with_extension);
        assert!(result.is_ok());

        // Test that exposed_properties with only subgraph field succeeds
        let property_json_with_subgraph = r#"{
  "ten": {
    "predefined_graphs": [
      {
        "name": "test_graph",
        "graph": {
          "exposed_properties": [
            {
              "name": "test_prop",
              "subgraph": "subgraph_1"
            }
          ]
        }
      }
    ]
  }
}"#;

        let result = ten_validate_property_json_string(property_json_with_subgraph);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_import_uri_mutual_exclusion_with_nodes() {
        // Test that import_uri and nodes are mutually exclusive
        let property = r#"{
  "ten": {
    "predefined_graphs": [
      {
        "name": "default",
        "graph": {
          "import_uri": "test_graph.json",
          "nodes": [
            {
              "type": "extension",
              "name": "test_ext",
              "addon": "test_addon",
              "extension_group": "test_group"
            }
          ]
        }
      }
    ]
  }
}"#;

        let result = ten_validate_property_json_string(property);
        assert!(result.is_err());
        let msg = result.unwrap_err().to_string();
        assert!(msg.contains("oneOf"));
    }

    #[test]
    fn test_validate_import_uri_mutual_exclusion_with_connections() {
        // Test that import_uri and connections are mutually exclusive
        let property = r#"{
  "ten": {
    "predefined_graphs": [
      {
        "name": "default",
        "graph": {
          "import_uri": "test_graph.json",
          "connections": [
            {
              "extension": "test_ext",
              "cmd": [
                {
                  "name": "test_cmd",
                  "dest": [
                    {
                      "extension": "test_ext"
                    }
                  ]
                }
              ]
            }
          ]
        }
      }
    ]
  }
}"#;

        let result = ten_validate_property_json_string(property);
        assert!(result.is_err());
        let msg = result.unwrap_err().to_string();
        assert!(msg.contains("oneOf"));
    }

    #[test]
    fn test_validate_import_uri_mutual_exclusion_with_exposed_messages() {
        // Test that import_uri and exposed_messages are mutually exclusive
        let property = r#"{
  "ten": {
    "predefined_graphs": [
      {
        "name": "default",
        "graph": {
          "import_uri": "test_graph.json",
          "exposed_messages": [
            {
              "type": "cmd_in",
              "name": "test_msg",
              "extension": "test_ext"
            }
          ]
        }
      }
    ]
  }
}"#;

        let result = ten_validate_property_json_string(property);
        assert!(result.is_err());
        let msg = result.unwrap_err().to_string();
        assert!(msg.contains("oneOf"));
    }

    #[test]
    fn test_validate_import_uri_mutual_exclusion_with_exposed_properties() {
        // Test that import_uri and exposed_properties are mutually exclusive
        let property = r#"{
  "ten": {
    "predefined_graphs": [
      {
        "name": "default",
        "graph": {
          "import_uri": "test_graph.json",
          "exposed_properties": [
            {
              "name": "test_prop",
              "extension": "test_ext"
            }
          ]
        }
      }
    ]
  }
}"#;

        let result = ten_validate_property_json_string(property);
        assert!(result.is_err());
        let msg = result.unwrap_err().to_string();
        assert!(msg.contains("oneOf"));
    }

    #[test]
    fn test_validate_import_uri_without_conflicting_fields_succeeds() {
        // Test that import_uri alone is valid
        let property = r#"{
  "ten": {
    "predefined_graphs": [
      {
        "name": "default",
        "graph": {
          "import_uri": "test_graph.json"
        }
      }
    ]
  }
}"#;

        let result = ten_validate_property_json_string(property);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_graph_without_import_uri_succeeds() {
        // Test that a graph without import_uri but with other fields is valid
        let property = r#"{
  "ten": {
    "predefined_graphs": [
      {
        "name": "default",
        "graph": {
          "nodes": [
            {
              "type": "extension",
              "name": "test_ext",
              "addon": "test_addon",
              "extension_group": "test_group"
            }
          ],
          "connections": [
            {
              "extension": "test_ext",
              "cmd": [
                {
                  "name": "test_cmd",
                  "dest": [
                    {
                      "extension": "test_ext"
                    }
                  ]
                }
              ]
            }
          ],
          "exposed_messages": [
            {
              "type": "cmd_in",
              "name": "test_msg",
              "extension": "test_ext"
            }
          ],
          "exposed_properties": [
            {
              "name": "test_prop",
              "extension": "test_ext"
            }
          ]
        }
      }
    ]
  }
}"#;

        let result = ten_validate_property_json_string(property);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_graph_with_subgraph_specified_addon() {
        let property = r#"{
  "ten": {
    "predefined_graphs": [
      {
        "name": "default",
        "graph": {
          "nodes": [
            {
              "type": "subgraph",
              "name": "subgraph_1",
              "addon": "subgraph_1",
              "graph": {
                "import_uri": "graphs/test_graph.json"
              }
            }
          ]
        }
      }
    ]
  }
}"#;

        let result = ten_validate_property_json_string(property);
        // The subgraph with specified addon is invalid.
        assert!(result.is_err());
    }

    #[test]
    fn test_validate_graph_with_extension_node_without_addon() {
        let property = r#"
        {
          "ten": {
            "predefined_graphs": [
              {
                "name": "default",
                "nodes": [
                  {
                    "type": "extension",
                    "name": "ext_a",
                  }
                ]
              }
            ]
          }
        }
        "#;

        let result = ten_validate_property_json_string(property);
        // The extension node without addon is invalid.
        assert!(result.is_err());
    }

    #[test]
    fn test_validate_source_extension_subgraph_mutual_exclusion() {
        // Test that source with both extension and subgraph fields fails
        let property = r#"{
  "ten": {
    "predefined_graphs": [
      {
        "name": "default",
        "graph": {
          "nodes": [
            {
              "type": "extension",
              "name": "test_ext",
              "addon": "test_addon",
              "extension_group": "test_group"
            }
          ],
          "connections": [
            {
              "extension": "test_ext",
              "cmd": [
                {
                  "name": "test_cmd",
                  "source": [
                    {
                      "extension": "test_ext",
                      "subgraph": "test_subgraph"
                    }
                  ]
                }
              ]
            }
          ]
        }
      }
    ]
  }
}"#;

        let result = ten_validate_property_json_string(property);
        assert!(result.is_err());
        let msg = result.unwrap_err().to_string();
        assert!(msg.contains("oneOf"));
    }

    #[test]
    fn test_validate_source_extension_only() {
        // Test that source with only extension field succeeds
        let property = r#"{
  "ten": {
    "predefined_graphs": [
      {
        "name": "default",
        "graph": {
          "nodes": [
            {
              "type": "extension",
              "name": "test_ext",
              "addon": "test_addon",
              "extension_group": "test_group"
            }
          ],
          "connections": [
            {
              "extension": "test_ext",
              "cmd": [
                {
                  "name": "test_cmd",
                  "source": [
                    {
                      "extension": "test_ext"
                    }
                  ]
                }
              ]
            }
          ]
        }
      }
    ]
  }
}"#;

        let result = ten_validate_property_json_string(property);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_source_subgraph_only() {
        // Test that source with only subgraph field succeeds
        let property = r#"{
  "ten": {
    "predefined_graphs": [
      {
        "name": "default",
        "graph": {
          "nodes": [
            {
              "type": "extension",
              "name": "test_ext",
              "addon": "test_addon",
              "extension_group": "test_group"
            }
          ],
          "connections": [
            {
              "extension": "test_ext",
              "cmd": [
                {
                  "name": "test_cmd",
                  "source": [
                    {
                      "subgraph": "test_subgraph"
                    }
                  ]
                }
              ]
            }
          ]
        }
      }
    ]
  }
}"#;

        let result = ten_validate_property_json_string(property);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_source_with_app() {
        // Test that source with app field succeeds
        let property = r#"{
  "ten": {
    "predefined_graphs": [
      {
        "name": "default",
        "graph": {
          "nodes": [
            {
              "type": "extension",
              "name": "test_ext",
              "addon": "test_addon",
              "extension_group": "test_group"
            }
          ],
          "connections": [
            {
              "extension": "test_ext",
              "cmd": [
                {
                  "name": "test_cmd",
                  "source": [
                    {
                      "app": "msgpack://127.0.0.1:8001/",
                      "extension": "test_ext"
                    }
                  ]
                }
              ]
            }
          ]
        }
      }
    ]
  }
}"#;

        let result = ten_validate_property_json_string(property);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_source_with_invalid_field() {
        // Test that source with invalid additional field fails
        let property = r#"{
  "ten": {
    "predefined_graphs": [
      {
        "name": "default",
        "graph": {
          "nodes": [
            {
              "type": "extension",
              "name": "test_ext",
              "addon": "test_addon",
              "extension_group": "test_group"
            }
          ],
          "connections": [
            {
              "extension": "test_ext",
              "cmd": [
                {
                  "name": "test_cmd",
                  "source": [
                    {
                      "extension": "test_ext",
                      "invalid_field": "value"
                    }
                  ]
                }
              ]
            }
          ]
        }
      }
    ]
  }
}"#;

        let result = ten_validate_property_json_string(property);
        assert!(result.is_err());
        let msg = result.unwrap_err().to_string();
        assert!(msg.contains("Additional properties are not allowed"));
    }

    #[test]
    fn test_validate_msg_dest_extension_with_app() {
        let property = r#"{
  "ten": {
    "predefined_graphs": [
      {
        "name": "test",
        "graph": {
          "connections": [
            {
              "extension": "test_ext",
              "cmd": [
                {
                  "name": "test_cmd",
                  "dest": [
                    {
                      "extension": "target_ext",
                      "app": "test_app"
                    }
                  ]
                }
              ]
            }
          ]
        }
      }
    ]
  }
}"#;
        let result = ten_validate_property_json_string(property);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_msg_dest_extension_with_msg_conversion() {
        let property = r#"{
  "ten": {
    "predefined_graphs": [
      {
        "name": "test",
        "graph": {
          "connections": [
            {
              "extension": "test_ext",
              "cmd": [
                {
                  "name": "test_cmd",
                  "dest": [
                    {
                      "extension": "target_ext",
                      "msg_conversion": {
                        "type": "per_property",
                        "rules": [
                          {
                            "path": "data",
                            "conversion_mode": "fixed_value",
                            "value": "test"
                          }
                        ]
                      }
                    }
                  ]
                }
              ]
            }
          ]
        }
      }
    ]
  }
}"#;
        let result = ten_validate_property_json_string(property);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_msg_dest_subgraph() {
        let property = r#"{
  "ten": {
    "predefined_graphs": [
      {
        "name": "test",
        "graph": {
          "connections": [
            {
              "extension": "test_ext",
              "cmd": [
                {
                  "name": "test_cmd",
                  "dest": [
                    {
                      "subgraph": "target_subgraph"
                    }
                  ]
                }
              ]
            }
          ]
        }
      }
    ]
  }
}"#;
        let result = ten_validate_property_json_string(property);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_msg_dest_selector() {
        let property = r#"{
  "ten": {
    "predefined_graphs": [
      {
        "name": "test",
        "graph": {
          "connections": [
            {
              "extension": "test_ext",
              "cmd": [
                {
                  "name": "test_cmd",
                  "dest": [
                    {
                      "selector": "target_selector"
                    }
                  ]
                }
              ]
            }
          ]
        }
      }
    ]
  }
}"#;
        let result = ten_validate_property_json_string(property);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_msg_dest_invalid_combination() {
        let property = r#"{
  "ten": {
    "predefined_graphs": [
      {
        "name": "test",
        "graph": {
          "connections": [
            {
              "extension": "test_ext",
              "cmd": [
                {
                  "name": "test_cmd",
                  "dest": [
                    {
                      "extension": "target_ext",
                      "subgraph": "target_subgraph"
                    }
                  ]
                }
              ]
            }
          ]
        }
      }
    ]
  }
}"#;
        let result = ten_validate_property_json_string(property);
        assert!(result.is_err());
    }

    #[test]
    fn test_validate_msg_source_extension_with_app() {
        let property = r#"{
  "ten": {
    "predefined_graphs": [
      {
        "name": "test",
        "graph": {
          "connections": [
            {
              "extension": "test_ext",
              "cmd": [
                {
                  "name": "test_cmd",
                  "source": [
                    {
                      "extension": "source_ext",
                      "app": "test_app"
                    }
                  ]
                }
              ]
            }
          ]
        }
      }
    ]
  }
}"#;
        let result = ten_validate_property_json_string(property);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_msg_source_subgraph() {
        let property = r#"{
  "ten": {
    "predefined_graphs": [
      {
        "name": "test",
        "graph": {
          "connections": [
            {
              "extension": "test_ext",
              "cmd": [
                {
                  "name": "test_cmd",
                  "source": [
                    {
                      "subgraph": "source_subgraph"
                    }
                  ]
                }
              ]
            }
          ]
        }
      }
    ]
  }
}"#;
        let result = ten_validate_property_json_string(property);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_msg_source_selector() {
        let property = r#"{
  "ten": {
    "predefined_graphs": [
      {
        "name": "test",
        "graph": {
          "connections": [
            {
              "extension": "test_ext",
              "cmd": [
                {
                  "name": "test_cmd",
                  "source": [
                    {
                      "selector": "source_selector"
                    }
                  ]
                }
              ]
            }
          ]
        }
      }
    ]
  }
}"#;
        let result = ten_validate_property_json_string(property);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_graph_node_extension() {
        let property = r#"{
  "ten": {
    "predefined_graphs": [
      {
        "name": "test",
        "graph": {
          "nodes": [
            {
              "type": "extension",
              "name": "test_ext",
              "addon": "test_addon",
              "extension_group": "test_group",
              "app": "test_app",
              "property": {
                "key1": "value1"
              }
            }
          ]
        }
      }
    ]
  }
}"#;
        let result = ten_validate_property_json_string(property);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_graph_node_subgraph() {
        let property = r#"{
  "ten": {
    "predefined_graphs": [
      {
        "name": "test",
        "graph": {
          "nodes": [
            {
              "type": "subgraph",
              "name": "test_subgraph",
              "graph": {
                "import_uri": "test.json"
              },
              "property": {
                "key1": "value1"
              }
            }
          ]
        }
      }
    ]
  }
}"#;
        let result = ten_validate_property_json_string(property);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_graph_node_extension_missing_required() {
        // missing addon
        let property = r#"
        {
          "ten": {
            "predefined_graphs": [{
              "name": "test",
              "nodes": [{
                "type": "extension",
                "name": "test_ext",
              }]
            }]
          }
        }
        "#;
        let result = ten_validate_property_json_string(property);
        assert!(result.is_err());
    }

    #[test]
    fn test_validate_graph_node_subgraph_missing_required() {
        // missing graph
        let property = r#"{
                            "ten": {
                              "predefined_graphs": [
                                {
                                  "name": "test",
                                  "graph": {
                                    "nodes": [
                                      {
                                        "type": "subgraph",
                                        "name": "test_subgraph"
                                      }
                                    ]
                                  }
                                }
                              ]
                            }
                          }"#;
        let result = ten_validate_property_json_string(property);
        assert!(result.is_err());
    }

    #[test]
    fn test_validate_graph_node_selector_missing_required() {
        // missing filter
        let property = r#"{
                            "ten": {
                              "predefined_graphs": [
                                {
                                  "name": "test",
                                  "graph": {
                                    "nodes": [
                                      {
                                        "type": "selector",
                                        "name": "test_selector"
                                      }
                                    ]
                                  }
                                }
                              ]
                            }
                          }"#;
        let result = ten_validate_property_json_string(property);
        assert!(result.is_err());
    }

    #[test]
    fn test_validate_selector_node_atomic_filter() {
        // Test atomic filter with exact operator
        let property = r#"{
                              "ten": {
                                "predefined_graphs": [
                                  {
                                    "name": "test",
                                    "graph": {
                                      "nodes": [
                                        {
                                          "type": "selector",
                                          "name": "test_selector",
                                          "filter": {
                                            "field": "name",
                                            "operator": "exact",
                                            "value": "test_extension"
                                          }
                                        }
                                      ]
                                    }
                                  }
                                ]
                              }
                            }"#;
        let result = ten_validate_property_json_string(property);
        assert!(result.is_ok());

        // Test atomic filter with regex operator
        let property = r#"{
                            "ten": {
                              "predefined_graphs": [
                                {
                                  "name": "test",
                                  "graph": {
                                    "nodes": [
                                      {
                                        "type": "selector",
                                        "name": "test_selector",
                                        "filter": {
                                          "field": "name",
                                          "operator": "regex",
                                          "value": "test.*"
                                        }
                                      }
                                    ]
                                  }
                                }
                              ]
                            }
                          }"#;
        let result = ten_validate_property_json_string(property);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_selector_node_compound_filter() {
        // Test AND filter
        let property = r#"{
                            "ten": {
                              "predefined_graphs": [
                                {
                                  "name": "test",
                                  "graph": {
                                    "nodes": [
                                      {
                                        "type": "selector",
                                        "name": "test_selector",
                                        "filter": {
                                          "and": [
                                            {
                                              "field": "name",
                                              "operator": "regex",
                                              "value": "test.*"
                                            },
                                            {
                                              "field": "type",
                                              "operator": "exact",
                                              "value": "extension"
                                            }
                                          ]
                                        }
                                      }
                                    ]
                                  }
                                }
                              ]
                            }
                          }"#;
        let result = ten_validate_property_json_string(property);
        assert!(result.is_ok());

        // Test OR filter
        let property = r#"{
                            "ten": {
                              "predefined_graphs": [
                                {
                                  "name": "test",
                                  "graph": {
                                    "nodes": [
                                      {
                                        "type": "selector",
                                        "name": "test_selector",
                                        "filter": {
                                          "or": [
                                            {
                                              "field": "name",
                                              "operator": "exact",
                                              "value": "test1"
                                            },
                                            {
                                              "field": "name",
                                              "operator": "exact",
                                              "value": "test2"
                                            }
                                          ]
                                        }
                                      }
                                    ]
                                  }
                                }
                              ]
                            }
                          }"#;
        let result = ten_validate_property_json_string(property);
        assert!(result.is_ok());

        // Test nested AND/OR filter
        let property = r#"{
                            "ten": {
                              "predefined_graphs": [
                                {
                                  "name": "test",
                                  "graph": {
                                    "nodes": [
                                      {
                                        "type": "selector",
                                        "name": "test_selector",
                                        "filter": {
                                          "and": [
                                            {
                                              "field": "type",
                                              "operator": "exact",
                                              "value": "extension"
                                            },
                                            {
                                              "or": [
                                                {
                                                  "field": "name",
                                                  "operator": "exact",
                                                  "value": "test1"
                                                },
                                                {
                                                  "field": "name",
                                                  "operator": "exact",
                                                  "value": "test2"
                                                }
                                              ]
                                            }
                                          ]
                                        }
                                      }
                                    ]
                                  }
                                }
                              ]
                            }
                          }"#;
        let result = ten_validate_property_json_string(property);
        assert!(result.is_ok());
    }

    #[test]
    fn test_validate_selector_node_invalid_filter() {
        // Test missing required field in atomic filter
        let property = r#"{
                            "ten": {
                              "predefined_graphs": [
                                {
                                  "name": "test",
                                  "graph": {
                                    "nodes": [
                                      {
                                        "type": "selector",
                                        "name": "test_selector",
                                        "filter": {
                                          "field": "name",
                                          "operator": "exact"
                                        }
                                      }
                                    ]
                                  }
                                }
                              ]
                            }
                          }"#;
        let result = ten_validate_property_json_string(property);
        assert!(result.is_err());

        // Test empty AND array
        let property = r#"{
                            "ten": {
                              "predefined_graphs": [
                                {
                                  "name": "test",
                                  "graph": {
                                    "nodes": [
                                      {
                                        "type": "selector",
                                        "name": "test_selector",
                                        "filter": {
                                          "and": []
                                        }
                                      }
                                    ]
                                  }
                                }
                              ]
                            }
                          }"#;
        let result = ten_validate_property_json_string(property);
        assert!(result.is_err());

        // Test invalid operator
        let property = r#"{
                            "ten": {
                              "predefined_graphs": [
                                {
                                  "name": "test",
                                  "graph": {
                                    "nodes": [
                                      {
                                        "type": "selector",
                                        "name": "test_selector",
                                        "filter": {
                                          "field": "name",
                                          "operator": "contains",
                                          "value": "test"
                                        }
                                      }
                                    ]
                                  }
                                }
                              ]
                            }
                          }"#;
        let result = ten_validate_property_json_string(property);
        assert!(result.is_err());

        // Test empty value string
        let property = r#"{
                            "ten": {
                              "predefined_graphs": [
                                {
                                  "name": "test",
                                  "graph": {
                                    "nodes": [
                                      {
                                        "type": "selector",
                                        "name": "test_selector",
                                        "filter": {
                                          "field": "name",
                                          "operator": "exact",
                                          "value": ""
                                        }
                                      }
                                    ]
                                  }
                                }
                              ]
                            }
                          }"#;
        let result = ten_validate_property_json_string(property);
        assert!(result.is_err());
    }

    #[test]
    fn test_validate_selector_node_additional_properties() {
        // Test additional properties in selector
        let property = r#"{
                            "ten": {
                              "predefined_graphs": [
                                {
                                  "name": "test",
                                  "graph": {
                                    "nodes": [
                                      {
                                        "type": "selector",
                                        "name": "test_selector",
                                        "filter": {
                                          "field": "name",
                                          "operator": "exact",
                                          "value": "test",
                                          "extra": "invalid"
                                        }
                                      }
                                    ]
                                  }
                                }
                              ]
                            }
                          }"#;
        let result = ten_validate_property_json_string(property);
        assert!(result.is_err());

        // Test additional properties in selector node
        let property = r#"{
                            "ten": {
                              "predefined_graphs": [
                                {
                                  "name": "test",
                                  "graph": {
                                    "nodes": [
                                      {
                                        "type": "selector",
                                        "name": "test_selector",
                                        "filter": {
                                          "field": "name",
                                          "operator": "exact",
                                          "value": "test"
                                        },
                                        "extra": "invalid"
                                      }
                                    ]
                                  }
                                }
                              ]
                            }
                          }"#;
        let result = ten_validate_property_json_string(property);
        assert!(result.is_err());
        assert!(result.unwrap_err().to_string().contains("Additional properties are not allowed"));
    }
}

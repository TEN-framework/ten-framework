//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::collections::{HashMap, HashSet};

use anyhow::{anyhow, Context, Result};

use crate::{
    json_schema::ten_validate_interface_json_string,
    pkg_info::manifest::api::{
        ManifestApi, ManifestApiInterface, ManifestApiMsg, ManifestApiProperty,
        ManifestApiPropertyAttributes,
    },
    utils::{
        path::{get_base_dir_of_uri, get_real_path_from_import_uri},
        uri::load_content_from_uri,
    },
};

/// Loads interface from the specified URI with an optional base directory.
///
/// The URI can be:
/// - A relative path (relative to the base_dir if provided)
/// - A URI (http:// or https:// or file://)
///
/// If the interface is already loaded or cannot be loaded, return an error.
async fn load_interface(
    interface: &ManifestApiInterface,
    interface_set: &mut HashSet<String>,
    app_base_dir: Option<&str>,
) -> Result<ManifestApi> {
    let import_uri = &interface.import_uri;
    let base_dir = interface.base_dir.as_deref();

    // Get the real path according to the import_uri and base_dir.
    let real_path = get_real_path_from_import_uri(import_uri, base_dir, app_base_dir)?;

    // Check if the interface is in the interface_set.
    if interface_set.contains(&real_path) {
        return Err(anyhow::anyhow!("Circular reference detected: {}", real_path));
    }

    // Add the interface to the interface_set.
    interface_set.insert(real_path.clone());

    // Load the content from the uri.
    let interface_content = load_content_from_uri(&real_path).await?;

    // Schema check for interface.
    ten_validate_interface_json_string(&interface_content)
        .with_context(|| format!("Invalid interface file: {real_path}"))?;

    // Parse the interface file into a ManifestApi structure.
    let mut interface_api: ManifestApi = serde_json::from_str(&interface_content)
        .with_context(|| format!("Failed to parse interface file from {real_path}"))?;

    let base_dir = get_base_dir_of_uri(&real_path)?;

    // Set the base_dir of the interface.
    if let Some(interface) = &mut interface_api.interface.as_mut() {
        for interface in interface.iter_mut() {
            interface.base_dir = Some(base_dir.clone());
        }
    }

    Ok(interface_api)
}

fn merge_manifest_api(apis: Vec<ManifestApi>) -> Result<ManifestApi> {
    if apis.len() < 2 {
        return Err(anyhow::anyhow!("At least 2 ManifestApi instances are required to merge"));
    }

    let mut merged_property: HashMap<String, ManifestApiPropertyAttributes> = HashMap::new();
    let mut merged_cmd_in: HashMap<String, ManifestApiMsg> = HashMap::new();
    let mut merged_cmd_out: HashMap<String, ManifestApiMsg> = HashMap::new();
    let mut merged_data_in: HashMap<String, ManifestApiMsg> = HashMap::new();
    let mut merged_data_out: HashMap<String, ManifestApiMsg> = HashMap::new();
    let mut merged_audio_frame_in: HashMap<String, ManifestApiMsg> = HashMap::new();
    let mut merged_audio_frame_out: HashMap<String, ManifestApiMsg> = HashMap::new();
    let mut merged_video_frame_in: HashMap<String, ManifestApiMsg> = HashMap::new();
    let mut merged_video_frame_out: HashMap<String, ManifestApiMsg> = HashMap::new();

    // Helper function to merge message maps
    fn merge_msg_map(
        target: &mut HashMap<String, ManifestApiMsg>,
        source: Option<Vec<ManifestApiMsg>>,
        msg_type: &str,
    ) -> Result<()> {
        if let Some(msgs) = source {
            for msg in msgs {
                if let Some(existing_msg) = target.get(&msg.name) {
                    if existing_msg != &msg {
                        return Err(anyhow!(
                            "Conflicting {} message '{}': properties, required fields, or result \
                             do not match",
                            msg_type,
                            msg.name
                        ));
                    }
                } else {
                    target.insert(msg.name.clone(), msg);
                }
            }
        }
        Ok(())
    }

    // Process each API
    for api in apis {
        // Merge property
        if let Some(api_property) = &api.property {
            if let Some(properties) = &api_property.properties {
                for (key, value) in properties {
                    if let Some(existing_value) = merged_property.get(key.as_str()) {
                        if existing_value != value {
                            return Err(anyhow!(
                                "Conflicting property '{}': values do not match",
                                key
                            ));
                        }
                    } else {
                        merged_property.insert(key.clone(), value.clone());
                    }
                }
            }
        }

        // Merge various message types
        merge_msg_map(&mut merged_cmd_in, api.cmd_in, "cmd_in")?;
        merge_msg_map(&mut merged_cmd_out, api.cmd_out, "cmd_out")?;
        merge_msg_map(&mut merged_data_in, api.data_in, "data_in")?;
        merge_msg_map(&mut merged_data_out, api.data_out, "data_out")?;
        merge_msg_map(&mut merged_audio_frame_in, api.audio_frame_in, "audio_frame_in")?;
        merge_msg_map(&mut merged_audio_frame_out, api.audio_frame_out, "audio_frame_out")?;
        merge_msg_map(&mut merged_video_frame_in, api.video_frame_in, "video_frame_in")?;
        merge_msg_map(&mut merged_video_frame_out, api.video_frame_out, "video_frame_out")?;
    }

    // Build the merged result
    Ok(ManifestApi {
        property: if merged_property.is_empty() {
            None
        } else {
            Some(ManifestApiProperty {
                properties: Some(merged_property),
                required: None, // TODO: Handle merging of required fields
            })
        },
        interface: None, // 3. No need to merge interface
        cmd_in: if merged_cmd_in.is_empty() {
            None
        } else {
            Some(merged_cmd_in.into_values().collect())
        },
        cmd_out: if merged_cmd_out.is_empty() {
            None
        } else {
            Some(merged_cmd_out.into_values().collect())
        },
        data_in: if merged_data_in.is_empty() {
            None
        } else {
            Some(merged_data_in.into_values().collect())
        },
        data_out: if merged_data_out.is_empty() {
            None
        } else {
            Some(merged_data_out.into_values().collect())
        },
        audio_frame_in: if merged_audio_frame_in.is_empty() {
            None
        } else {
            Some(merged_audio_frame_in.into_values().collect())
        },
        audio_frame_out: if merged_audio_frame_out.is_empty() {
            None
        } else {
            Some(merged_audio_frame_out.into_values().collect())
        },
        video_frame_in: if merged_video_frame_in.is_empty() {
            None
        } else {
            Some(merged_video_frame_in.into_values().collect())
        },
        video_frame_out: if merged_video_frame_out.is_empty() {
            None
        } else {
            Some(merged_video_frame_out.into_values().collect())
        },
    })
}

/// Flatten a ManifestApi instance.
/// If the ManifestApi contains any interface, it will be flattened. If some
/// error occurs during flattening, the original ManifestApi will be returned.
pub async fn flatten_manifest_api(
    manifest_api: &Option<ManifestApi>,
    flattened_api: &mut Option<ManifestApi>,
    app_base_dir: Option<&str>,
) -> Result<()> {
    // Try to flatten the manifest api if it contains any interface references.
    let maybe_flattened_api =
        if let Some(api) = manifest_api { api.flatten(app_base_dir).await? } else { None };

    if let Some(api) = maybe_flattened_api {
        *flattened_api = Some(api);
    }

    Ok(())
}

impl ManifestApi {
    /// Helper function that contains the common logic for flattening a
    /// ManifestApi instance.
    async fn flatten_internal(
        &self,
        flattened_apis: &mut Vec<ManifestApi>,
        interface_set: &mut HashSet<String>,
        app_base_dir: Option<&str>,
    ) -> Result<()> {
        // Push the current ManifestApi to the flattened_apis.
        flattened_apis.push(self.clone());

        // Check if the ManifestApi contains any interface.
        let has_interfaces =
            self.interface.is_some() && !self.interface.as_ref().unwrap().is_empty();

        if !has_interfaces {
            // No interfaces, return immediately.
            return Ok(());
        }

        // This ManifestApi has interfaces, so we need to flatten them.
        for interface in self.interface.as_ref().unwrap() {
            // Load the interface.
            // If the interface is already loaded or cannot be loaded,
            // return an error.
            let loaded_interface = load_interface(interface, interface_set, app_base_dir).await?;

            // Flatten the loaded interface using Box::pin to handle recursion
            Box::pin(loaded_interface.flatten_internal(
                flattened_apis,
                interface_set,
                app_base_dir,
            ))
            .await?;
        }

        Ok(())
    }

    /// Convenience method for flattening a ManifestApi instance.
    ///
    /// Returns `Ok(None)` if the ManifestApi contains no interface and doesn't
    /// need flattening. Returns `Ok(Some(flattened_manifest_api))` if the
    /// ManifestApi was successfully flattened.
    async fn flatten(&self, app_base_dir: Option<&str>) -> Result<Option<ManifestApi>> {
        // Check if the ManifestApi contains any interface.
        if self.interface.is_none() || self.interface.as_ref().unwrap().is_empty() {
            return Ok(None);
        }

        // This ManifestApi has interfaces, so we need to flatten them.
        let mut flattened_apis = Vec::new();
        let mut interface_set = HashSet::new();

        self.flatten_internal(&mut flattened_apis, &mut interface_set, app_base_dir).await?;

        // Merge the flattened apis into a single ManifestApi.
        let merged_api = merge_manifest_api(flattened_apis)?;

        Ok(Some(merged_api))
    }
}

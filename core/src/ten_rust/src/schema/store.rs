//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::collections::HashMap;

use anyhow::{Context, Ok, Result};

use super::runtime_interface::{create_schema_from_json, TenSchema};
use crate::pkg_info::{
    manifest::{
        api::{ManifestApi, ManifestApiMsg, ManifestApiProperty},
        Manifest,
    },
    message::{MsgDirection, MsgType},
    PkgInfo,
};

/// Represents the schema for a command and its result.
///
/// This structure holds two optional schemas:
/// - `cmd`: The schema that defines the structure of the command.
/// - `result`: The schema that defines the structure of the command's result.
#[derive(Debug, Clone, Default)]
pub struct TenMsgSchema {
    /// Schema for the message structure.
    pub msg: Option<TenSchema>,

    /// Schema for the command's result structure.
    pub result: Option<TenSchema>,
}

#[derive(Debug, Clone, Default)]
pub struct SchemaStore {
    /// Schema for property definitions.
    pub property: Option<TenSchema>,

    /// Command schemas for incoming commands.
    pub cmd_in: HashMap<String, TenMsgSchema>,
    /// Command schemas for outgoing commands.
    pub cmd_out: HashMap<String, TenMsgSchema>,

    /// Data schemas for incoming data.
    pub data_in: HashMap<String, TenMsgSchema>,
    /// Data schemas for outgoing data.
    pub data_out: HashMap<String, TenMsgSchema>,

    /// Schema for incoming video frames.
    pub video_frame_in: HashMap<String, TenMsgSchema>,
    /// Schema for outgoing video frames.
    pub video_frame_out: HashMap<String, TenMsgSchema>,

    /// Schema for incoming audio frames.
    pub audio_frame_in: HashMap<String, TenMsgSchema>,
    /// Schema for outgoing audio frames.
    pub audio_frame_out: HashMap<String, TenMsgSchema>,
}

impl SchemaStore {
    /// Creates a SchemaStore from a Manifest.
    ///
    /// This function extracts API schemas from the manifest and constructs a
    /// SchemaStore containing all the command, data, and frame schemas
    /// defined in the manifest.
    pub async fn from_manifest(
        manifest: &Manifest,
        app_base_dir: Option<&str>,
    ) -> Result<Option<Self>> {
        if manifest.api.is_none() {
            return Ok(None);
        }

        let mut schema_store = SchemaStore::default();

        if let Some(api) = manifest.get_flattened_api(app_base_dir).await.unwrap_or(None) {
            schema_store.parse_schemas_from_manifest(&api)?;
            Ok(Some(schema_store))
        } else {
            schema_store.parse_schemas_from_manifest(manifest.api.as_ref().unwrap())?;

            Ok(Some(schema_store))
        }
    }

    /// Parses schemas from a ManifestApi and populates the SchemaStore.
    ///
    /// This method processes all schema types defined in the manifest API:
    /// - Property schemas
    /// - Command schemas (both incoming and outgoing)
    /// - Data schemas (both incoming and outgoing)
    /// - Video frame schemas (both incoming and outgoing)
    /// - Audio frame schemas (both incoming and outgoing)
    ///
    /// Each schema type is parsed and stored in the appropriate collection
    /// within the SchemaStore.
    pub fn parse_schemas_from_manifest(&mut self, manifest_api: &ManifestApi) -> Result<()> {
        // Parse property schema if defined.
        if let Some(property) = &manifest_api.property {
            let mut property_schema_value: serde_json::Value =
                serde_json::json!({"type": "object"});
            let property_schema_object = property_schema_value.as_object_mut().unwrap();

            if let Some(properties) = &property.properties {
                property_schema_object
                    .insert("properties".to_string(), serde_json::to_value(properties)?);
            }

            if let Some(required) = &property.required {
                property_schema_object
                    .insert("required".to_string(), serde_json::to_value(required)?);
            }

            let schema = create_schema_from_json(
                serde_json::to_value(property_schema_object).as_ref().unwrap(),
            )?;
            self.property = Some(schema);
        }

        // Parse incoming command schemas.
        if let Some(cmd_in_schema) = &manifest_api.cmd_in {
            parse_msgs_schema_from_manifest(cmd_in_schema, &mut self.cmd_in)
                .with_context(|| "Failed to parse cmd_in schema")?;
        }

        // Parse outgoing command schemas.
        if let Some(cmd_out_schema) = &manifest_api.cmd_out {
            parse_msgs_schema_from_manifest(cmd_out_schema, &mut self.cmd_out)
                .with_context(|| "Failed to parse cmd_out schema")?;
        }

        // Parse incoming data schemas.
        if let Some(data_in_schema) = &manifest_api.data_in {
            parse_msgs_schema_from_manifest(data_in_schema, &mut self.data_in)
                .with_context(|| "Failed to parse data_in schema")?;
        }

        // Parse outgoing data schemas.
        if let Some(data_out_schema) = &manifest_api.data_out {
            parse_msgs_schema_from_manifest(data_out_schema, &mut self.data_out)
                .with_context(|| "Failed to parse data_out schema")?;
        }

        // Parse incoming video frame schemas.
        if let Some(video_frame_in_schema) = &manifest_api.video_frame_in {
            parse_msgs_schema_from_manifest(video_frame_in_schema, &mut self.video_frame_in)
                .with_context(|| "Failed to parse video_frame_in schema")?;
        }

        // Parse outgoing video frame schemas.
        if let Some(video_frame_out_schema) = &manifest_api.video_frame_out {
            parse_msgs_schema_from_manifest(video_frame_out_schema, &mut self.video_frame_out)
                .with_context(|| "Failed to parse video_frame_out schema")?;
        }

        // Parse incoming audio frame schemas.
        if let Some(audio_frame_in_schema) = &manifest_api.audio_frame_in {
            parse_msgs_schema_from_manifest(audio_frame_in_schema, &mut self.audio_frame_in)
                .with_context(|| "Failed to parse audio_frame_in schema")?;
        }

        // Parse outgoing audio frame schemas.
        if let Some(audio_frame_out_schema) = &manifest_api.audio_frame_out {
            parse_msgs_schema_from_manifest(audio_frame_out_schema, &mut self.audio_frame_out)
                .with_context(|| "Failed to parse audio_frame_out schema")?;
        }

        Ok(())
    }
}

fn parse_msgs_schema_from_manifest(
    manifest_msgs: &Vec<ManifestApiMsg>,
    target_map: &mut HashMap<String, TenMsgSchema>,
) -> Result<()> {
    for manifest_msg in manifest_msgs {
        let msg_name = manifest_msg.name.clone();
        let msg_schema = create_c_schema_from_manifest_api(manifest_msg)?;
        if let Some(schema) = msg_schema {
            let present = target_map.insert(msg_name.clone(), schema);
            if present.is_some() {
                return Err(anyhow::anyhow!("duplicated schema definition for cmd {}.", msg_name));
            }
        }
    }

    Ok(())
}

// The TEN schema system will assign a type to each schema type. For example,
// the type for an object schema is 'object,' and the type for an array schema
// is 'array,' and so on. If the schema is of object type, it also needs to have
// a top-level field called 'properties' to hold the contents of the object
// schema. However, the content of the JSON will be somewhat simplified to avoid
// the need for manually writing redundant information. Therefore, the parsing
// logic will need to dynamically add some appropriate fields on the fly to meet
// the requirements of the TEN schema format.
//
// The 'property' field is of object type, so its schema definition needs to be
// an object with a 'type' field equals to 'object'. The expected schema json of
// 'property' field is:
//
// {
//   "type": "object",
//   "properties": {
//
//   },
//   "required": []
// }
pub fn create_c_schema_from_properties_and_required(
    property: &Option<ManifestApiProperty>,
) -> Result<Option<TenSchema>> {
    let mut property_schema_value: serde_json::Value = serde_json::json!({"type": "object"});
    let property_schema_object = property_schema_value.as_object_mut().unwrap();

    if let Some(property) = property {
        if let Some(properties) = &property.properties {
            let mut property_json_value = serde_json::json!({});
            let property_json_object = property_json_value.as_object_mut().unwrap();

            properties.iter().for_each(|(key, attr)| {
                property_json_object.insert(key.clone(), serde_json::to_value(attr).unwrap());
            });

            property_schema_object
                .insert("properties".to_string(), serde_json::to_value(property_json_object)?);
        }

        if let Some(required) = &property.required {
            property_schema_object.insert("required".to_string(), serde_json::to_value(required)?);
        }
    }

    Ok(Some(create_schema_from_json(
        serde_json::to_value(property_schema_object).as_ref().unwrap(),
    )?))
}

pub fn create_c_schema_from_manifest_api(
    manifest_msg: &ManifestApiMsg,
) -> Result<Option<TenMsgSchema>> {
    let mut schema = TenMsgSchema::default();

    if let Some(manifest_result) = &manifest_msg.result {
        let result_schema =
            create_c_schema_from_properties_and_required(&manifest_result.property)?;
        schema.result = result_schema;
    }

    let property_schema = create_c_schema_from_properties_and_required(&manifest_msg.property)?;
    schema.msg = property_schema;

    if schema.msg.is_none() && schema.result.is_none() {
        Ok(None)
    } else {
        Ok(Some(schema))
    }
}

pub fn are_ten_schemas_compatible(
    source: Option<&TenSchema>,
    target: Option<&TenSchema>,
    none_source_is_not_error: bool,
    none_target_is_not_error: bool,
) -> Result<()> {
    if none_target_is_not_error {
        if target.is_none() {
            return Ok(());
        }
    } else if target.is_none() {
        return Err(anyhow::anyhow!("target schema is undefined."));
    }

    if none_source_is_not_error {
        if source.is_none() {
            return Ok(());
        }
    } else if source.is_none() {
        return Err(anyhow::anyhow!("source schema is undefined."));
    }

    let source = source.unwrap();
    let target = target.unwrap();
    source.is_compatible_with(target)
}

pub fn are_msg_schemas_compatible(
    source: Option<&TenMsgSchema>,
    target: Option<&TenMsgSchema>,
    none_source_is_not_error: bool,
    none_target_is_not_error: bool,
) -> Result<()> {
    if none_target_is_not_error {
        if target.is_none() {
            return Ok(());
        }
    } else if target.is_none() {
        return Err(anyhow::anyhow!("target schema is undefined."));
    }

    if none_source_is_not_error {
        if source.is_none() {
            return Ok(());
        }
    } else if source.is_none() {
        return Err(anyhow::anyhow!("source schema is undefined."));
    }

    let source = source.unwrap();
    let target = target.unwrap();

    are_ten_schemas_compatible(
        source.msg.as_ref(),
        target.msg.as_ref(),
        none_source_is_not_error,
        none_target_is_not_error,
    )?;

    // Note: Here target is the reverse of source, because result is the reverse
    // of source.
    are_ten_schemas_compatible(target.result.as_ref(), source.result.as_ref(), true, true)?;

    Ok(())
}

pub fn find_c_msg_schema_from_pkg_info<'a>(
    extension_pkg_info: &'a PkgInfo,
    msg_type: &MsgType,
    msg_name: &str,
    direction: &MsgDirection,
) -> Option<&'a TenMsgSchema> {
    // Access the schema_store. If it's None, propagate None.
    let schema_store = extension_pkg_info.schema_store.as_ref()?;

    // Retrieve the message schema based on the direction and message type.
    match msg_type {
        MsgType::Cmd => match direction {
            MsgDirection::In => schema_store.cmd_in.get(msg_name),
            MsgDirection::Out => schema_store.cmd_out.get(msg_name),
        },
        MsgType::Data => match direction {
            MsgDirection::In => schema_store.data_in.get(msg_name),
            MsgDirection::Out => schema_store.data_out.get(msg_name),
        },
        MsgType::AudioFrame => match direction {
            MsgDirection::In => schema_store.audio_frame_in.get(msg_name),
            MsgDirection::Out => schema_store.audio_frame_out.get(msg_name),
        },
        MsgType::VideoFrame => match direction {
            MsgDirection::In => schema_store.video_frame_in.get(msg_name),
            MsgDirection::Out => schema_store.video_frame_out.get(msg_name),
        },
    }
}

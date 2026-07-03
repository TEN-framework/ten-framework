//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{
    fmt,
    sync::{Arc, OnceLock},
};

use actix_web::{web, HttpResponse, Responder};
use serde::{Deserialize, Serialize};
use serde_json::{Map, Value};

use super::locale::Locale;
use crate::designer::{
    response::{ApiResponse, ErrorResponse, Status},
    DesignerState,
};

// Supported languages.
const DEFAULT_LOCALE: Locale = Locale::EnUs;
const SUPPORTED_LOCALES: [Locale; 3] = [Locale::EnUs, Locale::ZhCn, Locale::ZhTw];

/// Enum for doc link keys.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Deserialize, Serialize)]
pub enum DocLinkKey {
    #[serde(rename = "graph")]
    Graph,

    #[serde(rename = "app")]
    App,

    #[serde(rename = "extension")]
    Extension,

    #[serde(rename = "unknown")]
    #[serde(other)]
    Unknown,
}

impl fmt::Display for DocLinkKey {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            DocLinkKey::Graph => write!(f, "graph"),
            DocLinkKey::App => write!(f, "app"),
            DocLinkKey::Extension => write!(f, "extension"),
            DocLinkKey::Unknown => write!(f, "unknown"),
        }
    }
}

#[derive(Debug, Deserialize, Serialize)]
pub struct GetDocLinkRequestPayload {
    pub key: DocLinkKey,

    #[serde(default = "default_locale")]
    pub locale: Locale,
}

fn default_locale() -> Locale {
    DEFAULT_LOCALE
}

#[derive(Debug, Deserialize, Serialize)]
pub struct GetDocLinkResponseData {
    pub key: DocLinkKey,
    pub locale: Locale,
    pub text: String,
}

// Include the doc link JSON.
static DOC_LINKS_JSON: &str = include_str!("doc_links.json");

// Use a OnceLock to parse the JSON only once.
static DOC_LINKS: OnceLock<Map<String, Value>> = OnceLock::new();

/// This function handles requests for doc link from the frontend. It accepts a
/// JSON payload with a "key" property and returns the corresponding doc link.
pub async fn get_doc_link_endpoint(
    request_payload: web::Json<GetDocLinkRequestPayload>,
    _state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    let key = &request_payload.key;
    let locale = &request_payload.locale;

    // Get the doc link for the given key and locale.
    let doc_link = get_doc_link_for_key(&key.to_string(), locale);

    if let Some((text, used_locale)) = doc_link {
        let response_data = GetDocLinkResponseData {
            key: *key,
            locale: used_locale,
            text,
        };

        let api_response = ApiResponse {
            status: Status::Ok,
            data: response_data,
            meta: None,
        };

        Ok(HttpResponse::Ok().json(api_response))
    } else {
        let error_response = ErrorResponse {
            status: Status::Fail,
            message: format!("Doc link not found for key {key} and locale {locale}"),
            error: None,
        };

        Ok(HttpResponse::NotFound().json(error_response))
    }
}

/// Retrieves doc link for a given key and locale from the JSON file.
/// Returns a tuple of (text, locale_used) where locale_used is the locale that
/// was actually used (might be different from requested if fallback occurred).
fn get_doc_link_for_key(key: &str, requested_locale: &Locale) -> Option<(String, Locale)> {
    // Parse the JSON only once and store it in the static variable.
    let doc_links = DOC_LINKS.get_or_init(|| {
        match serde_json::from_str::<Map<String, Value>>(DOC_LINKS_JSON) {
            Ok(map) => map,
            Err(_) => Map::new(), // Return empty map in case of parsing error.
        }
    });

    // Look up the key in the JSON data.
    doc_links.get(key).and_then(|value| {
        if let Some(obj) = value.as_object() {
            // Convert Locale to string for JSON lookup.
            let locale_str = requested_locale.to_string();

            // Try to get the requested locale.
            if let Some(text) = obj.get(&locale_str).and_then(|v| v.as_str()) {
                return Some((text.to_string(), *requested_locale));
            }

            // If requested locale not found, try default locale.
            if requested_locale != &DEFAULT_LOCALE {
                let default_locale_str = DEFAULT_LOCALE.to_string();
                if let Some(text) = obj.get(&default_locale_str).and_then(|v| v.as_str()) {
                    return Some((text.to_string(), DEFAULT_LOCALE));
                }
            }

            // If even default locale is not found, try any supported locale.
            for locale in SUPPORTED_LOCALES.iter() {
                let locale_str = locale.to_string();
                if let Some(text) = obj.get(&locale_str).and_then(|v| v.as_str()) {
                    return Some((text.to_string(), *locale));
                }
            }
        }
        None
    })
}

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

/// Enum for help text keys.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Deserialize, Serialize)]
pub enum HelpTextKey {
    #[serde(rename = "ten_agent")]
    TenAgent,

    #[serde(rename = "ten_framework")]
    TenFramework,

    #[serde(rename = "unknown")]
    #[serde(other)]
    Unknown,
}

impl fmt::Display for HelpTextKey {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            HelpTextKey::TenAgent => write!(f, "ten_agent"),
            HelpTextKey::TenFramework => write!(f, "ten_framework"),
            HelpTextKey::Unknown => write!(f, "unknown"),
        }
    }
}

#[derive(Debug, Deserialize, Serialize)]
pub struct GetHelpTextRequestPayload {
    pub key: HelpTextKey,

    #[serde(default = "default_locale")]
    pub locale: Locale,
}

fn default_locale() -> Locale {
    DEFAULT_LOCALE
}

#[derive(Debug, Deserialize, Serialize)]
pub struct GetHelpTextResponseData {
    pub key: HelpTextKey,
    pub locale: Locale,
    pub text: String,
}

// Include the help text JSON.
static HELP_TEXTS_JSON: &str = include_str!("help_texts.json");

// Use a OnceLock to parse the JSON only once.
static HELP_TEXTS: OnceLock<Map<String, Value>> = OnceLock::new();

/// This function handles requests for help text from the frontend. It accepts a
/// JSON payload with a "key" property and returns the corresponding help text.
pub async fn get_help_text_endpoint(
    request_payload: web::Json<GetHelpTextRequestPayload>,
    _state: web::Data<Arc<DesignerState>>,
) -> Result<impl Responder, actix_web::Error> {
    let key = &request_payload.key;
    let locale = &request_payload.locale;

    // Get the help text for the given key and locale.
    let help_text = get_help_text_for_key(&key.to_string(), locale);

    if let Some((text, used_locale)) = help_text {
        let response_data = GetHelpTextResponseData {
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
            message: format!("Help text not found for key {key} and locale {locale}"),
            error: None,
        };

        Ok(HttpResponse::NotFound().json(error_response))
    }
}

/// Retrieves help text for a given key and locale from the JSON file.
/// Returns a tuple of (text, locale_used) where locale_used is the locale that
/// was actually used (might be different from requested if fallback occurred).
fn get_help_text_for_key(key: &str, requested_locale: &Locale) -> Option<(String, Locale)> {
    // Parse the JSON only once and store it in the static variable.
    let help_texts = HELP_TEXTS.get_or_init(|| {
        match serde_json::from_str::<Map<String, Value>>(HELP_TEXTS_JSON) {
            Ok(map) => map,
            Err(_) => Map::new(), // Return empty map in case of parsing error.
        }
    });

    // Look up the key in the JSON data.
    help_texts.get(key).and_then(|value| {
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

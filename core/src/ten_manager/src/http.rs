//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use anyhow::{anyhow, Result};

pub fn create_http_client_with_proxies() -> Result<reqwest::Client> {
    let mut client_builder = reqwest::Client::builder();

    // Handle HTTP_PROXY and http_proxy.
    if let Ok(http_proxy) = std::env::var("HTTP_PROXY") {
        let proxy = reqwest::Proxy::http(&http_proxy)
            .map_err(|e| anyhow!("Invalid HTTP_PROXY URL: {}", e))?;
        client_builder = client_builder.proxy(proxy);
    } else if let Ok(http_proxy_lower) = std::env::var("http_proxy") {
        let proxy = reqwest::Proxy::http(&http_proxy_lower)
            .map_err(|e| anyhow!("Invalid http_proxy URL: {}", e))?;
        client_builder = client_builder.proxy(proxy);
    }

    // Handle HTTPS_PROXY and https_proxy.
    if let Ok(https_proxy) = std::env::var("HTTPS_PROXY") {
        let proxy = reqwest::Proxy::https(&https_proxy)
            .map_err(|e| anyhow!("Invalid HTTPS_PROXY URL: {}", e))?;
        client_builder = client_builder.proxy(proxy);
    } else if let Ok(https_proxy_lower) = std::env::var("https_proxy") {
        let proxy = reqwest::Proxy::https(&https_proxy_lower)
            .map_err(|e| anyhow!("Invalid https_proxy URL: {}", e))?;
        client_builder = client_builder.proxy(proxy);
    }

    // Build the client.
    let client = client_builder.build()?;

    Ok(client)
}

//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use actix_web::web;

    use ten_manager::designer::registry::packages::get_packages_endpoint;
    use ten_rust::pkg_info::pkg_type::PkgType;

    use crate::test_case::common::builtin_server::start_test_server;

    #[actix_rt::test]
    async fn test_get_packages_success() {
        // Start the http server and get its address.
        let server_addr =
            start_test_server("/api/designer/v1/packages", || {
                web::get().to(get_packages_endpoint)
            })
            .await;
        println!("Server started at: {server_addr}");

        // Create query parameters
        let pkg_type = PkgType::Extension;
        let name = "ext_a";
        let version_req = "1.0.0";

        // Create a reqwest client to send the request.
        let client = reqwest::Client::builder()
            .no_proxy() // Disable using proxy from environment variables.
            .build()
            .expect("Failed to build client");

        // Construct the full URL using the server address with query
        // parameters.
        let url = format!(
            "http://{server_addr}/api/designer/v1/packages?pkg_type={pkg_type}&name={name}&version_req={version_req}"
        );
        println!("Sending request to URL: {url}");

        // Send the GET request with query parameters.
        let response =
            client.get(&url).send().await.expect("Failed to send request");

        assert_eq!(response.status(), 200);
        let body = response.text().await.expect("Failed to read response");
        println!("Response body: {body}");
    }
}

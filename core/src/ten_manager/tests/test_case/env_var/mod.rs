//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use std::env;

    use actix_web::{test, web, App};
    use ten_manager::designer::env_var::{
        get_env_var_endpoint, GetEnvVarRequestPayload, GetEnvVarResponseData,
    };

    use crate::test_case::common::designer_state::create_designer_state;

    #[actix_web::test]
    async fn test_get_existing_env_var() {
        // Setup
        const TEST_VAR_NAME: &str = "TEST_ENV_VAR_EXISTING";
        const TEST_VAR_VALUE: &str = "test_value";

        // Set the environment variable
        env::set_var(TEST_VAR_NAME, TEST_VAR_VALUE);

        // Create test app with the endpoint
        let state = web::Data::new(create_designer_state());
        let app = test::init_service(
            App::new()
                .app_data(state.clone())
                .route("/env-var", web::post().to(get_env_var_endpoint)),
        )
        .await;

        // Create test request
        let req = test::TestRequest::post()
            .uri("/env-var")
            .set_json(&GetEnvVarRequestPayload {
                name: TEST_VAR_NAME.to_string(),
            })
            .to_request();

        // Send request and get response.
        let resp: GetEnvVarResponseData = test::call_and_read_body_json(&app, req).await;

        // Clean up.
        env::remove_var(TEST_VAR_NAME);

        // Verify response.
        assert_eq!(resp.value, Some(TEST_VAR_VALUE.to_string()));
    }

    #[actix_web::test]
    async fn test_get_nonexistent_env_var() {
        // Setup - using a variable name that is unlikely to exist.
        const TEST_VAR_NAME: &str = "TEST_ENV_VAR_NONEXISTENT_12345";

        // Make sure the variable doesn't exist.
        env::remove_var(TEST_VAR_NAME);

        // Create test app with the endpoint
        let state = web::Data::new(create_designer_state());
        let app = test::init_service(
            App::new()
                .app_data(state.clone())
                .route("/env-var", web::post().to(get_env_var_endpoint)),
        )
        .await;

        // Create test request.
        let req = test::TestRequest::post()
            .uri("/env-var")
            .set_json(&GetEnvVarRequestPayload {
                name: TEST_VAR_NAME.to_string(),
            })
            .to_request();

        // Send request and get response.
        let resp: GetEnvVarResponseData = test::call_and_read_body_json(&app, req).await;

        // Verify response.
        assert_eq!(resp.value, None);
    }

    #[actix_web::test]
    async fn test_get_empty_env_var() {
        // Setup.
        const TEST_VAR_NAME: &str = "TEST_ENV_VAR_EMPTY";

        // Set the environment variable to an empty string.
        env::set_var(TEST_VAR_NAME, "");

        // Create test app with the endpoint.
        let state = web::Data::new(create_designer_state());
        let app = test::init_service(
            App::new()
                .app_data(state.clone())
                .route("/env-var", web::post().to(get_env_var_endpoint)),
        )
        .await;

        // Create test request.
        let req = test::TestRequest::post()
            .uri("/env-var")
            .set_json(&GetEnvVarRequestPayload {
                name: TEST_VAR_NAME.to_string(),
            })
            .to_request();

        // Send request and get response.
        let resp: GetEnvVarResponseData = test::call_and_read_body_json(&app, req).await;

        // Clean up.
        env::remove_var(TEST_VAR_NAME);

        // Verify response - an empty string is still a value, not None.
        assert_eq!(resp.value, Some("".to_string()));
    }
}

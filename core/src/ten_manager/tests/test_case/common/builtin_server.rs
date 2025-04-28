//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{thread, time::Duration};

use actix_cors::Cors;
use actix_web::{http::header, web, App, HttpServer};
use tokio::{self, sync::oneshot};

use super::designer_state::create_designer_state;

/// Starts a test server for the given endpoint path and returns the server
/// address. This is a common function used by multiple test cases to avoid
/// code duplication.
pub async fn start_test_server<F>(
    endpoint_path: &str,
    endpoint_factory: F,
) -> std::net::SocketAddr
where
    F: Fn() -> actix_web::Route + Clone + Send + 'static,
{
    println!("Starting test server for endpoint: {}", endpoint_path);

    // Create channel for server address.
    let (addr_tx, addr_rx) = oneshot::channel();

    // Start server in a separate thread.
    let endpoint_path = endpoint_path.to_string();
    thread::spawn(move || {
        println!("Server thread started");

        let rt = tokio::runtime::Runtime::new().unwrap();
        rt.block_on(async {
            let designer_state = create_designer_state();

            // Start the server.
            let server = HttpServer::new(move || {
                let cors = Cors::default()
                    .allow_any_origin()
                    .allowed_methods(vec!["GET", "POST", "PUT", "DELETE"])
                    .allowed_headers(vec![
                        header::AUTHORIZATION,
                        header::ACCEPT,
                        header::CONTENT_TYPE,
                    ])
                    .max_age(3600);

                App::new()
                    .app_data(web::Data::new(designer_state.clone()))
                    .wrap(cors)
                    .route(&endpoint_path, endpoint_factory())
            })
            // Using port 0 for automatic port selection.
            .bind("127.0.0.1:0")
            .unwrap();

            // Get the actual server address.
            let server_addr = server.addrs()[0];
            println!("Server bound to address: {}", server_addr);
            let _ = addr_tx.send(server_addr);

            // Run the server - this will block until the server is stopped or
            // the thread is terminated.
            println!("Starting server run");
            server.run().await.unwrap();
        });
    });

    // Wait for the server to start and get the address.
    let server_addr = addr_rx.await.unwrap();

    // Increase delay to ensure server is fully initialized.
    tokio::time::sleep(Duration::from_millis(500)).await;
    println!("Server initialization delay completed");

    server_addr
}

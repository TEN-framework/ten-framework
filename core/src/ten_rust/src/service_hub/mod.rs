//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
mod api;
mod bindings;
mod telemetry;

use std::{ffi::CStr, os::raw::c_char, ptr, thread};

use actix_web::{web, App, HttpServer};
use anyhow::Result;
use futures::{channel::oneshot, future::select, FutureExt};
use prometheus::Registry;

use crate::constants::{
    SERVICE_HUB_SERVER_BIND_MAX_RETRIES, SERVICE_HUB_SERVER_BIND_RETRY_INTERVAL_SECS,
};

pub struct ServiceHub {
    /// The Prometheus registry.
    registry: Registry,

    /// The server thread handle.
    server_thread_handle: Option<thread::JoinHandle<()>>,

    /// Used to send a shutdown signal to the server thread.
    server_thread_shutdown_tx: Option<oneshot::Sender<()>>,
}

/// Configure API and telemetry endpoints.
///
/// This function can be used to configure routes on an App based on what
/// endpoint they should be bound to. It will configure telemetry endpoints, API
/// endpoints, or both depending on which endpoint is being bound.
fn configure_routes(
    cfg: &mut web::ServiceConfig,
    registry: Registry,
    is_telemetry_endpoint: bool,
    is_api_endpoint: bool,
) {
    if is_telemetry_endpoint {
        // Configure telemetry endpoint.
        telemetry::configure_telemetry_route(cfg, registry.clone());
    }

    if is_api_endpoint {
        // Configure API endpoints.
        api::configure_api_route(cfg);
    }
}

fn determine_binding_addresses(
    telemetry_endpoint: &Option<String>,
    api_endpoint: &Option<String>,
) -> Vec<String> {
    // Determine if both endpoints are available and if they are different.
    let has_both_endpoints = telemetry_endpoint.is_some() && api_endpoint.is_some();
    let same_endpoint = has_both_endpoints && telemetry_endpoint == api_endpoint;

    if has_both_endpoints && !same_endpoint {
        // Both endpoints are different, bind to both.
        vec![telemetry_endpoint.as_ref().cloned(), api_endpoint.as_ref().cloned()]
            .into_iter()
            .flatten()
            .collect::<Vec<_>>()
    } else if same_endpoint {
        // Both endpoints are the same, bind to just one.
        telemetry_endpoint.as_ref().map(|e| vec![e.clone()]).unwrap_or_default()
    } else if telemetry_endpoint.is_some() {
        // Only telemetry endpoint.
        telemetry_endpoint.as_ref().map(|e| vec![e.clone()]).unwrap_or_default()
    } else {
        // Only API endpoint.
        api_endpoint.as_ref().map(|e| vec![e.clone()]).unwrap_or_default()
    }
}

/// Creates a configured App based on the provided endpoints.
///
/// This function configures routes for the App based on whether the telemetry
/// and API endpoints are provided, and whether they are the same or different.
fn create_server_app(
    registry: Registry,
    telemetry_endpoint: &Option<String>,
    api_endpoint: &Option<String>,
) -> App<
    impl actix_web::dev::ServiceFactory<
        actix_web::dev::ServiceRequest,
        Config = (),
        Response = actix_web::dev::ServiceResponse,
        Error = actix_web::Error,
        InitError = (),
    >,
> {
    let has_both = telemetry_endpoint.is_some() && api_endpoint.is_some();
    let same_ep = has_both && telemetry_endpoint == api_endpoint;

    let app_builder = App::new();

    if has_both && !same_ep {
        // Both endpoints are provided and they are different. We need to use
        // guards to ensure the right routes are bound to the right endpoints.
        let telemetry_port =
            telemetry_endpoint.as_ref().and_then(|s| s.rsplit(':').next().map(|p| p.to_string()));

        let api_port =
            api_endpoint.as_ref().and_then(|s| s.rsplit(':').next().map(|p| p.to_string()));

        app_builder
            // Add telemetry routes with guard.
            .service(
                web::scope("")
                    .guard(actix_web::guard::fn_guard(move |ctx| {
                        if let Some(port) = &telemetry_port {
                            // Get the host/port info from the request.
                            let host =
                                ctx.head().uri.authority().map(|auth| auth.as_str()).unwrap_or("");

                            // Check if the port matches.
                            host.rsplit(':').next().map(|p| p == port).unwrap_or(false)
                        } else {
                            false
                        }
                    }))
                    .configure(|cfg| configure_routes(cfg, registry.clone(), true, false)),
            )
            // Add API routes with guard.
            .service(
                web::scope("")
                    .guard(actix_web::guard::fn_guard(move |ctx| {
                        if let Some(port) = &api_port {
                            // Get the host/port info from the request.
                            let host =
                                ctx.head().uri.authority().map(|auth| auth.as_str()).unwrap_or("");

                            // Check if the port matches.
                            host.rsplit(':').next().map(|p| p == port).unwrap_or(false)
                        } else {
                            false
                        }
                    }))
                    .configure(|cfg| configure_routes(cfg, registry.clone(), false, true)),
            )
    } else {
        // Either single endpoint or both endpoints are the same.
        // Configure all relevant routes for the endpoint(s).
        let is_telemetry = telemetry_endpoint.is_some();
        let is_api = api_endpoint.is_some();

        app_builder.configure(|cfg| configure_routes(cfg, registry.clone(), is_telemetry, is_api))
    }
}

/// Create a server and attempt to bind it to the given addresses.
///
/// Returns a tuple containing:
/// - An Option with the bound server if successful, or None if binding failed
/// - A vector of error messages if binding failed
fn create_server_and_bind_to_addresses(
    binding_addresses: &[String],
    registry: Registry,
    telemetry_endpoint: &Option<String>,
    api_endpoint: &Option<String>,
) -> (Option<actix_web::dev::Server>, Vec<String>) {
    let mut bind_errors = Vec::new();

    // Create the necessary parameters for the server factory
    let registry_clone = registry.clone();
    let telemetry_endpoint_clone = telemetry_endpoint.clone();
    let api_endpoint_clone = api_endpoint.clone();

    // Create a new server with a factory that uses the create_server_app
    // function
    let server = HttpServer::new(move || {
        create_server_app(registry_clone.clone(), &telemetry_endpoint_clone, &api_endpoint_clone)
    })
    .shutdown_timeout(0)
    .backlog(1024);

    let bound_server = if binding_addresses.len() == 1 {
        // Single address binding is simple.
        match server.bind(&binding_addresses[0]) {
            Ok(server) => {
                eprintln!("Successfully bound to endpoint: {}", binding_addresses[0]);
                Some(server.run())
            }
            Err(e) => {
                let error_msg =
                    format!("Failed to bind to address {}: {:?}", binding_addresses[0], e);
                eprintln!("{error_msg}");
                bind_errors.push(error_msg);
                None
            }
        }
    } else if binding_addresses.len() == 2 {
        // For two addresses, try binding to both.
        let result = server.bind(&binding_addresses[0]).and_then(|server| {
            eprintln!("Successfully bound to endpoint: {}", binding_addresses[0]);
            // Try binding to second address.
            server.bind(&binding_addresses[1])
        });

        match result {
            Ok(server) => {
                // Successfully bound to both addresses.
                eprintln!("Successfully bound to endpoint: {}", binding_addresses[1]);
                Some(server.run())
            }
            Err(e) => {
                // Failed to bind to at least one address.
                let error_msg = format!("Failed to bind to addresses: {e:?}");
                eprintln!("{error_msg}");
                bind_errors.push(error_msg);
                None
            }
        }
    } else {
        None
    };

    (bound_server, bind_errors)
}

/// Creates an HTTP server with retry mechanism if binding fails.
///
/// This function attempts to bind an HTTP server to the specified endpoints.
/// If binding fails, it will retry up to a configured maximum number of
/// attempts with a delay between each attempt.
///
/// The server can bind to:
/// - Just a telemetry endpoint
/// - Just an API endpoint
/// - Both endpoints if they are different
///
/// Each endpoint will have appropriate routes configured.
fn create_service_hub_server_with_retry(
    telemetry_endpoint: &Option<String>,
    api_endpoint: &Option<String>,
    registry: Registry,
) -> Option<actix_web::dev::Server> {
    // If both endpoints are None, return None early.
    if telemetry_endpoint.is_none() && api_endpoint.is_none() {
        return None;
    }

    // Determine which addresses to bind to.
    let binding_addresses = determine_binding_addresses(telemetry_endpoint, api_endpoint);

    // Try to bind a few times with retry.
    for i in 0..SERVICE_HUB_SERVER_BIND_MAX_RETRIES {
        let (server, errors) = create_server_and_bind_to_addresses(
            &binding_addresses,
            registry.clone(),
            telemetry_endpoint,
            api_endpoint,
        );

        // If we've successfully bound to the specified addresses, return the
        // server.
        if server.is_some() {
            return server;
        }

        // If we've reached the maximum number of attempts, log the error and
        // return None.
        if i >= SERVICE_HUB_SERVER_BIND_MAX_RETRIES - 1 {
            eprintln!(
                "Error binding to addresses: {:?} after {} attempts. Errors: {:?}",
                binding_addresses,
                i + 1,
                errors
            );

            // Provide a helpful message for common issues.
            eprintln!(
                "Check if another process is using these ports or if you have permission to bind \
                 to these addresses."
            );

            return None;
        }

        // Otherwise, log the error and retry after a delay.
        eprintln!(
            "Failed to bind to endpoints. Attempt {} of {}. Retrying in {} second{}...",
            i + 1,
            SERVICE_HUB_SERVER_BIND_MAX_RETRIES,
            SERVICE_HUB_SERVER_BIND_RETRY_INTERVAL_SECS,
            if SERVICE_HUB_SERVER_BIND_RETRY_INTERVAL_SECS == 1 { "" } else { "s" }
        );
        std::thread::sleep(std::time::Duration::from_secs(
            SERVICE_HUB_SERVER_BIND_RETRY_INTERVAL_SECS,
        ));
    }

    None
}

/// Creates and starts a server thread that runs the actix system with the
/// provided server.
///
/// This function encapsulates the logic for running an actix server in a
/// separate thread, handling both normal operation and shutdown requests.
fn create_service_hub_server_thread(
    server: actix_web::dev::Server,
) -> (thread::JoinHandle<()>, oneshot::Sender<()>) {
    // Get a handle to the server to control it later.
    let server_handle = server.handle();

    // Create a channel to send shutdown signals to the server thread.
    let (shutdown_tx, shutdown_rx) = oneshot::channel::<()>();

    // Spawn a new thread to run the actix system.
    let server_thread_handle = thread::spawn(move || {
        // Create a new actix system.
        let system = actix_rt::System::new();

        // Block on the async executor to run our server and shutdown logic.
        let result: Result<()> = system.block_on(async move {
            // Set up the concurrent execution of server and shutdown tasks.

            // The server task handles normal operation and error reporting.
            let server_future = async {
                match server.await {
                    Ok(_) => {
                        // Server completed normally (unlikely).
                        eprintln!("Endpoint server completed normally");
                    }
                    Err(e) => {
                        // Server encountered an error.
                        eprintln!("Endpoint server error: {e}");
                        // Force the entire process to exit immediately.
                        std::process::exit(-1);
                    }
                }
            }
            .fuse();

            // The shutdown task waits for a signal to gracefully stop the
            // server.
            let shutdown_future = async move {
                // Wait for shutdown signal.
                let _ = shutdown_rx.await;

                eprintln!("Shutting down endpoint server (graceful stop)...");

                // Gracefully stop the server.
                server_handle.stop(true).await;

                // Terminate the actix system after the server is fully down.
                actix_rt::System::current().stop();
            }
            .fuse();

            // Use `futures::select!` to concurrently execute both futures
            // and respond to whichever completes first.
            futures::pin_mut!(server_future, shutdown_future);
            select(server_future, shutdown_future).await;

            eprintln!("Endpoint server shut down.");
            Ok(())
        });

        // Handle any errors from the actix system.
        if let Err(e) = result {
            eprintln!("Fatal error in endpoint server thread: {e:?}");
            std::process::exit(-1);
        }
    });

    (server_thread_handle, shutdown_tx)
}

/// Initialize the endpoint system.
///
/// # Safety
///
/// This function takes raw C string pointers and port values.
/// The pointers must be valid and point to properly null-terminated strings or
/// be NULL. The returned pointer must be freed with `ten_service_hub_shutdown`
/// to avoid memory leaks.
#[no_mangle]
pub unsafe extern "C" fn ten_service_hub_create(
    telemetry_host: *const c_char,
    telemetry_port: u32,
    api_host: *const c_char,
    api_port: u32,
) -> *mut ServiceHub {
    // Check if both hosts are NULL, if so, return null.
    if telemetry_host.is_null() && api_host.is_null() {
        eprintln!("Both telemetry and API hosts are NULL, not starting service hub");
        return ptr::null_mut();
    }

    // Create a new Prometheus registry.
    let registry = Registry::new();

    // Convert C strings to Rust strings if not NULL.
    let telemetry_host_str = if !telemetry_host.is_null() {
        match CStr::from_ptr(telemetry_host).to_str() {
            Ok(s) if !s.trim().is_empty() => Some(s.to_string()),
            _ => None,
        }
    } else {
        None
    };

    let api_host_str = if !api_host.is_null() {
        match CStr::from_ptr(api_host).to_str() {
            Ok(s) if !s.trim().is_empty() => Some(s.to_string()),
            _ => None,
        }
    } else {
        None
    };

    // Format the endpoints if hosts are available.
    let telemetry_endpoint =
        telemetry_host_str.as_ref().map(|host| format!("{host}:{telemetry_port}"));
    let api_endpoint = api_host_str.as_ref().map(|host| format!("{host}:{api_port}"));

    // If both endpoints are the same and not None, use a single server.
    if telemetry_endpoint.is_some() && api_endpoint.is_some() {
        if telemetry_endpoint == api_endpoint {
            if let Some(endpoint) = telemetry_endpoint.as_ref() {
                eprintln!("Creating combined telemetry/API server at {endpoint}");

                // Create a server with both routes.
                let registry_clone = registry.clone();

                let server = match create_service_hub_server_with_retry(
                    &telemetry_endpoint,
                    &api_endpoint,
                    registry_clone,
                ) {
                    Some(server) => server,
                    None => {
                        eprintln!("Failed to bind server to {endpoint}");
                        return ptr::null_mut();
                    }
                };

                let (thread_handle, shutdown_tx) = create_service_hub_server_thread(server);

                return Box::into_raw(Box::new(ServiceHub {
                    registry,
                    server_thread_handle: Some(thread_handle),
                    server_thread_shutdown_tx: Some(shutdown_tx),
                }));
            } else {
                eprintln!("Unexpected error in service hub creation");
                return ptr::null_mut();
            }
        } else {
            // Both endpoints are different - we'll handle this with one HTTP
            // server instance that uses guards to route based on endpoint.
            eprintln!(
                "Creating service with telemetry at {} and API at {}",
                telemetry_endpoint.as_ref().unwrap(),
                api_endpoint.as_ref().unwrap()
            );

            let registry_clone = registry.clone();

            let server = match create_service_hub_server_with_retry(
                &telemetry_endpoint,
                &api_endpoint,
                registry_clone,
            ) {
                Some(server) => server,
                None => {
                    eprintln!(
                        "Failed to bind server to endpoints {} and {}",
                        telemetry_endpoint.as_ref().unwrap(),
                        api_endpoint.as_ref().unwrap()
                    );
                    return ptr::null_mut();
                }
            };

            let (thread_handle, shutdown_tx) = create_service_hub_server_thread(server);

            return Box::into_raw(Box::new(ServiceHub {
                registry,
                server_thread_handle: Some(thread_handle),
                server_thread_shutdown_tx: Some(shutdown_tx),
            }));
        }
    }

    if telemetry_endpoint.is_some() && api_endpoint.is_none() {
        // Telemetry only.
        if let Some(endpoint) = telemetry_endpoint.as_ref() {
            eprintln!("Creating telemetry-only server at {endpoint}");

            // Create telemetry server with retry mechanism.
            let registry_clone = registry.clone();

            let server = match create_service_hub_server_with_retry(
                &telemetry_endpoint,
                &None,
                registry_clone,
            ) {
                Some(server) => server,
                None => {
                    eprintln!("Failed to bind telemetry server to {endpoint}");
                    return ptr::null_mut();
                }
            };

            let (thread_handle, shutdown_tx) = create_service_hub_server_thread(server);

            return Box::into_raw(Box::new(ServiceHub {
                registry,
                server_thread_handle: Some(thread_handle),
                server_thread_shutdown_tx: Some(shutdown_tx),
            }));
        }
    }

    if api_endpoint.is_some() && telemetry_endpoint.is_none() {
        // API only.
        if let Some(endpoint) = api_endpoint.as_ref() {
            eprintln!("Creating API-only server at {endpoint}");

            // Create API server with retry mechanism.
            let registry_clone = registry.clone();

            let server =
                match create_service_hub_server_with_retry(&None, &api_endpoint, registry_clone) {
                    Some(server) => server,
                    None => {
                        eprintln!("Failed to bind API server to {endpoint}");
                        return ptr::null_mut();
                    }
                };

            let (thread_handle, shutdown_tx) = create_service_hub_server_thread(server);

            return Box::into_raw(Box::new(ServiceHub {
                registry,
                server_thread_handle: Some(thread_handle),
                server_thread_shutdown_tx: Some(shutdown_tx),
            }));
        }
    }

    // This should never happen due to the checks above.
    eprintln!("Unexpected error in service hub creation");
    ptr::null_mut()
}

/// Shut down the endpoint system, stop the server, and clean up all resources.
///
/// This function implements a graceful shutdown with proper resource cleanup:
/// 1. Sends a shutdown signal to the server
/// 2. Waits for the server thread to complete with a timeout
/// 3. Ensures all resources are properly released
///
/// # Safety
///
/// This function assumes that `system_ptr` is either null or a valid pointer to
/// a `ServiceHub` that was previously created with
/// `ten_service_hub_create`. Calling this function with an invalid pointer
/// will lead to undefined behavior.
#[no_mangle]
pub unsafe extern "C" fn ten_service_hub_shutdown(service_hub_ptr: *mut ServiceHub) {
    debug_assert!(!service_hub_ptr.is_null(), "System pointer is null");
    // Early return for null pointers.
    if service_hub_ptr.is_null() {
        eprintln!("Warning: Attempt to shut down null ServiceHub pointer");
        return;
    }

    // Retrieve ownership using `Box::from_raw`. This transfers ownership to
    // Rust, and the Box will be automatically dropped when it goes out of
    // scope.
    let service_hub = Box::from_raw(service_hub_ptr);

    // Notify the actix system to shut down through the `oneshot` channel.
    if let Some(shutdown_tx) = service_hub.server_thread_shutdown_tx {
        eprintln!("Shutting down service hub...");
        if let Err(e) = shutdown_tx.send(()) {
            eprintln!("Failed to send shutdown signal: {e:?}");
            // Don't panic, just continue with cleanup.
            eprintln!("Continuing with cleanup despite shutdown signal failure");
        }
    } else {
        eprintln!("No shutdown channel available for the service hub");
    }

    // Wait for the server thread to complete with a timeout.
    if let Some(server_thread_handle) = service_hub.server_thread_handle {
        eprintln!("Waiting for service hub to shut down...");

        // Define a timeout for the join operation.
        const SHUTDOWN_TIMEOUT_SECS: u64 = 10;

        // We use std::thread::scope to ensure the spawned thread is joined
        // This prevents thread leaks even if an error occurs.
        std::thread::scope(|s| {
            // Create a timeout channel for coordination.
            let (tx, rx) = std::sync::mpsc::channel();

            // Spawn a scoped thread to join the server thread.
            s.spawn(move || {
                let join_result = server_thread_handle.join();

                // Send result, ignore errors if receiver dropped.
                let _ = tx.send(join_result);
            });

            // Wait with timeout.
            match rx.recv_timeout(std::time::Duration::from_secs(SHUTDOWN_TIMEOUT_SECS)) {
                Ok(join_result) => match join_result {
                    Ok(_) => {
                        eprintln!("Service hub server thread joined successfully")
                    }
                    Err(e) => eprintln!("Error joining service hub server thread: {e:?}"),
                },
                Err(std::sync::mpsc::RecvTimeoutError::Timeout) => {
                    eprintln!(
                        "WARNING: Service hub server thread did not shut down within timeout \
                         ({SHUTDOWN_TIMEOUT_SECS}s)"
                    );
                    eprintln!("The thread may still be running, potentially leaking resources");
                }
                Err(std::sync::mpsc::RecvTimeoutError::Disconnected) => {
                    eprintln!(
                        "ERROR: Channel disconnected while waiting for server thread to join"
                    );
                }
            }

            // The scoped thread is automatically joined when we exit this
            // scope.
        });
    } else {
        eprintln!("No thread handle available for the service hub");
    }

    // The system will be automatically dropped here, cleaning up all resources.
    eprintln!("Service hub resources cleaned up");
}

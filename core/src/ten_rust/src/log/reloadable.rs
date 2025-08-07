//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::sync::{Arc, Mutex};
use tracing_subscriber::{
    fmt::{self as tracing_fmt},
    layer::SubscriberExt,
    reload,
    util::SubscriberInitExt,
    EnvFilter, Layer, Registry,
};

use crate::log::{create_layer_and_filter, AdvancedLogConfig, LogInitError};

const MAX_HANDLERS: usize = 5;

type LogLayer = Box<dyn Layer<Registry> + Send + Sync>;
type LayerReloadHandle = reload::Handle<LogLayer, Registry>;
type FilterReloadHandle = reload::Handle<EnvFilter, Registry>;

/// Handle for a reloadable logging layer
struct LogLayerHandle {
    layer_handle: LayerReloadHandle,
    filter_handle: FilterReloadHandle,
    is_active: bool,
}

impl LogLayerHandle {
    fn new(
        layer_handle: LayerReloadHandle,
        filter_handle: FilterReloadHandle,
    ) -> Self {
        Self { layer_handle, filter_handle, is_active: false }
    }

    fn reload(&self, layer: Option<LogLayer>, filter: EnvFilter) {
        // Update layer
        if let Some(layer) = layer {
            let err = self.filter_handle.modify(|f| *f = filter);
            if let Err(e) = err {
                println!("Failed to reload filter: {e}");
            }

            let err = self.layer_handle.modify(|l| *l = layer);
            if let Err(e) = err {
                println!("Failed to reload layer: {e}");
            }
        } else {
            let err = self.filter_handle.modify(|f| *f = filter);
            if let Err(e) = err {
                println!("Failed to reload filter: {e}");
            }
        }
    }
}

/// Global logging manager that supports dynamic reconfiguration
struct LogManager {
    layer_handles: Vec<LogLayerHandle>,
    /// Guard for each layer that must be kept alive
    guards: Vec<Option<Box<dyn std::any::Any + Send + Sync>>>,
}

impl LogManager {
    fn new() -> Self {
        let mut layer_handles = Vec::with_capacity(MAX_HANDLERS);
        let mut layers = Vec::with_capacity(MAX_HANDLERS);
        let mut guards = Vec::with_capacity(MAX_HANDLERS);
        for _ in 0..MAX_HANDLERS {
            guards.push(None);
        }

        // Initialize all layer handles
        for _ in 0..MAX_HANDLERS {
            // Create initial layer and filter
            let initial_layer: LogLayer = Box::new(tracing_fmt::Layer::new());
            let initial_filter = EnvFilter::new("off");

            // Create reloadable layer and filter
            let (layer, layer_handle) = reload::Layer::new(initial_layer);
            let (filter, filter_handle) = reload::Layer::new(initial_filter);

            // Combine layer and filter
            let combined_layer =
                Box::new(layer.with_filter(filter)) as LogLayer;
            layers.push(combined_layer);
            layer_handles
                .push(LogLayerHandle::new(layer_handle, filter_handle));
        }

        // Initialize the registry
        tracing_subscriber::registry()
            .with(layers)
            .try_init()
            .expect("Failed to initialize registry");

        Self { layer_handles, guards }
    }

    fn update_config(&mut self, config: &AdvancedLogConfig) {
        // Update existing handlers
        for (i, handle) in self.layer_handles.iter_mut().enumerate() {
            if let Some(handler) = config.handlers.get(i) {
                let (layer_with_guard, filter) =
                    create_layer_and_filter(handler);

                // Update guard for this handler
                self.guards[i] = layer_with_guard.guard;

                handle.reload(Some(layer_with_guard.layer), filter);
                handle.is_active = true;
            } else {
                // If the number of handlers in the config is less than
                // MAX_HANDLERS, turn off the excess layers
                self.guards[i] = None; // Clear any existing guard
                handle.reload(None, EnvFilter::new("off"));
                handle.is_active = false;
            }
        }
    }
}

// Global logging manager instance
static LOG_MANAGER: once_cell::sync::Lazy<Arc<Mutex<LogManager>>> =
    once_cell::sync::Lazy::new(|| Arc::new(Mutex::new(LogManager::new())));

/// Configure the logging system with support for dynamic reconfiguration
///
/// This function can be called multiple times to dynamically update the logging
/// configuration. It will:
/// 1. Initialize the global logging manager on first call
/// 2. Update active log handlers based on the provided configuration
/// 3. Turn off unused log handlers
///
/// # Arguments
/// * `config` - The logging configuration
///
/// # Notes
/// * Supports up to 5 concurrent log handlers
/// * If more than 5 handlers are configured, extra handlers will be ignored
pub fn ten_configure_log_reloadable(
    config: &AdvancedLogConfig,
) -> Result<(), LogInitError> {
    if config.handlers.len() > MAX_HANDLERS {
        tracing::warn!(
            "Too many log handlers configured. Maximum is {}, but {} were \
             provided. Extra handlers will be ignored.",
            MAX_HANDLERS,
            config.handlers.len()
        );

        return Err(LogInitError {
            message: "Too many log handlers configured",
        });
    }

    // Get the global logging manager and update configuration
    if let Ok(mut manager) = LOG_MANAGER.lock() {
        manager.update_config(config);
    } else {
        return Err(LogInitError { message: "Failed to lock logging manager" });
    }

    Ok(())
}

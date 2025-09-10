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
    Layer, Registry,
};

use crate::log::{
    create_layer_with_dynamic_filter, file_appender::FileAppenderGuard, AdvancedLogConfig,
    AdvancedLogHandler, LogInitError,
};

const MAX_HANDLERS: usize = 5;

type LogLayer = Box<dyn Layer<Registry> + Send + Sync>;
type LayerReloadHandle = reload::Handle<LogLayer, Registry>;

/// Handle for a reloadable logging layer
struct LogLayerHandle {
    layer_handle: LayerReloadHandle,
    /// Store the current handler configuration for rebuilding the layer
    current_handler: Option<AdvancedLogHandler>,
    is_active: bool,
}

impl LogLayerHandle {
    fn new(layer_handle: LayerReloadHandle) -> Self {
        Self {
            layer_handle,
            current_handler: None,
            is_active: false,
        }
    }

    fn reload(
        &mut self,
        handler: Option<AdvancedLogHandler>,
        guard: &mut Option<Box<dyn std::any::Any + Send + Sync>>,
    ) {
        if let Some(handler) = handler {
            // Create new layer with the updated handler configuration
            let layer_with_guard = create_layer_with_dynamic_filter(&handler);

            // Update the guard
            *guard = layer_with_guard.guard;

            // Reload the layer
            let err = self.layer_handle.modify(|l| *l = layer_with_guard.layer);
            if let Err(e) = err {
                tracing::error!("Failed to reload layer: {e}");
            }

            // Store the current handler configuration
            self.current_handler = Some(handler);
            self.is_active = true;
        } else {
            // Disable this layer by providing a dummy layer that filters out everything
            use crate::log::dynamic_filter::DynamicTargetFilterLayer;
            let dummy_layer: LogLayer = Box::new(DynamicTargetFilterLayer::new(
                Box::new(tracing_fmt::Layer::new()),
                vec![], // No matchers means it will filter out everything
            ));
            let err = self.layer_handle.modify(|l| *l = dummy_layer);
            if let Err(e) = err {
                tracing::error!("Failed to disable layer: {e}");
            }

            // Clear the guard and handler
            *guard = None;
            self.current_handler = None;
            self.is_active = false;
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
            // Create initial disabled layer using our custom filter
            use crate::log::dynamic_filter::DynamicTargetFilterLayer;
            let initial_layer: LogLayer = Box::new(DynamicTargetFilterLayer::new(
                Box::new(tracing_fmt::Layer::new()),
                vec![], // No matchers means it will filter out everything
            ));

            // Create reloadable layer
            let (layer, layer_handle) = reload::Layer::new(initial_layer);

            layers.push(Box::new(layer) as LogLayer);
            layer_handles.push(LogLayerHandle::new(layer_handle));
        }

        // Initialize the registry
        tracing_subscriber::registry()
            .with(layers)
            .try_init()
            .expect("Failed to initialize registry");

        Self {
            layer_handles,
            guards,
        }
    }

    fn update_config(&mut self, config: &AdvancedLogConfig) {
        // Update existing handlers
        for (i, handle) in self.layer_handles.iter_mut().enumerate() {
            if let Some(handler) = config.handlers.get(i) {
                handle.reload(Some(handler.clone()), &mut self.guards[i]);
            } else {
                // If the number of handlers in the config is less than
                // MAX_HANDLERS, turn off the excess layers
                handle.reload(None, &mut self.guards[i]);
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
pub fn ten_configure_log_reloadable(config: &AdvancedLogConfig) -> Result<(), LogInitError> {
    if config.handlers.len() > MAX_HANDLERS {
        tracing::warn!(
            "Too many log handlers configured. Maximum is {}, but {} were provided. Extra \
             handlers will be ignored.",
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
        return Err(LogInitError {
            message: "Failed to lock logging manager",
        });
    }

    Ok(())
}

/// Request all file appenders managed by the reloadable log manager to reopen
/// on next write. No-op if the manager isn't initialized yet.
pub fn request_reopen_all_files() {
    if let Ok(manager) = LOG_MANAGER.lock() {
        for guard_opt in manager.guards.iter() {
            if let Some(any_guard) = guard_opt.as_ref() {
                if let Some(file_guard) = any_guard.downcast_ref::<FileAppenderGuard>() {
                    file_guard.request_reopen();
                }
            }
        }
    }
}

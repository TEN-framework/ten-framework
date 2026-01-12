//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

use tracing::Subscriber;
use tracing_subscriber::Layer;

use crate::log::{AdvancedLogLevelFilter, AdvancedLogMatcher};

/// A visitor to extract category field from tracing event
#[derive(Default)]
pub(crate) struct CategoryExtractor {
    pub category: Option<String>,
}

impl tracing_subscriber::field::Visit for CategoryExtractor {
    fn record_str(&mut self, field: &tracing::field::Field, value: &str) {
        if field.name() == "category" {
            self.category = Some(value.to_string());
        }
    }

    fn record_debug(&mut self, field: &tracing::field::Field, value: &dyn std::fmt::Debug) {
        if field.name() == "category" {
            self.category = Some(format!("{value:?}").trim_matches('"').to_string());
        }
    }
}

/// Custom layer that filters based on dynamic category field
pub(crate) struct DynamicTargetFilterLayer<L> {
    pub inner: L,
    pub matchers: Vec<AdvancedLogMatcher>,
}

impl<L> DynamicTargetFilterLayer<L> {
    pub fn new(inner: L, matchers: Vec<AdvancedLogMatcher>) -> Self {
        Self {
            inner,
            matchers,
        }
    }

    pub fn should_filter(
        &self,
        metadata: &tracing::Metadata<'_>,
        event: &tracing::Event<'_>,
    ) -> bool {
        let event_level = *metadata.level();

        // Extract category from event fields
        let mut target_extractor = CategoryExtractor::default();
        event.record(&mut target_extractor);

        // Use extracted category from fields, fallback to metadata target
        let category = target_extractor.category.as_deref().unwrap_or(metadata.target());

        // Find the most specific matching rule
        // Rules with category patterns are more specific than global rules
        // (category=None)
        let mut matching_rule: Option<&AdvancedLogMatcher> = None;

        // First, look for exact category matches
        // Use the last matching specific rule (later rules override earlier ones)
        for matcher in &self.matchers {
            if let Some(pattern) = &matcher.category {
                if category.starts_with(pattern) {
                    matching_rule = Some(matcher);
                    // Don't break - continue to find the last matching rule
                }
            }
        }

        // If no specific category rule found, look for global rules (category=None)
        // Use the last matching global rule (later rules override earlier ones)
        if matching_rule.is_none() {
            for matcher in &self.matchers {
                if matcher.category.is_none() {
                    matching_rule = Some(matcher);
                    // Don't break - continue to find the last matching rule
                }
            }
        }

        // Apply the matching rule
        if let Some(rule) = matching_rule {
            match rule.level {
                AdvancedLogLevelFilter::OFF => false, // Always filter out
                AdvancedLogLevelFilter::Debug => event_level <= tracing::Level::DEBUG,
                AdvancedLogLevelFilter::Info => event_level <= tracing::Level::INFO,
                AdvancedLogLevelFilter::Warn => event_level <= tracing::Level::WARN,
                AdvancedLogLevelFilter::Error => event_level <= tracing::Level::ERROR,
            }
        } else {
            // No matching rule, filter out by default
            false
        }
    }
}

impl<S, L> Layer<S> for DynamicTargetFilterLayer<L>
where
    S: Subscriber,
    L: Layer<S>,
{
    fn on_event(&self, event: &tracing::Event<'_>, ctx: tracing_subscriber::layer::Context<'_, S>) {
        // Skip logging if we're in panic unwinding or during TLS destruction.
        // During program exit, TLS may be destroyed while logging is still
        // being attempted, which can cause "cannot access a Thread Local
        // Storage value during or after destruction" panics.
        if std::thread::panicking() {
            return;
        }

        // Use catch_unwind to gracefully handle TLS access errors that can
        // occur during program exit. This is defensive programming to ensure
        // the logging system doesn't crash the process during shutdown.
        let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
            if self.should_filter(event.metadata(), event) {
                self.inner.on_event(event, ctx);
            }
        }));

        // Silently ignore any panics - they're likely due to TLS destruction
        // during program exit
        if result.is_err() {
            // Use eprintln! directly since tracing may not be available
            // We intentionally don't log this in production to avoid noise
            #[cfg(debug_assertions)]
            eprintln!(
                "[ten_rust::log] Caught panic in on_event (likely TLS destruction during exit)"
            );
        }
    }

    fn on_enter(&self, id: &tracing::span::Id, ctx: tracing_subscriber::layer::Context<'_, S>) {
        if std::thread::panicking() {
            return;
        }
        let _ = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
            self.inner.on_enter(id, ctx);
        }));
    }

    fn on_exit(&self, id: &tracing::span::Id, ctx: tracing_subscriber::layer::Context<'_, S>) {
        if std::thread::panicking() {
            return;
        }
        let _ = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
            self.inner.on_exit(id, ctx);
        }));
    }

    fn on_new_span(
        &self,
        attrs: &tracing::span::Attributes<'_>,
        id: &tracing::span::Id,
        ctx: tracing_subscriber::layer::Context<'_, S>,
    ) {
        if std::thread::panicking() {
            return;
        }
        let _ = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
            self.inner.on_new_span(attrs, id, ctx);
        }));
    }

    fn enabled(
        &self,
        metadata: &tracing::Metadata<'_>,
        ctx: tracing_subscriber::layer::Context<'_, S>,
    ) -> bool {
        if std::thread::panicking() {
            return false;
        }
        // Use catch_unwind and default to false if it panics (safe fallback)
        std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
            // We enable at the layer level, but filter at the event level
            self.inner.enabled(metadata, ctx)
        }))
        .unwrap_or(false)
    }
}

//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use std::{
    fs::{self, File},
    io::BufWriter,
    path::{Path, PathBuf},
    sync::Arc,
};

use anyhow::Result;
use tracing::Level;
use tracing_chrome::ChromeLayerBuilder;
use tracing_flame::FlameLayer;
use tracing_subscriber::{
    fmt::{self, format::FmtSpan},
    layer::SubscriberExt,
    util::SubscriberInitExt,
    EnvFilter, Layer,
};

use crate::output::TmanOutput;

/// Tracing mode
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TracingMode {
    /// Tracing disabled
    Disabled,
    /// Console output with timing statistics
    Console,
    /// Chrome tracing format (viewable at chrome://tracing)
    Chrome,
    /// Flamegraph format
    Flame,
    /// All enabled (console + chrome + flamegraph)
    All,
}

impl TracingMode {
    /// Create TracingMode from config value (--tracing flag)
    pub fn from_config(config_value: Option<&str>) -> Self {
        config_value.map(Self::from_str).unwrap_or(TracingMode::Disabled)
    }

    fn from_str(s: &str) -> Self {
        match s.to_lowercase().as_str() {
            "console" => TracingMode::Console,
            "chrome" => TracingMode::Chrome,
            "flame" => TracingMode::Flame,
            "all" => TracingMode::All,
            _ => TracingMode::Disabled,
        }
    }

    pub fn is_enabled(&self) -> bool {
        !matches!(self, TracingMode::Disabled)
    }
}

/// Tracing output path management
pub struct TracingOutputPaths {
    pub chrome_file: Option<PathBuf>,
    pub flame_file: Option<PathBuf>,
}

impl TracingOutputPaths {
    pub fn new(output_dir: Option<&Path>) -> Result<Self> {
        let base_dir = output_dir.map(|p| p.to_path_buf()).unwrap_or_else(|| {
            std::env::current_dir().unwrap_or_else(|_| PathBuf::from(".")).join("tman_tracing")
        });

        fs::create_dir_all(&base_dir)?;

        let timestamp = chrono::Local::now().format("%Y%m%d_%H%M%S");

        Ok(Self {
            chrome_file: Some(base_dir.join(format!("trace_{}.json", timestamp))),
            flame_file: Some(base_dir.join(format!("flamegraph_{}.svg", timestamp))),
        })
    }

    pub fn print_summary(&self, out: &dyn TmanOutput) {
        out.normal_line("");
        out.normal_line("📊 Tracing output files:");
        if let Some(chrome) = &self.chrome_file {
            out.normal_line(&format!("  🔍 Chrome Tracing: {}", chrome.display()));
            out.normal_line(
                "     How to view: Open Chrome browser, visit chrome://tracing, then drag and \
                 drop the file",
            );
        }
        if let Some(flame) = &self.flame_file {
            out.normal_line(&format!("  🔥 Flamegraph: {}", flame.display()));
            out.normal_line("     How to view: Open the SVG file directly in a browser");
        }
    }
}

/// Tracing guard, used to generate flamegraph on program exit
pub struct TracingGuard {
    flame_guard: Option<tracing_flame::FlushGuard<BufWriter<File>>>,
    flame_output_path: Option<PathBuf>,
    chrome_guard: Option<tracing_chrome::FlushGuard>,
    out: Arc<Box<dyn TmanOutput>>,
}

impl TracingGuard {
    pub fn new(
        flame_guard: Option<tracing_flame::FlushGuard<BufWriter<File>>>,
        flame_output_path: Option<PathBuf>,
        chrome_guard: Option<tracing_chrome::FlushGuard>,
        out: Arc<Box<dyn TmanOutput>>,
    ) -> Self {
        Self {
            flame_guard,
            flame_output_path,
            chrome_guard,
            out,
        }
    }
}

impl Drop for TracingGuard {
    fn drop(&mut self) {
        // Chrome tracing will flush automatically
        drop(self.chrome_guard.take());

        // Generate flamegraph
        if let (Some(flame_guard), Some(output_path)) =
            (self.flame_guard.take(), self.flame_output_path.take())
        {
            self.out.normal_line("");
            self.out.normal_line("🔥 Generating flamegraph...");

            // Flush flame guard
            drop(flame_guard);

            // Read raw data and generate SVG
            let folded_path = output_path.with_extension("folded");
            if let Ok(data) = fs::read(&folded_path) {
                if let Ok(svg) = generate_flamegraph_svg(&data) {
                    if let Err(e) = fs::write(&output_path, svg) {
                        self.out.error_line(&format!("⚠️  Failed to write flamegraph SVG: {}", e));
                    } else {
                        self.out.normal_line(&format!(
                            "✅ Flamegraph generated: {}",
                            output_path.display()
                        ));
                    }
                } else {
                    self.out.error_line("⚠️  Failed to generate flamegraph SVG");
                }
            } else {
                self.out.error_line(&format!(
                    "⚠️  Failed to read flamegraph data: {}",
                    folded_path.display()
                ));
            }
        }
    }
}

fn generate_flamegraph_svg(folded_data: &[u8]) -> Result<Vec<u8>> {
    let mut options = inferno::flamegraph::Options::default();
    options.title = "tman install Performance Analysis".to_string();
    options.subtitle = Some(
        "The width of each block represents the total time spent in that function".to_string(),
    );
    options.count_name = "time".to_string();

    let mut svg_output = Vec::new();
    inferno::flamegraph::from_reader(
        &mut options,
        std::io::Cursor::new(folded_data),
        &mut svg_output,
    )?;

    Ok(svg_output)
}

/// Initialize tracing system
pub fn init_tracing(
    mode: TracingMode,
    out: Arc<Box<dyn TmanOutput>>,
) -> Result<Option<TracingGuard>> {
    if !mode.is_enabled() {
        return Ok(None);
    }

    out.normal_line(&format!("🔍 Enabling tracing mode: {:?}", mode));
    out.normal_line("   Stack overflow detection: Will track maximum call stack depth");
    out.normal_line("   Performance analysis: Will record execution time of each function");
    out.normal_line("");

    let env_filter = EnvFilter::try_from_default_env()
        .unwrap_or_else(|_| EnvFilter::new("ten_manager=trace,info"));

    let paths = TracingOutputPaths::new(None)?;

    // Build subscriber
    let registry = tracing_subscriber::registry().with(env_filter);

    let mut chrome_guard = None;
    let mut flame_guard = None;
    let mut flame_output_path = None;

    match mode {
        TracingMode::Console => {
            // Enable console output only
            let fmt_layer = fmt::layer()
                .with_thread_ids(true)
                .with_thread_names(true)
                .with_span_events(FmtSpan::ENTER | FmtSpan::CLOSE)
                .with_target(true)
                .with_level(true)
                .with_filter(tracing::level_filters::LevelFilter::from_level(Level::TRACE));

            registry.with(fmt_layer).init();
        }
        TracingMode::Chrome => {
            // Enable Chrome tracing only
            let (chrome_layer, guard) = ChromeLayerBuilder::new()
                .file(paths.chrome_file.clone().unwrap())
                .include_args(true)
                .build();

            chrome_guard = Some(guard);

            registry.with(chrome_layer).init();

            paths.print_summary(out.as_ref().as_ref());
        }
        TracingMode::Flame => {
            // Enable flamegraph only
            let flame_path = paths.flame_file.clone().unwrap();
            let folded_path = flame_path.with_extension("folded");
            let (flame_layer, guard) = FlameLayer::with_file(&folded_path)?;

            flame_guard = Some(guard);
            flame_output_path = Some(flame_path);

            registry
                .with(
                    flame_layer
                        .with_filter(tracing::level_filters::LevelFilter::from_level(Level::TRACE)),
                )
                .init();

            paths.print_summary(out.as_ref().as_ref());
        }
        TracingMode::All => {
            // Enable all features
            let fmt_layer = fmt::layer()
                .with_thread_ids(true)
                .with_thread_names(true)
                .with_span_events(FmtSpan::ENTER | FmtSpan::CLOSE)
                .with_target(false)
                .with_level(true)
                .compact()
                .with_filter(tracing::level_filters::LevelFilter::from_level(Level::INFO));

            let (chrome_layer, guard) = ChromeLayerBuilder::new()
                .file(paths.chrome_file.clone().unwrap())
                .include_args(true)
                .build();

            chrome_guard = Some(guard);

            let flame_path = paths.flame_file.clone().unwrap();
            let folded_path = flame_path.with_extension("folded");
            let (flame_layer, guard2) = FlameLayer::with_file(&folded_path)?;

            flame_guard = Some(guard2);
            flame_output_path = Some(flame_path);

            registry
                .with(fmt_layer)
                .with(chrome_layer)
                .with(
                    flame_layer
                        .with_filter(tracing::level_filters::LevelFilter::from_level(Level::TRACE)),
                )
                .init();

            paths.print_summary(out.as_ref().as_ref());
        }
        TracingMode::Disabled => unreachable!(),
    }

    Ok(Some(TracingGuard::new(flame_guard, flame_output_path, chrome_guard, out)))
}

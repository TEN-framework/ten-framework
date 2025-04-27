//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use anyhow::Result;
use jaq_core::{Ctx, RcIter};
use serde_json::Value;

pub fn jq_run(input: Value, code: &str) -> Result<Value> {
    use jaq_core::load::{Arena, File, Loader};
    let arena = Arena::default();
    let loader = Loader::new(jaq_std::defs().chain(jaq_json::defs()));
    let modules = loader.load(&arena, File { path: (), code }).unwrap();
    let filter = jaq_core::Compiler::default()
        .with_funs(jaq_std::funs().chain(jaq_json::funs()))
        .compile(modules)
        .unwrap();

    let inputs = RcIter::new(core::iter::empty());
    let mut out = filter.run((Ctx::new([], &inputs), input.into()));

    if let Some(Ok(out)) = out.next() {
        return Ok(out.into());
    }

    Err(anyhow::anyhow!("empty output"))
}

/// Convert a path array to a jq filter string.
/// For example, ["a", "b", "c"] becomes ".a.b.c"
pub fn path_to_jq_filter(path: &[&str]) -> String {
    if path.is_empty() {
        return ".".to_string();
    }
    let mut result = String::from(".");
    result.push_str(&path.join("."));
    result
}

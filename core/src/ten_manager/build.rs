//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

fn main() {
    // Set stack size for Windows platform at link time
    #[cfg(target_os = "windows")]
    {
        // Set stack size to 32MB for Windows
        // MSVC uses /STACK:33554432, MinGW uses -Wl,--stack,33554432

        // Use TARGET environment variable to determine the actual target
        // This is more reliable than cfg! which reflects the host environment
        let target = std::env::var("TARGET").unwrap_or_default();

        if target.contains("msvc") {
            println!("cargo:rustc-link-arg=/STACK:33554432");
        } else if target.contains("gnu") || target.contains("mingw") {
            // MinGW/GNU ld format
            println!("cargo:rustc-link-arg=-Wl,--stack,33554432");

            // Disable gc-sections for MinGW to avoid linker errors with newer MinGW versions
            // This is necessary because --gc-sections can cause issues with .drectve sections
            // in some Rust dependencies
            // https://stackoverflow.com/questions/45077846/collect2-exe-error-ld-returned-5-exit-status

            // TODO(nzh): check if these args are necessary
            println!("cargo:rustc-link-arg=-Wl,--no-gc-sections");
            println!("cargo:rustc-link-arg=-Wl,-verbose");
        }
    }
}

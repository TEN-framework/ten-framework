use std::env;

fn main() {
    // update clingo submodule
    // git submodule update --init --recursive

    // // create bindings
    // let bindings = bindgen::Builder::default()
    //     .header("clingo/libclingo/clingo.h")
    //     .no_copy("clingo_solve_control")
    //     .no_copy("clingo_model")
    //     .no_copy("clingo_solve_handle")
    //     .no_copy("clingo_program_builder")
    //     .no_copy("clingo_control")
    //     .no_copy("clingo_options")
    //     .no_copy("clingo_symbolic_atoms")
    //     .no_copy("clingo_theory_atoms")
    //     .no_copy("clingo_assignment")
    //     .no_copy("clingo_propagate_init")
    //     .no_copy("clingo_propagate_control")
    //     .no_copy("clingo_backend")
    //     .no_copy("clingo_configuration")
    //     .no_copy("clingo_statistic")
    //     // .no_copy("clingo_ast_term")
    //     // .no_copy("clingo_ast_function")
    //     // .no_copy("clingo_ast_pool")
    //     // .no_copy("clingo_ast_csp_product_term_t")
    //     .blocklist_type("max_align_t") // https://github.com/rust-lang/rust-bindgen/issues/550
    //     .size_t_is_usize(true)
    //     .generate()
    //     .expect("Unable to generate bindings");

    // // write the bindings to the bindings.rs file.
    // bindings
    //     .write_to_file("bindings.rs")
    //     .expect("Couldn't write bindings!");

    if let Ok(_) = std::env::var("DOCS_RS") {
        // skip linking on docs.rs
        return;
    }

    let target_os = env::var("CARGO_CFG_TARGET_OS").unwrap();

    if env::var("CARGO_FEATURE_STATIC_LINKING").is_ok() {
        // build clingo for static linking

        use cmake::Config;
        let target_env = env::var("CARGO_CFG_TARGET_ENV").unwrap_or_default();

        let mut config = Config::new("clingo");
        config
            .very_verbose(true)
            .define("CLINGO_BUILD_SHARED", "OFF")
            .define("CLINGO_BUILD_STATIC", "ON")
            .define("CLINGO_MANAGE_RPATH", "OFF")
            .define("CLINGO_BUILD_WITH_PYTHON", "OFF")
            .define("CLINGO_BUILD_WITH_LUA", "OFF")
            .define("CLINGO_INSTALL_LIB", "ON")
            .define("CLINGO_BUILD_APPS", "OFF")
            .define("CLASP_BUILD_APP", "OFF");

        // For MinGW on Windows, specify the MinGW Makefiles generator
        // to ensure correct library file naming (libclingo.a instead of clingo.lib)
        if target_os.as_str() == "windows" && target_env == "gnu" {
            config.generator("MinGW Makefiles");

            // Force CMake to use MinGW GCC instead of Clang/LLVM
            // This ensures .a files are generated instead of .lib files
            if let Ok(cc) = env::var("CC") {
                config.define("CMAKE_C_COMPILER", &cc);
            } else {
                // Try to find gcc in PATH
                if let Ok(path) = std::process::Command::new("gcc")
                    .arg("--version")
                    .output()
                {
                    if path.status.success() {
                        config.define("CMAKE_C_COMPILER", "gcc");
                    }
                }
            }

            if let Ok(cxx) = env::var("CXX") {
                config.define("CMAKE_CXX_COMPILER", &cxx);
            } else {
                // Try to find g++ in PATH
                if let Ok(path) = std::process::Command::new("g++")
                    .arg("--version")
                    .output()
                {
                    if path.status.success() {
                        config.define("CMAKE_CXX_COMPILER", "g++");
                    }
                }
            }
        }

        let dst = config.build();

        let lib_dir = dst.join("lib");
        println!(
            "cargo:rustc-link-search=native={}",
            lib_dir.display()
        );

        // Verify that the library files exist
        // On MinGW, CMake should generate libclingo.a, but if it used Clang/LLVM
        // it might generate clingo.lib instead. We need to check for both.
        if target_os.as_str() == "windows" {
            if target_env == "gnu" {
                // MinGW: Check for libclingo.a first (expected)
                let lib_file_a = lib_dir.join("libclingo.a");
                let lib_file_lib = lib_dir.join("clingo.lib");

                if !lib_file_a.exists() && !lib_file_lib.exists() {
                    panic!(
                        "clingo static library not found at {} or {}. CMake build may have failed or used wrong toolchain (Clang instead of GCC).",
                        lib_file_a.display(),
                        lib_file_lib.display()
                    );
                }

                // If only .lib exists, CMake used Clang/LLVM instead of MinGW GCC
                // This is a problem because Rust expects .a files for MinGW target
                if !lib_file_a.exists() && lib_file_lib.exists() {
                    panic!(
                        "CMake generated clingo.lib (MSVC format) instead of libclingo.a (MinGW format). \
                        This indicates CMake used Clang/LLVM toolchain instead of MinGW GCC. \
                        Please ensure MinGW GCC (gcc/g++) is in PATH and CMake can find it, \
                        or set CC and CXX environment variables to point to MinGW GCC."
                    );
                }
            } else {
                // MSVC: Check for clingo.lib
                let lib_file = lib_dir.join("clingo.lib");
                if !lib_file.exists() {
                    panic!(
                        "clingo static library not found at {}. CMake build may have failed.",
                        lib_file.display()
                    );
                }
            }
        } else {
            // Unix-like (Linux, macOS, etc.): Check for libclingo.a
            let lib_file = lib_dir.join("libclingo.a");
            if !lib_file.exists() {
                panic!(
                    "clingo static library not found at {}. CMake build may have failed.",
                    lib_file.display()
                );
            }
        }

        println!("cargo:rustc-link-lib=static=clingo");
        println!("cargo:rustc-link-lib=static=reify");
        println!("cargo:rustc-link-lib=static=potassco");
        println!("cargo:rustc-link-lib=static=clasp");
        println!("cargo:rustc-link-lib=static=gringo");

        // Link C++ standard library
        if target_os.as_str() == "linux" {
            println!("cargo:rustc-link-lib=dylib=stdc++");
        } else if target_os.as_str() == "macos" {
            println!("cargo:rustc-link-lib=dylib=c++");
        } else if target_os.as_str() == "windows" {
            // MinGW uses stdc++ like Linux
            if target_env == "gnu" {
                // MinGW target
                println!("cargo:rustc-link-lib=dylib=stdc++");
            }
            // MSVC target doesn't need explicit C++ library linking
        }
    } else {
        let path = env::var("CLINGO_LIBRARY_PATH").expect("$CLINGO_LIBRARY_PATH should be defined");
        println!("cargo:rustc-link-search=native={}", path);

        if target_os.as_str() == "windows" {
            println!("cargo:rustc-link-lib=dylib=import_clingo");
        } else {
            println!("cargo:rustc-link-lib=dylib=clingo");
        }
    }
    //     println!("cargo:rustc-link-lib=python3.6m");
    //     -DWITH_PYTHON=1 -I/usr/include/python3.6m
}

use std::env;
use std::fs;
use std::path::PathBuf;

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

    // let target_os = env::var("CARGO_CFG_TARGET_OS").unwrap();
    let target_os =
        env::var("CARGO_CFG_TARGET_OS").unwrap_or_else(|_| "unknown".into());
    let is_windows = target_os == "windows";

    if env::var("CARGO_FEATURE_STATIC_LINKING").is_ok() {
        // build clingo for static linking

        use cmake::Config;
        let dst = Config::new("clingo")
            .very_verbose(true)
            .define("CLINGO_BUILD_SHARED", "OFF")
            .define("CLINGO_BUILD_STATIC", "ON")
            .define("CLINGO_MANAGE_RPATH", "OFF")
            .define("CLINGO_BUILD_WITH_PYTHON", "OFF")
            .define("CLINGO_BUILD_WITH_LUA", "OFF")
            .define("CLINGO_INSTALL_LIB", "ON")
            .define("CLINGO_BUILD_APPS", "OFF")
            .define("CLASP_BUILD_APP", "OFF")
            .build();

        println!(
            "cargo:rustc-link-search=native={}",
            dst.join("lib").display()
        );

        println!("cargo:rustc-link-lib=static=clingo");
        println!("cargo:rustc-link-lib=static=reify");
        println!("cargo:rustc-link-lib=static=potassco");
        println!("cargo:rustc-link-lib=static=clasp");
        println!("cargo:rustc-link-lib=static=gringo");

        // Possible library output directories (CMake output paths may vary
        // across platforms/generators)
        let mut libdirs: Vec<PathBuf> = vec![
            dst.join("lib"),
            dst.join("lib64"),
            dst.join("build"),
            dst.clone(),
            dst.join("Release"),
            dst.join("build").join("Release"),
            dst.join("Debug"),
            dst.join("build").join("Debug"),
        ];
        // remove duplicates & only keep existing directories
        libdirs.sort();
        libdirs.dedup();
        libdirs.retain(|p| p.is_dir());

        // help debug: list contents of candidate directories
        for d in &libdirs {
            if let Ok(entries) = fs::read_dir(d) {
                for e in entries.flatten() {
                    println!(
                        "cargo:warning=libdir entry: {}",
                        e.path().display()
                    );
                }
            }
        }

        // tell rustc these directories can find libraries
        for dir in &libdirs {
            println!("cargo:rustc-link-search=native={}", dir.display());
        }

        // base names for clingo static libraries
        const BASES: &[&str] =
            &["clingo", "reify", "potassco", "clasp", "gringo"];

        // find actual file names on disk, return the base string to pass to
        // rustc-link-lib
        fn find_actual_basename(
            dirlist: &[PathBuf],
            base: &str,
            is_windows: bool,
        ) -> Option<String> {
            let exts: &[&str] = if is_windows { &[".lib"] } else { &[".a"] };
            // different projects may add lib-prefix or -static suffix
            let candidates = [
                base.to_string(),
                format!("lib{base}"),
                format!("{base}-static"),
                format!("lib{base}-static"),
                // some projects use underscore style
                format!("{base}_static"),
                format!("lib{base}_static"),
            ];

            for dir in dirlist {
                for cand in &candidates {
                    for ext in exts {
                        let f = dir.join(format!("{cand}{ext}"));
                        if f.exists() {
                            return Some(cand.clone());
                        }
                    }
                }
            }
            None
        }

        // add each "existing" static library
        for base in BASES {
            if let Some(actual) =
                find_actual_basename(&libdirs, base, is_windows)
            {
                // note: here we use the actual base name, e.g. clingo-static /
                // libclingo-static
                println!("cargo:rustc-link-lib=static={actual}");
            } else {
                println!(
                    "cargo:warning=Did not find {base} static lib in built \
                     directories"
                );
            }
        }

        if target_os.as_str() == "linux" {
            println!("cargo:rustc-link-lib=dylib=stdc++");
        } else if target_os.as_str() == "macos" {
            println!("cargo:rustc-link-lib=dylib=c++");
        }
    } else {
        let path = env::var("CLINGO_LIBRARY_PATH")
            .expect("$CLINGO_LIBRARY_PATH should be defined");
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

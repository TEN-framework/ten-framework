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

        if target_os.as_str() == "linux" {
            println!("cargo:rustc-link-lib=dylib=stdc++");
        } else if target_os.as_str() == "macos" {
            println!("cargo:rustc-link-lib=dylib=c++");
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

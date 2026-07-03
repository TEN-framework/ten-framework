# clingo-sys [![Build Status](https://github.com/potassco/clingo-sys/workflows/CI%20Test/badge.svg)](https://github.com/potassco/clingo-sys)[![Latest Version](https://img.shields.io/crates/v/clingo-sys.svg)](https://crates.io/crates/clingo-sys)[![Rust Documentation](https://docs.rs/clingo-sys/badge.svg)](https://docs.rs/clingo-sys)

Rust raw FFI bindings to the C API of [clingo](https://github.com/potassco/clingo) library.
Clingo version 5.6.2.

## Requirements library

Per default the crate uses the clingo library via dynamic linking.
It is assumed that a clingo dynamic library is installed on the system.
You have to set the environment variable `CLINGO_LIBRARY_PATH`. For example:

```sh
export CLINGO_LIBRARY_PATH=/scratch/miniconda3/envs/test/lib
```

## Using `static-linking`

The crate defines a [Cargo feature] that allows to use the clingo library via static linking.

The recommended way to use the optional static linking support is as
follows.

```toml
[dependencies]
clingo-sys = { version = "0.7.2", features = ["static-linking"] }
```

To build clingo for static linking you need the following tools installed:

- a C++14 conforming compiler
  - *at least* [GCC](https://gcc.gnu.org/) version 4.9
  - [Clang](http://clang.llvm.org/) version 3.1 (using either libstdc++
    provided by gcc 4.9 or libc++)
  - *at least* MSVC 15.0 ([Visual Studio](https://www.visualstudio.com/) 2017)
  - other compilers might work
- the [cmake](https://www.cmake.org/) build system
  - at least version 3.18 is recommended
  - at least version 3.1 is *required*

[Cargo feature]: https://doc.rust-lang.org/cargo/reference/manifest.html#the-features-section

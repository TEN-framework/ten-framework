//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

// Each file in the tests directory is a separate crate, and will be compiled as
// an executable. We need a all-in-one executable to run all the tests, and we
// will copy the all-in-one executable to the out directory. We use this main
// file to achieve this purpose.

fn main() {
    println!("Running integration tests of ten_rust...");
}

// Those following mods will be compiled in one executable.
mod test_case;

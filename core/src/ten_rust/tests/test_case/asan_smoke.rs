//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

const ENABLE_ASAN_SMOKE_ENV: &str = "TEN_RUST_ENABLE_ASAN_SMOKE";
const CHILD_ENV: &str = "TEN_RUST_ASAN_LEAK_CHILD";
const CHILD_TEST_NAME: &str = "test_case::asan_smoke::asan_lsan_intentional_leak_child";

#[test]
fn asan_lsan_detects_intentional_leak() {
    if std::env::var(ENABLE_ASAN_SMOKE_ENV).as_deref() != Ok("1") {
        return;
    }

    assert!(
        cfg!(all(target_os = "linux", target_arch = "x86_64")),
        "{ENABLE_ASAN_SMOKE_ENV}=1 is only supported for Linux x86_64 ASan builds"
    );

    let current_exe = std::env::current_exe().expect("Failed to get current test executable.");
    let output = std::process::Command::new(current_exe)
        .arg(CHILD_TEST_NAME)
        .arg("--exact")
        .arg("--nocapture")
        .env(CHILD_ENV, "1")
        .env("ASAN_OPTIONS", "detect_leaks=1:color=never:abort_on_error=0:exitcode=23")
        .output()
        .expect("Failed to run the ASan leak child test.");

    assert!(
        !output.status.success(),
        "The ASan leak child test unexpectedly succeeded. stdout:\n{}\nstderr:\n{}",
        String::from_utf8_lossy(&output.stdout),
        String::from_utf8_lossy(&output.stderr)
    );

    let stderr = String::from_utf8_lossy(&output.stderr);
    assert!(
        stderr.contains("LeakSanitizer") && stderr.contains("detected memory leaks"),
        "Expected LeakSanitizer to report a leak. status: {:?}\nstdout:\n{}\nstderr:\n{}",
        output.status,
        String::from_utf8_lossy(&output.stdout),
        stderr
    );
}

#[test]
fn asan_lsan_intentional_leak_child() {
    if std::env::var(CHILD_ENV).as_deref() != Ok("1") {
        return;
    }

    std::thread::spawn(|| unsafe {
        let ptr = libc::malloc(4096);
        assert!(!ptr.is_null());
        std::ptr::write_bytes(ptr, 0x5a, 4096);
        std::hint::black_box(ptr);
    })
    .join()
    .expect("The intentional leak thread panicked.");
}

//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

/// Print memory statistics based on the current memory allocator.
///
/// This function prints memory usage information for debugging purposes.
/// The implementation depends on which memory allocator is used.
///
/// # Arguments
///
/// * `context` - A string slice that provides context about when the statistics
///   are being printed
pub fn print_memory_stats(context: &str) {
    #[cfg(feature = "jemalloc")]
    {
        print_jemalloc_stats(context);
    }

    // Default case when no specific memory allocator feature is enabled
    #[cfg(not(feature = "jemalloc"))]
    {
        println!("\n=== Memory Statistics ({context}) ===");
        println!("No detailed memory statistics available - jemalloc not enabled");
        println!("=== End of Memory Statistics ===\n");
    }
}

#[cfg(feature = "jemalloc")]
fn print_jemalloc_stats(context: &str) {
    extern "C" fn write_cb(_cbopaque: *mut std::ffi::c_void, message: *const std::os::raw::c_char) {
        unsafe {
            if !message.is_null() {
                let c_str = std::ffi::CStr::from_ptr(message);
                if let Ok(s) = c_str.to_str() {
                    print!("{}", s);
                }
            }
        }
    }

    println!("\n=== Jemalloc Memory Statistics ({}) ===", context);
    unsafe {
        jemalloc_sys::malloc_stats_print(Some(write_cb), std::ptr::null_mut(), std::ptr::null());
    }
    println!("=== End of Jemalloc Memory Statistics ===\n");
}

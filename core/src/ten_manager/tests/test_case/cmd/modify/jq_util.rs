//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

#[cfg(test)]
mod tests {
    use ten_manager::cmd::cmd_modify::jq_util::*;

    #[test]
    fn test_path_to_jq_filter() {
        let result = path_to_jq_filter(&["a", "b", "c"]);
        assert_eq!(result, ".a.b.c");

        let result = path_to_jq_filter(&["a"]);
        assert_eq!(result, ".a");

        let result = path_to_jq_filter(&[]);
        assert_eq!(result, ".");
    }
}

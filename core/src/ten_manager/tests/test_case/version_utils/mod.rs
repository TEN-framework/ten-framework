//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
#[cfg(test)]
mod tests {
    use anyhow::Result;
    use semver::{Version, VersionReq};
    use ten_manager::version_utils::*;

    #[test]
    fn test_parse_version_rc() -> Result<()> {
        let input = "aaa@3.0.0-rc1";
        let (pkg, ver_req) = parse_pkg_name_version_req(input)?;
        assert_eq!(pkg, "aaa");

        let expected = VersionReq::parse("3.0.0-rc1")?;
        assert_eq!(ver_req, expected);

        Ok(())
    }

    #[test]
    fn test_pre_release_version_matches_1() {
        let version_str = "0.8.0-rc.1";
        let req_str = "^0.8.0-rc.1";

        let req = VersionReq::parse(req_str)
            .expect("Failed to parse version requirement");
        let version =
            Version::parse(version_str).expect("Failed to parse version");

        assert!(
            req.matches(&version),
            "Version {:?} does not match requirement {}",
            version,
            req_str
        );
    }

    #[test]
    fn test_pre_release_version_matches_2() {
        let version_str = "0.8.0-rc.1";
        let req_str = "=0.8.0-rc.1";

        let req = VersionReq::parse(req_str)
            .expect("Failed to parse version requirement");
        let version =
            Version::parse(version_str).expect("Failed to parse version");

        assert!(
            req.matches(&version),
            "Version {:?} does not match requirement {}",
            version,
            req_str
        );
    }
}

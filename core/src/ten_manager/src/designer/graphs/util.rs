//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use ten_rust::{
    base_dir_pkg_info::PkgsInfoInApp,
    pkg_info::{pkg_type::PkgType, PkgInfo},
};

/// Finds the app package in the list of packages
pub fn find_app_package(pkgs: &mut [PkgInfo]) -> Option<&mut PkgInfo> {
    pkgs.iter_mut()
        .find(|pkg| pkg.manifest.type_and_name.pkg_type == PkgType::App)
}

/// Finds the app package directly from PkgsInfoInApp.
pub fn find_app_package_from_base_dir(
    base_dir_pkg_info: &mut PkgsInfoInApp,
) -> Option<&mut PkgInfo> {
    base_dir_pkg_info.app_pkg_info.as_mut()
}

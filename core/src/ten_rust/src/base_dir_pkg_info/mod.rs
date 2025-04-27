//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use crate::pkg_info::{pkg_type::PkgType, PkgInfo};

#[derive(Debug, Clone, Default)]
pub struct PkgsInfoInApp {
    pub app_pkg_info: Option<PkgInfo>,
    pub extension_pkgs_info: Option<Vec<PkgInfo>>,
    pub protocol_pkgs_info: Option<Vec<PkgInfo>>,
    pub addon_loader_pkgs_info: Option<Vec<PkgInfo>>,
    pub system_pkgs_info: Option<Vec<PkgInfo>>,
}

impl PkgsInfoInApp {
    // Get a reference to the extension packages or an empty slice if none.
    pub fn get_extensions(&self) -> &[PkgInfo] {
        if let Some(extensions) = &self.extension_pkgs_info {
            extensions.as_slice()
        } else {
            &[]
        }
    }

    /// Combines all package types into a single vector.
    pub fn to_vec(&self) -> Vec<PkgInfo> {
        let mut all_pkgs = Vec::new();
        if let Some(app_info) = &self.app_pkg_info {
            all_pkgs.push(app_info.clone());
        }
        if let Some(ext_info) = &self.extension_pkgs_info {
            all_pkgs.extend(ext_info.clone());
        }
        if let Some(proto_info) = &self.protocol_pkgs_info {
            all_pkgs.extend(proto_info.clone());
        }
        if let Some(addon_info) = &self.addon_loader_pkgs_info {
            all_pkgs.extend(addon_info.clone());
        }
        if let Some(sys_info) = &self.system_pkgs_info {
            all_pkgs.extend(sys_info.clone());
        }
        all_pkgs
    }

    // Check if all fields are None or empty vectors.
    pub fn is_empty(&self) -> bool {
        self.app_pkg_info.is_none()
            && (self.extension_pkgs_info.is_none()
                || self.extension_pkgs_info.as_ref().unwrap().is_empty())
            && (self.protocol_pkgs_info.is_none()
                || self.protocol_pkgs_info.as_ref().unwrap().is_empty())
            && (self.addon_loader_pkgs_info.is_none()
                || self.addon_loader_pkgs_info.as_ref().unwrap().is_empty())
            && (self.system_pkgs_info.is_none()
                || self.system_pkgs_info.as_ref().unwrap().is_empty())
    }

    /// Find a package by its type and name.
    /// Returns None if no matching package is found.
    pub fn find_pkg_by_type_and_name(
        &self,
        pkg_type: PkgType,
        name: &str,
    ) -> Option<&PkgInfo> {
        if let Some(app_pkg) = &self.app_pkg_info {
            if app_pkg.manifest.type_and_name.pkg_type == pkg_type
                && app_pkg.manifest.type_and_name.name == name
            {
                return Some(app_pkg);
            }
        }

        if let Some(ext_pkgs) = &self.extension_pkgs_info {
            let found = ext_pkgs.iter().find(|pkg| {
                pkg.manifest.type_and_name.pkg_type == pkg_type
                    && pkg.manifest.type_and_name.name == name
            });
            if found.is_some() {
                return found;
            }
        }

        if let Some(proto_pkgs) = &self.protocol_pkgs_info {
            let found = proto_pkgs.iter().find(|pkg| {
                pkg.manifest.type_and_name.pkg_type == pkg_type
                    && pkg.manifest.type_and_name.name == name
            });
            if found.is_some() {
                return found;
            }
        }

        if let Some(addon_loader_pkgs) = &self.addon_loader_pkgs_info {
            let found = addon_loader_pkgs.iter().find(|pkg| {
                pkg.manifest.type_and_name.pkg_type == pkg_type
                    && pkg.manifest.type_and_name.name == name
            });
            if found.is_some() {
                return found;
            }
        }

        if let Some(system_pkgs) = &self.system_pkgs_info {
            let found = system_pkgs.iter().find(|pkg| {
                pkg.manifest.type_and_name.pkg_type == pkg_type
                    && pkg.manifest.type_and_name.name == name
            });
            if found.is_some() {
                return found;
            }
        }

        None
    }

    /// Returns the total number of packages in this PkgsInfoInApp.
    pub fn len(&self) -> usize {
        let mut count = 0;
        if self.app_pkg_info.is_some() {
            count += 1;
        }
        if let Some(extensions) = &self.extension_pkgs_info {
            count += extensions.len();
        }
        if let Some(protocols) = &self.protocol_pkgs_info {
            count += protocols.len();
        }
        if let Some(addon_loaders) = &self.addon_loader_pkgs_info {
            count += addon_loaders.len();
        }
        if let Some(systems) = &self.system_pkgs_info {
            count += systems.len();
        }
        count
    }
}

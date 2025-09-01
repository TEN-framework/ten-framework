//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use actix_web_actors::ws::WebsocketContext;
use ten_rust::pkg_info::manifest::support::ManifestSupport;

use super::{common::run_installation, WsBuiltinFunction};
use crate::cmd::cmd_install::LocalInstallMode;

impl WsBuiltinFunction {
    pub fn install_all(&mut self, base_dir: String, ctx: &mut WebsocketContext<WsBuiltinFunction>) {
        let install_command = crate::cmd::cmd_install::InstallCommand {
            package_type: None,
            package_name: None,
            support: ManifestSupport {
                os: None,
                arch: None,
            },
            local_install_mode: LocalInstallMode::Link,
            standalone: false,
            production: false,
            local_path: None,
            cwd: base_dir.clone(),
            max_latest_versions: crate::constants::DEFAULT_MAX_LATEST_VERSIONS_WHEN_INSTALL,
        };

        run_installation(
            self.tman_config.clone(),
            self.tman_storage_in_memory.clone(),
            install_command,
            ctx,
        );
    }
}

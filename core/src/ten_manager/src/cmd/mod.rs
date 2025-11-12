//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
pub mod cmd_check;
pub mod cmd_completion;
pub mod cmd_create;
pub mod cmd_delete;
pub mod cmd_designer;
pub mod cmd_fetch;
pub mod cmd_install;
pub mod cmd_modify;
pub mod cmd_package;
pub mod cmd_publish;
pub mod cmd_run;
pub mod cmd_uninstall;

use std::sync::Arc;

use anyhow::Result;

use super::home::config::TmanConfig;
use crate::{designer::storage::in_memory::TmanStorageInMemory, output::TmanOutput};

pub enum CommandData {
    Create(self::cmd_create::CreateCommand),
    Install(self::cmd_install::InstallCommand),
    Fetch(crate::cmd::cmd_fetch::FetchCommand),
    Uninstall(self::cmd_uninstall::UninstallCommand),
    Package(self::cmd_package::PackageCommand),
    Publish(self::cmd_publish::PublishCommand),
    Delete(self::cmd_delete::DeleteCommand),
    Designer(self::cmd_designer::DesignerCommand),
    Check(self::cmd_check::CheckCommandData),
    Modify(self::cmd_modify::ModifyCommandData),
    Run(self::cmd_run::RunCommand),
    Completion(self::cmd_completion::CompletionCommand),
}

pub async fn execute_cmd(
    tman_config: Arc<tokio::sync::RwLock<TmanConfig>>,
    tman_storage_in_memory: Arc<tokio::sync::RwLock<TmanStorageInMemory>>,
    command_data: CommandData,
    out: Arc<Box<dyn TmanOutput>>,
) -> Result<()> {
    match command_data {
        CommandData::Create(cmd) => {
            crate::cmd::cmd_create::execute_cmd(tman_config, tman_storage_in_memory, cmd, out).await
        }
        CommandData::Install(cmd) => {
            crate::cmd::cmd_install::execute_cmd(tman_config, tman_storage_in_memory, cmd, out)
                .await
        }
        CommandData::Fetch(cmd) => {
            crate::cmd::cmd_fetch::execute_cmd(tman_config, tman_storage_in_memory, cmd, out).await
        }
        CommandData::Uninstall(cmd) => {
            crate::cmd::cmd_uninstall::execute_cmd(tman_config, tman_storage_in_memory, cmd, out)
                .await
        }
        CommandData::Package(cmd) => {
            crate::cmd::cmd_package::execute_cmd(tman_config, tman_storage_in_memory, cmd, out)
                .await
        }
        CommandData::Publish(cmd) => {
            crate::cmd::cmd_publish::execute_cmd(tman_config, tman_storage_in_memory, cmd, out)
                .await
        }
        CommandData::Delete(cmd) => {
            crate::cmd::cmd_delete::execute_cmd(tman_config, tman_storage_in_memory, cmd, out).await
        }
        CommandData::Designer(cmd) => {
            crate::cmd::cmd_designer::execute_cmd(tman_config, tman_storage_in_memory, cmd, out)
                .await
        }
        CommandData::Check(cmd) => {
            crate::cmd::cmd_check::execute_cmd(tman_config, tman_storage_in_memory, cmd, out).await
        }
        CommandData::Modify(cmd) => {
            crate::cmd::cmd_modify::execute_cmd(tman_config, tman_storage_in_memory, cmd, out).await
        }
        CommandData::Run(cmd) => {
            crate::cmd::cmd_run::execute_cmd(tman_config, tman_storage_in_memory, cmd, out).await
        }
        CommandData::Completion(cmd) => {
            crate::cmd::cmd_completion::execute_cmd(tman_config, tman_storage_in_memory, cmd, out)
                .await
        }
    }
}

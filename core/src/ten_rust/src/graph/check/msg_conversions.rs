//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
use anyhow::Result;

use crate::graph::{connection::GraphMessageFlow, Graph};

impl Graph {
    /// result conversion should only be used for cmd out msg.
    fn check_msg_result_conversion_valid(&self) -> Result<()> {
        let Some(connections) = &self.connections else {
            return Ok(());
        };

        fn check_invalid_msg_conversion(
            msg_type: &str,
            msg_flows: &[GraphMessageFlow],
        ) -> Result<()> {
            for flow in msg_flows {
                if flow.dest.iter().any(|dest| {
                    dest.msg_conversion.as_ref().and_then(|conv| conv.result.as_ref()).is_some()
                }) {
                    return Err(anyhow::anyhow!(
                        "result conversion is not allowed for {} out msg",
                        msg_type
                    ));
                }
            }
            Ok(())
        }

        for connection in connections {
            if let Some(data) = &connection.data {
                check_invalid_msg_conversion("data", data)?;
            }

            if let Some(audio_frame) = &connection.audio_frame {
                check_invalid_msg_conversion("audio frame", audio_frame)?;
            }

            if let Some(video_frame) = &connection.video_frame {
                check_invalid_msg_conversion("video frame", video_frame)?;
            }
        }

        Ok(())
    }

    /// Validates that msg conversions are valid.
    ///
    /// # Returns
    /// - `Ok(())` if msg conversions are valid
    /// - `Err` with descriptive message if msg conversions are invalid
    pub fn check_msg_conversions(&self) -> Result<()> {
        self.check_msg_result_conversion_valid()?;

        Ok(())
    }
}

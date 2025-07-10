//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

// Refer to https://json-schema.org/learn/getting-started-step-by-step
export const PERSISTENT_SCHEMA = {
  $schema: "https://json-schema.org/draft/2020-12/schema",
  $id: "https://example.com/persistent.schema.json",
  title: "Persistent Schema",
  description: "A schema for persistent storage",
  type: "object",
  properties: {
    version: {
      type: "string",
      description: "Schema version",
    },
    recent_run_apps: {
      type: "array",
      items: {
        type: "object",
        properties: {
          base_dir: { type: "string", description: "Base Dir" },
          script_name: { type: "string", description: "Selected Script Name" },
          stdout_is_log: { type: "boolean", description: "Std Out Logs" },
          stderr_is_log: { type: "boolean", description: "Std Err Logs" },
          run_with_agent: { type: "boolean", description: "Run with Agent" },
        },
        required: [
          "base_dir",
          "script_name",
          "stdout_is_log",
          "stderr_is_log",
          "run_with_agent",
        ],
      },
    },
    graph_ui: {
      type: "object",
      patternProperties: {
        "^.*$": {
          type: "object",
          properties: {
            graph_geometry: {
              type: "object",
              properties: {
                nodes_geometry: {
                  type: "array",
                  items: {
                    type: "object",
                    properties: {
                      app: { type: "string", description: "App ID" },
                      extension: {
                        type: "string",
                        description: "Extension ID",
                      },
                      x: { type: "number", description: "X coordinate" },
                      y: { type: "number", description: "Y coordinate" },
                    },
                    required: ["extension", "x", "y"],
                  },
                },
              },
              required: ["nodes_geometry"],
            },
          },
          required: ["graph_geometry"],
        },
      },
    },
  },
  required: ["version"],
};

export const PERSISTENT_DEFAULTS = {
  version: "0.0.1",
  recent_run_apps: [],
  graph_ui: {},
};

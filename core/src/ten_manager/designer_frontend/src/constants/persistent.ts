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
  },
  required: ["version"],
};

export const PERSISTENT_DEFAULTS = {
  version: "0.0.1",
};

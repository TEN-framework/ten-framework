//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import { createRequire } from "node:module";

let addon = {};

try {
  // Even in Node.js v23.11, using ESM modules to load native addons is not
  // supported. Refer to the following note in the Node.js documentation:
  //
  // **No Addon Loading**
  // Addons are not currently supported with ES module imports.
  // They can instead be loaded with `module.createRequire()` or
  // `process.dlopen`.
  const require = createRequire(import.meta.url);
  addon = require("libten_runtime_nodejs");
} catch (e) {
  console.error(`Failed to load libten_runtime_nodejs module: ${e}`);
}

export default addon as unknown as typeof import("libten_runtime_nodejs");

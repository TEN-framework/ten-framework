//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import js from "@eslint/js";
import tseslint from "typescript-eslint";
import prettierPlugin from "eslint-plugin-prettier";
import prettierConfig from "eslint-config-prettier";

export default await tseslint.config({
  files: ["**/*.ts", "**/*.tsx"],
  languageOptions: {
    parserOptions: {
      ecmaVersion: 2022,
      sourceType: "module",
    },
  },
  rules: {
    "@typescript-eslint/no-unused-vars": "warn",
    "prettier/prettier": "warn",
    "max-len": ["error", { code: 80 }],
  },
  plugins: {
    "@typescript-eslint": tseslint.plugin,
    prettier: prettierPlugin,
  },
  extends: [
    js.configs.recommended,
    ...tseslint.configs.recommended,
    prettierConfig,
  ],
});

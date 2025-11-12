//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import z from "zod";

import type { TenLocalStorePackageSchema } from "@/types/extension";

export interface ISetBaseDirResponse {
  app_uri: string | null;
}

export interface IGetBaseDirResponse {
  base_dir: string | null;
}

export interface IApp {
  base_dir: string;
  app_uri: string | null;
}

export interface IGetAppsResponse {
  app_info: IApp[];
}

export interface IRunAppParams {
  base_dir: string;
  script_name: string;
  stdout_is_log: boolean;
  stderr_is_log: boolean;
  run_with_agent: boolean;
}

// eslint-disable-next-line @typescript-eslint/no-empty-object-type
export interface IExtensionAddon
  extends z.infer<typeof TenLocalStorePackageSchema> {}

export enum EWSMessageType {
  STANDARD_OUTPUT = "stdout",
  STANDARD_ERROR = "stderr",
  STANDARD_OUTPUT_LOG = "stdout_log",
  STANDARD_ERROR_LOG = "stderr_log",
  EXIT = "exit",
  NORMAL_LINE = "normal_line",
}

export enum EPreferencesTabs {
  GENERAL = "general",
  LOG = "log",
}

export const PREFERENCES_SCHEMA_LOG = z.object({
  logviewer_line_size: z.number().min(1).default(1000),
});

export const PREFERENCES_SCHEMA_TRULIENCE = z.object({
  trulience: z.object({
    enabled: z.boolean().default(false),
    trulienceAvatarId: z.string().default(""),
    trulienceAvatarToken: z.string().default(""),
    trulienceSdkUrl: z
      .string()
      .default("https://trulience.com/sdk/trulience.sdk.js"),
    trulienceAnimationUrl: z.string().default("https://trulience.com"),
  }),
});

export enum EPreferencesLocale {
  EN_US = "en-US",
  ZH_CN = "zh-CN",
  ZH_TW = "zh-TW",
  JA_JP = "ja-JP",
}
// tmp, need to move to scripts and retrieve from the backend.
export const PREFERENCES_SCHEMA = z.object({
  locale: z.nativeEnum(EPreferencesLocale).default(EPreferencesLocale.EN_US),
  ...PREFERENCES_SCHEMA_LOG.shape,
  ...PREFERENCES_SCHEMA_TRULIENCE.shape,
});

export enum ETemplateLanguage {
  CPP = "cpp",
  GOLANG = "golang",
  PYTHON = "python",
  TYPESCRIPT = "typescript",
}

export enum ETemplateType {
  // INVALID = "invalid",
  // SYSTEM = "system",
  APP = "app",
  // EXTENSION = "extension",
  // PROTOCOL = "protocol",
  // ADDON_LOADER = "addon_loader",
}

export enum ETenLogLevel {
  INVALID = "INVALID",
  VERBOSE = "VERBOSE",
  DEBUG = "DEBUG",
  INFO = "INFO",
  WARN = "WARN",
  ERROR = "ERROR",
  FATAL = "FATAL",
  MANDATORY = "MANDATORY",
}

export const TemplatePkgsReqSchema = z.object({
  pkg_type: z.nativeEnum(ETemplateType),
  language: z.nativeEnum(ETemplateLanguage),
});

export const AppCreateReqSchema = z.object({
  base_dir: z.string().min(1),
  app_name: z.string().min(1),
  template_name: z.string().min(1),
  template_version: z.string().optional(),
});

export const LogLineMetadataSchema = z.object({
  graph_id: z.string().optional(),
  graph_name: z.string().optional(),
  extension: z.string().optional(),
  log_level: z.nativeEnum(ETenLogLevel).optional(),
});

export const LogLineInfoSchema = z.object({
  line: z.string(),
  metadata: LogLineMetadataSchema.optional(),
  type: z.nativeEnum(EWSMessageType),
});

export const LogSchema = z.object({
  type: z.nativeEnum(EWSMessageType),
  data: LogLineInfoSchema,
  code: z.number().optional(),
  error_message: z.string().optional(),
  status: z.string().optional(),
  message: z.string().optional(),
});

export const LegacyLogSchema = LogSchema.extend({
  data: z.string(),
});

export enum ELocalAppStatus {
  LOADED = "loaded",
  RUNNING = "running",
}

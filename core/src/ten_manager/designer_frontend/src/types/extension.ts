//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import z from "zod";

export enum ETenPackageType {
  Invalid = "invalid",
  System = "system",
  App = "app",
  Extension = "extension",
  Protocol = "protocol",
  AddonLoader = "addon_loader",
}

export const TenPackageBaseSchema = z.object({
  type: z.nativeEnum(ETenPackageType),
  name: z.string(),
});

export const TenLocalStorePackageSchema = TenPackageBaseSchema.extend({
  url: z.string(),
  api: z.unknown().optional(),
});

export const TenCloudStorePackageSchema = TenPackageBaseSchema.extend({
  version: z.string(),
  hash: z.string(),
  dependencies: z.array(
    z.object({
      name: z.string(),
      type: z.nativeEnum(ETenPackageType),
      version: z.string(),
    })
  ),
  downloadUrl: z.string(),
  supports: z
    .array(
      z.object({
        os: z.string(),
        arch: z.string(),
      })
    )
    .optional(),
});

export enum EPackageSource {
  Local = "local", // means the package is for local only
  Default = "default", // means the package is from the cloud store
}

// eslint-disable-next-line @typescript-eslint/no-empty-object-type
export interface IListTenCloudStorePackage
  extends z.infer<typeof TenCloudStorePackageSchema> {}

// eslint-disable-next-line @typescript-eslint/no-empty-object-type
export interface IListTenLocalStorePackage
  extends z.infer<typeof TenLocalStorePackageSchema> {}

export interface ITenPackageLocal extends IListTenLocalStorePackage {
  isInstalled: true;
  _type: EPackageSource.Local;
}

export interface ITenPackage extends IListTenCloudStorePackage {
  isInstalled?: boolean;
  _type: EPackageSource.Default;
}

export const ExtensionPropertySchema = z.record(
  z.string(),
  z.object({
    type: z.string(),
    required: z.any().optional(),
    items: z.any().optional(),
    properties: z.any().optional(),
  })
);

export const ExtensionConnectionItemSchema = z.object({
  name: z.string(),
  property: z.record(z.string(), z.any()).optional(),
  required: z.array(z.string()).optional(),
  result: z.any().optional(),
});

export const ExtensionSchema = z.object({
  property: ExtensionPropertySchema.optional(),
  required: z.array(z.string()).optional(),
  cmd_in: z.array(ExtensionConnectionItemSchema).optional(),
  cmd_out: z.array(ExtensionConnectionItemSchema).optional(),
  data_in: z.array(ExtensionConnectionItemSchema).optional(),
  data_out: z.array(ExtensionConnectionItemSchema).optional(),
  audio_frame_in: z.array(ExtensionConnectionItemSchema).optional(),
  audio_frame_out: z.array(ExtensionConnectionItemSchema).optional(),
  video_frame_in: z.array(ExtensionConnectionItemSchema).optional(),
  video_frame_out: z.array(ExtensionConnectionItemSchema).optional(),
});

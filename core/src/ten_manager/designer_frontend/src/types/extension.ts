//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import {
  AsteriskIcon,
  HandshakeIcon,
  LayoutGridIcon,
  type LucideIcon,
  MonitorCogIcon,
  PuzzleIcon,
  // TicketXIcon
} from "lucide-react";
import z from "zod";

export enum ETenPackageType {
  System = "system",
  App = "app",
  Extension = "extension",
  Protocol = "protocol",
  AddonLoader = "addon_loader",
}

export const TenPackageTypeMappings: Record<
  ETenPackageType,
  {
    id: ETenPackageType;
    transKey: string;
    icon: LucideIcon;
  }
> = {
  // [ETenPackageType.Invalid]: {
  //   id: ETenPackageType.Invalid,
  //   transKey: 'extensionStore.packageType.invalid',
  //   icon: TicketXIcon
  // },
  [ETenPackageType.System]: {
    id: ETenPackageType.System,
    transKey: "extensionStore.packageType.system",
    icon: MonitorCogIcon,
  },
  [ETenPackageType.App]: {
    id: ETenPackageType.App,
    transKey: "extensionStore.packageType.app",
    icon: LayoutGridIcon,
  },
  [ETenPackageType.Extension]: {
    id: ETenPackageType.Extension,
    transKey: "extensionStore.packageType.extension",
    icon: PuzzleIcon,
  },
  [ETenPackageType.Protocol]: {
    id: ETenPackageType.Protocol,
    transKey: "extensionStore.packageType.protocol",
    icon: HandshakeIcon,
  },
  [ETenPackageType.AddonLoader]: {
    id: ETenPackageType.AddonLoader,
    transKey: "extensionStore.packageType.addonLoader",
    icon: AsteriskIcon,
  },
};

export const TenPackageBaseSchema = z.object({
  type: z.nativeEnum(ETenPackageType),
  name: z.string(),
});

export const TenLocalStorePackageSchema = TenPackageBaseSchema.extend({
  url: z.string(),
  api: z.unknown().optional(),
});

export const TenCloudStorePackageSchemaI18nField = z.object({
  locales: z.record(
    z.string(),
    z.object({
      content: z.string(),
    })
  ),
});

export const TenCloudStorePackageSchema = TenPackageBaseSchema.extend({
  version: z.string(),
  hash: z.string(),
  dependencies: z
    .array(
      z.object({
        name: z.string(),
        type: z.nativeEnum(ETenPackageType),
        version: z.string(),
      })
    )
    .optional(),
  downloadUrl: z.string(),
  supports: z
    .array(
      z.object({
        os: z.string(),
        arch: z.string(),
      })
    )
    .optional(),
  tags: z.array(z.string()).optional(),
  display_name: TenCloudStorePackageSchemaI18nField.optional(),
  description: TenCloudStorePackageSchemaI18nField.optional(),
  readme: TenCloudStorePackageSchemaI18nField.optional(),
});

export const TenPackageQueryAtomicFilterSchema = z.object({
  field: z.string(),
  operator: z.string(),
  value: z.string(),
});
export const TenPackageQueryLogicFilterSchema = z.object({
  and: z.array(TenPackageQueryAtomicFilterSchema).optional(),
  or: z.array(TenPackageQueryAtomicFilterSchema).optional(),
});

export const TenPackageQueryFilterSchema = TenPackageQueryAtomicFilterSchema.or(
  TenPackageQueryLogicFilterSchema
);

export const TenPackageQueryOptionsSchema = z.object({
  page: z.number().min(1).optional(),
  page_size: z.number().min(1).optional(),
  sort_by: z.string().optional(),
  sort_order: z.string().optional(),
  scope: z.string().optional(),
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
  property: z
    .object({
      properties: ExtensionPropertySchema.optional(),
      required: z.array(z.string()).optional(),
    })
    .optional(),
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

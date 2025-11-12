//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import { cva } from "class-variance-authority";
import { ETenPackageType } from "@/types/extension";

export const extensionListItemVariants = cva("", {
  variants: {
    text: {
      [ETenPackageType.AddonLoader]: "text-blue-800 dark:text-blue-200",
      [ETenPackageType.App]: "text-green-800 dark:text-green-200",
      [ETenPackageType.Extension]: "text-purple-800 dark:text-purple-200",
      [ETenPackageType.Protocol]: "text-orange-800 dark:text-orange-200",
      [ETenPackageType.System]: "text-red-800 dark:text-red-200",
    },
    bg: {
      [ETenPackageType.AddonLoader]: "bg-blue-100 dark:bg-blue-900",
      [ETenPackageType.App]: "bg-green-100 dark:bg-green-900",
      [ETenPackageType.Extension]: "bg-purple-100 dark:bg-purple-900",
      [ETenPackageType.Protocol]: "bg-orange-100 dark:bg-orange-900",
      [ETenPackageType.System]: "bg-red-100 dark:bg-red-900",
    },
  },
});

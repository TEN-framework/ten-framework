//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

import {
  type LucideIcon,
  NetworkIcon,
  PuzzleIcon,
  RegexIcon,
} from "lucide-react";
import { ECustomNodeType } from "@/types/flow";

export const NODE_CONFIG_MAPPING: {
  [key in ECustomNodeType]: {
    Icon: LucideIcon;
  };
} = {
  [ECustomNodeType.GRAPH]: {
    Icon: NetworkIcon,
  },
  [ECustomNodeType.EXTENSION]: {
    Icon: PuzzleIcon,
  },
  [ECustomNodeType.SELECTOR]: {
    Icon: RegexIcon,
  },
  [ECustomNodeType.SUB_GRAPH]: {
    Icon: NetworkIcon,
  },
};

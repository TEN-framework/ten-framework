//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

/**
 * Configuration interface for log options, including skip parameter for
 * extensibility
 */
export interface LogOption {
  /**
   * Number of stack frames to skip when determining caller information
   */
  skip: number;
}

/**
 * Default log option instance with skip=2
 */
export const DefaultLogOption: LogOption = {
  skip: 2,
};

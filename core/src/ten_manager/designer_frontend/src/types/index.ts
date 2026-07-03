//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
export interface SuccessResponse<T> {
  status: "ok";
  data: T;
  meta?: unknown;
}

export interface ErrorResponse {
  status: "fail";
  message: string;
  error?: InnerError;
}

export interface InnerError {
  // Note: The value of 'type' must adhere to the 'error_type' in rust.
  type: string;

  code?: string;
  message: string;
}

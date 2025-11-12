//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
export interface IGetEnvVarResponse {
  value: string | null;
}

export interface IRTCEnvVar {
  appId: string;
  appCert: string | null;
}

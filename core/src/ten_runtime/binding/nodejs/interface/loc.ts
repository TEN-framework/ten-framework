//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
export class Loc {
  constructor(
    public readonly appUri: string | undefined,
    public readonly graphId: string | undefined,
    public readonly extensionName: string | undefined,
  ) {}
}

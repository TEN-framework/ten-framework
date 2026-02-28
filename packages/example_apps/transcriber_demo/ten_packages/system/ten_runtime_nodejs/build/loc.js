//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
export class Loc {
    appUri;
    graphId;
    extensionName;
    constructor(init = {}) {
        // Copy the fields from init if present; otherwise, keep them as undefined.
        Object.assign(this, init);
    }
}
//# sourceMappingURL=loc.js.map
//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//

/**
 * Configuration options for sending messages.
 */
export interface SendOptions {
    /**
     * Whether to wait for the send result. If false, the send operation will
     * not wait for completion and will not return error information, thus
     * avoiding the creation of additional async tasks.
     * Defaults to false for optimal performance.
     */
    waitForResult?: boolean;
}

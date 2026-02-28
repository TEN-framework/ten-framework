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

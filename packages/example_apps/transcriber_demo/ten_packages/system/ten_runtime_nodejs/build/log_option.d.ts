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
export declare const DefaultLogOption: LogOption;

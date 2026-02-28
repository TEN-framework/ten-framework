import { Cmd } from "./cmd.js";
export declare class StartGraphCmd extends Cmd {
    private constructor();
    static Create(): StartGraphCmd;
    /**
     * Set the predefined graph name for this start graph command.
     */
    setPredefinedGraphName(predefinedGraphName: string): void;
    /**
     * Set the graph configuration from a JSON string.
     */
    setGraphFromJSON(jsonStr: string): void;
}

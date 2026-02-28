import { Cmd } from "./cmd.js";
export declare class StopGraphCmd extends Cmd {
    private constructor();
    static Create(): StopGraphCmd;
    /**
     * Set the graph ID for this stop graph command.
     */
    setGraphId(graphId: string): void;
}

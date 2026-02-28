import { Cmd } from "./cmd.js";
export declare class TriggerLifeCycleCmd extends Cmd {
    private constructor();
    static Create(): TriggerLifeCycleCmd;
    /**
     * Set the stage for this trigger life cycle command.
     * Valid values are "start" and "stop".
     */
    setStage(stage: string): void;
}

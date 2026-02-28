import { Msg } from "../msg.js";
import type { Cmd } from "./cmd.js";
export declare enum StatusCode {
    OK = 0,
    ERROR = 1
}
export declare class CmdResult extends Msg {
    private constructor();
    static Create(statusCode: StatusCode, targetCmd: Cmd): CmdResult;
    getStatusCode(): StatusCode;
    setFinal(isFinal: boolean): void;
    isFinal(): boolean;
    isCompleted(): boolean;
}

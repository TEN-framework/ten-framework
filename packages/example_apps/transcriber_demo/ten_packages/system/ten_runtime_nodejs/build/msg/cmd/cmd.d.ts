import { Msg } from "../msg.js";
export declare class Cmd extends Msg {
    protected constructor(name: string, createShellOnly: boolean);
    static Create(name: string): Cmd;
}

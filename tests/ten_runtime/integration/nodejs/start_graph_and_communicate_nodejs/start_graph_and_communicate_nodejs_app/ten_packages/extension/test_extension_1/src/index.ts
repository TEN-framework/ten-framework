//
// Copyright © 2025 Agora
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0, with certain conditions.
// Refer to the "LICENSE" file in the root directory for more information.
//
import {
    Extension,
    TenEnv,
    Cmd,
    CmdResult,
    StatusCode,
    Data,
    Addon,
    RegisterAddonAsExtension,
    LogLevel,
    StartGraphCmd,
    StopGraphCmd,
} from "ten-runtime-nodejs";

class TestExtension1 extends Extension {
    private receivedDataFromNewGraph = false;
    private newGraphId = "";
    private testCmd: Cmd | null = null;

    constructor(name: string) {
        super(name);
    }

    async onStart(tenEnv: TenEnv): Promise<void> {
        // Start a new graph
        const startGraphCmd = StartGraphCmd.Create();

        // The destination of the 'start_graph' command is the current app,
        // using "" to represent current app.
        startGraphCmd.setDests([{ appUri: "" }]);

        // The new graph contains 3 extensions.
        const graphJSON = {
            nodes: [
                {
                    type: "extension",
                    name: "test_extension_2",
                    addon: "test_extension_2",
                },
                {
                    type: "extension",
                    name: "test_extension_3",
                    addon: "test_extension_3",
                },
                {
                    type: "extension",
                    name: "test_extension_4",
                    addon: "test_extension_4",
                },
            ],
            connections: [
                {
                    extension: "test_extension_2",
                    cmd: [
                        {
                            name: "A",
                            dest: [
                                {
                                    extension: "test_extension_3",
                                    msg_conversion: {
                                        keep_original: true,
                                        type: "per_property",
                                        rules: [
                                            {
                                                path: "ten.name",
                                                conversion_mode: "fixed_value",
                                                value: "B",
                                            },
                                        ],
                                    },
                                },
                                {
                                    extension: "test_extension_4",
                                },
                            ],
                        },
                    ],
                },
            ],
            exposed_messages: [
                {
                    type: "cmd_in",
                    name: "start",
                    extension: "test_extension_2",
                },
                {
                    type: "data_out",
                    name: "data_from_new_graph",
                    extension: "test_extension_4",
                },
            ],
        };

        startGraphCmd.setGraphFromJSON(JSON.stringify(graphJSON));

        const [cmdResult, error] = await tenEnv.sendCmd(startGraphCmd);
        if (error) {
            tenEnv.log(LogLevel.ERROR, `Start graph failed: ${error}`);
            return;
        }

        if (!cmdResult) {
            tenEnv.log(LogLevel.ERROR, "Start graph cmd_result is None");
            return;
        }

        const [newGraphId] = cmdResult.getPropertyString("graph_id");
        this.newGraphId = newGraphId;
        tenEnv.logInfo(`new_graph_id: ${newGraphId}`);

        // Send a 'start' command to test_extension_2
        const cmdStart = Cmd.Create("start");
        cmdStart.setDests([{ appUri: "", graphId: newGraphId }]);

        const [result, startError] = await tenEnv.sendCmd(cmdStart);
        if (startError) {
            tenEnv.log(LogLevel.ERROR, `Start command failed: ${startError}`);
            return;
        }

        if (!result) {
            tenEnv.log(LogLevel.ERROR, "Start command result is None");
            return;
        }
    }

    async onStop(tenEnv: TenEnv): Promise<void> {
        tenEnv.logInfo("on_stop");

        // Stop the started graph
        const stopGraphCmd = StopGraphCmd.Create();
        stopGraphCmd.setDests([{ appUri: "" }]);
        stopGraphCmd.setGraphId(this.newGraphId);

        const [result, error] = await tenEnv.sendCmd(stopGraphCmd);
        if (error) {
            tenEnv.log(LogLevel.ERROR, `Stop graph failed: ${error}`);
            return;
        }

        if (!result) {
            tenEnv.log(LogLevel.ERROR, "Stop graph result is None");
            return;
        }
    }

    async onCmd(tenEnv: TenEnv, cmd: Cmd): Promise<void> {
        const cmdName = cmd.getName();

        if (cmdName === "test") {
            this.testCmd = cmd;

            if (this.receivedDataFromNewGraph) {
                // Send the response to the client.
                await this.replyToClient(tenEnv);
            }
        } else {
            tenEnv.log(
                LogLevel.ERROR,
                `Should not happen - unknown command: ${cmdName}`,
            );
        }
    }

    async onData(tenEnv: TenEnv, data: Data): Promise<void> {
        const dataName = data.getName();

        if (dataName === "data_from_new_graph") {
            this.receivedDataFromNewGraph = true;

            if (this.testCmd !== null) {
                await this.replyToClient(tenEnv);
            }
        } else {
            tenEnv.log(LogLevel.ERROR, `Should not happen - unknown data: ${dataName}`);
        }
    }

    private async replyToClient(tenEnv: TenEnv): Promise<void> {
        console.log("reply to client");

        if (!this.testCmd) {
            throw new Error("testCmd is null");
        }

        const cmdResult = CmdResult.Create(StatusCode.OK, this.testCmd);

        const detail = { id: 1, name: "a" };
        cmdResult.setPropertyString("detail", JSON.stringify(detail));

        await tenEnv.returnResult(cmdResult);
    }
}

@RegisterAddonAsExtension("test_extension_1")
class TestExtension1Addon extends Addon {
    async onCreateInstance(
        _tenEnv: TenEnv,
        instanceName: string,
    ): Promise<Extension> {
        return new TestExtension1(instanceName);
    }
}

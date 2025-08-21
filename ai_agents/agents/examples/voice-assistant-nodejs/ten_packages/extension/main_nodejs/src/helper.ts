//
// This file is part of TEN Framework, an open source project.
// Licensed under the Apache License, Version 2.0.
// See the LICENSE file for more information.
//
import {
    TenEnv,
    Cmd,
    CmdResult,
    Data,
    // Loc,
    TenError,
} from "ten-runtime-nodejs";

/**
 * Check if a character is punctuation (Chinese and English).
 */
export function isPunctuation(char: string): boolean {
    return [",", "，", ".", "。", "?", "？", "!", "！"].includes(char);
}

/**
 * Parse sentences from content based on punctuation.
 * It will return complete sentences and the remaining incomplete fragment.
 */
export function parseSentences(
    sentenceFragment: string,
    content: string
): [string[], string] {
    const sentences: string[] = [];
    let currentSentence = sentenceFragment;

    for (const char of content) {
        currentSentence += char;
        if (isPunctuation(char)) {
            const strippedSentence = currentSentence;
            if ([...strippedSentence].some((c) => /[0-9A-Za-z]/.test(c))) {
                sentences.push(strippedSentence);
            }
            currentSentence = "";
        }
    }

    const remain = currentSentence; // Remaining fragment
    return [sentences, remain];
}

/**
 * Send a command with optional payload.
 * Shortcut for intra-graph communication.
 */
export async function sendCmd(
    tenEnv: TenEnv,
    cmdName: string,
    dest: string,
    payload?: any
): Promise<[CmdResult | undefined, TenError | undefined]> {
    const cmd = Cmd.Create(cmdName);
    cmd.setDests([{
        appUri: "",
        graphId: "",
        extensionName: dest
    }]);

    if (payload !== undefined) {
        cmd.setPropertyFromJson("", JSON.stringify(payload));
    }
    tenEnv.logDebug(`sendCmd: cmd_name ${cmdName}, dest ${dest}`);

    return await tenEnv.sendCmd(cmd);
}

/**
 * Send a command with optional payload and stream results back.
 */
export async function* sendCmdEx(
    tenEnv: TenEnv,
    cmdName: string,
    dest: string,
    payload?: any
): AsyncGenerator<[CmdResult | undefined, TenError | undefined]> {
    const cmd = Cmd.Create(cmdName);
    cmd.setDests([{
        appUri: "",
        graphId: "",
        extensionName: dest
    }]);

    if (payload !== undefined) {
        cmd.setPropertyFromJson("", JSON.stringify(payload));
    }
    tenEnv.logDebug(`sendCmdEx: cmd_name ${cmdName}, dest ${dest}`);

    // for await (const [cmdResult, tenError] of tenEnv.sendCmdEx(cmd)) {
    //     if (cmdResult) {
    //         tenEnv.logDebug(`sendCmdEx: cmd_result ${cmdResult}`);
    //     }
    //     yield [cmdResult, tenError];
    // }
}

/**
 * Send data with optional payload.
 * Shortcut for intra-graph communication.
 */
export async function sendData(
    tenEnv: TenEnv,
    dataName: string,
    dest: string,
    payload?: any
): Promise<TenError | undefined> {
    const data = Data.Create(dataName);
    data.setDests([{
        appUri: "",
        graphId: "",
        extensionName: dest
    }]);

    if (payload !== undefined) {
        data.setPropertyFromJson("", JSON.stringify(payload));
    }
    tenEnv.logInfo(`sendData: data_name ${dataName}, dest ${dest}`);

    return await tenEnv.sendData(data);
}

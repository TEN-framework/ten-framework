import ten_addon from "./ten_addon.js";
import { LogLevel } from "./log_level.js";
import { TenError, TenErrorCode } from "./error.js";
import { Value } from "./value.js";
import { DefaultLogOption } from "./log_option.js";
import { serializeToBuffer } from "./value_buffer.js";
export class TenEnvTester {
    async sendCmd(cmd) {
        return new Promise((resolve) => {
            const err = ten_addon.ten_nodejs_ten_env_tester_send_cmd(this, cmd, async (cmdResult, error) => {
                resolve([cmdResult, error]);
            });
            if (err) {
                resolve([undefined, err]);
            }
        });
    }
    async sendData(data, options) {
        // If options is undefined or doesn't wait for result, use fire-and-forget
        // mode.
        if (!options || !options.waitForResult) {
            return ten_addon.ten_nodejs_ten_env_tester_send_data(this, data, undefined);
        }
        // If care about result, use the original async waiting mode.
        return new Promise((resolve) => {
            const err = ten_addon.ten_nodejs_ten_env_tester_send_data(this, data, async (error) => {
                resolve(error);
            });
            if (err) {
                resolve(err);
            }
        });
    }
    async sendVideoFrame(videoFrame, options) {
        // If options is undefined or doesn't wait for result, use fire-and-forget
        // mode.
        if (!options || !options.waitForResult) {
            return ten_addon.ten_nodejs_ten_env_tester_send_video_frame(this, videoFrame, undefined);
        }
        // If care about result, use the original async waiting mode.
        return new Promise((resolve) => {
            const err = ten_addon.ten_nodejs_ten_env_tester_send_video_frame(this, videoFrame, async (error) => {
                resolve(error);
            });
            if (err) {
                resolve(err);
            }
        });
    }
    async sendAudioFrame(audioFrame, options) {
        // If options is undefined or doesn't wait for result, use fire-and-forget
        // mode.
        if (!options || !options.waitForResult) {
            return ten_addon.ten_nodejs_ten_env_tester_send_audio_frame(this, audioFrame, undefined);
        }
        // If care about result, use the original async waiting mode.
        return new Promise((resolve) => {
            const err = ten_addon.ten_nodejs_ten_env_tester_send_audio_frame(this, audioFrame, async (error) => {
                resolve(error);
            });
            if (err) {
                resolve(err);
            }
        });
    }
    async returnResult(cmdResult, options) {
        // If options is undefined or doesn't wait for result, use fire-and-forget
        // mode.
        if (!options || !options.waitForResult) {
            return ten_addon.ten_nodejs_ten_env_tester_return_result(this, cmdResult, undefined);
        }
        // If care about result, use the original async waiting mode.
        return new Promise((resolve) => {
            const err = ten_addon.ten_nodejs_ten_env_tester_return_result(this, cmdResult, async (error) => {
                resolve(error);
            });
            if (err) {
                resolve(err);
            }
        });
    }
    stopTest(result = undefined) {
        if (result) {
            return ten_addon.ten_nodejs_ten_env_tester_stop_test(this, result.errorCode, result.errorMessage);
        }
        return ten_addon.ten_nodejs_ten_env_tester_stop_test(this, 0, "");
    }
    logDebug(message, category = undefined, fields = undefined, option = DefaultLogOption) {
        return this.log_internal(LogLevel.DEBUG, message, category, fields, option);
    }
    logInfo(message, category = undefined, fields = undefined, option = DefaultLogOption) {
        return this.log_internal(LogLevel.INFO, message, category, fields, option);
    }
    logWarn(message, category = undefined, fields = undefined, option = DefaultLogOption) {
        return this.log_internal(LogLevel.WARN, message, category, fields, option);
    }
    logError(message, category = undefined, fields = undefined, option = DefaultLogOption) {
        return this.log_internal(LogLevel.ERROR, message, category, fields, option);
    }
    log(level, message, category = undefined, fields = undefined, option = DefaultLogOption) {
        return this.log_internal(level, message, category, fields, option);
    }
    log_internal(level, message, category, fields, option) {
        // Convert fields to Value if it's a plain object
        let fieldsValue = undefined;
        if (fields !== undefined) {
            try {
                if (fields instanceof Value) {
                    // fields is already a Value object
                    fieldsValue = fields;
                }
                else {
                    // fields is a plain object, convert it to Value
                    fieldsValue = Value.fromNative(fields);
                }
            }
            catch (e) {
                return new TenError(TenErrorCode.ErrorCodeGeneric, `failed to convert fields: ${e instanceof Error ? e.message : String(e)}`);
            }
        }
        // Serialize fields Value to buffer if provided
        let fieldsBuf = undefined;
        if (fieldsValue !== undefined) {
            try {
                fieldsBuf = serializeToBuffer(fieldsValue);
            }
            catch (e) {
                return new TenError(TenErrorCode.ErrorCodeGeneric, `failed to serialize fields: ${e instanceof Error ? e.message : String(e)}`);
            }
        }
        const _prepareStackTrace = Error.prepareStackTrace;
        Error.prepareStackTrace = (_, stack) => stack;
        const stack_ = new Error().stack;
        const stack = stack_.slice(1);
        Error.prepareStackTrace = _prepareStackTrace;
        const skipIndex = Math.min(option.skip - 1, stack.length - 1);
        const _callerFile = stack[skipIndex].getFileName();
        const _callerLine = stack[skipIndex].getLineNumber();
        const _callerFunction = stack[skipIndex].getFunctionName();
        const callerFile = _callerFile ? _callerFile : "unknown";
        const callerLine = _callerLine ? _callerLine : 0;
        const callerFunction = _callerFunction ? _callerFunction : "anonymous";
        return ten_addon.ten_nodejs_ten_env_tester_log_internal(this, level, callerFunction, callerFile, callerLine, category, message, fieldsBuf);
    }
}
ten_addon.ten_nodejs_ten_env_tester_register_class(TenEnvTester);
//# sourceMappingURL=env_tester.js.map
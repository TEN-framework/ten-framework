import { TenError, TenErrorCode } from "./error.js";
import ten_addon from "./ten_addon.js";
import { LogLevel } from "./log_level.js";
import { Value } from "./value.js";
import { DefaultLogOption } from "./log_option.js";
import { serializeToBuffer } from "./value_buffer.js";
export class TenEnv {
    async sendCmd(cmd) {
        return new Promise((resolve) => {
            const err = ten_addon.ten_nodejs_ten_env_send_cmd(this, cmd, async (cmdResult, error) => {
                resolve([cmdResult, error]);
            }, false);
            if (err) {
                resolve([undefined, err]);
            }
        });
    }
    async *sendCmdEx(cmd) {
        let resolvePromise;
        let promise = new Promise((resolve) => {
            resolvePromise = resolve;
        });
        const err = ten_addon.ten_nodejs_ten_env_send_cmd(this, cmd, async (cmdResult, error) => {
            resolvePromise?.([cmdResult, error]);
            promise = new Promise((resolve) => {
                resolvePromise = resolve;
            });
        }, true);
        if (err) {
            yield [undefined, err];
            return;
        }
        while (true) {
            const [result, error] = await promise;
            yield [result, error];
            if (error !== undefined) {
                break;
            }
            if (result?.isCompleted()) {
                break;
            }
        }
    }
    async sendData(data, options) {
        // If options is undefined or doesn't wait for result, use fire-and-forget
        // mode.
        if (!options || !options.waitForResult) {
            return ten_addon.ten_nodejs_ten_env_send_data(this, data, undefined);
        }
        // If wait for result, use the original async waiting mode.
        return new Promise((resolve) => {
            const err = ten_addon.ten_nodejs_ten_env_send_data(this, data, async (error) => {
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
            return ten_addon.ten_nodejs_ten_env_send_video_frame(this, videoFrame, undefined);
        }
        // If wait for result, use the original async waiting mode.
        return new Promise((resolve) => {
            const err = ten_addon.ten_nodejs_ten_env_send_video_frame(this, videoFrame, async (error) => {
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
            return ten_addon.ten_nodejs_ten_env_send_audio_frame(this, audioFrame, undefined);
        }
        // If wait for result, use the original async waiting mode.
        return new Promise((resolve) => {
            const err = ten_addon.ten_nodejs_ten_env_send_audio_frame(this, audioFrame, async (error) => {
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
            return ten_addon.ten_nodejs_ten_env_return_result(this, cmdResult, undefined);
        }
        // If wait for result, use the original async waiting mode.
        return new Promise((resolve) => {
            const err = ten_addon.ten_nodejs_ten_env_return_result(this, cmdResult, async (error) => {
                resolve(error);
            });
            if (err) {
                resolve(err);
            }
        });
    }
    async isPropertyExist(path) {
        return new Promise((resolve) => {
            ten_addon.ten_nodejs_ten_env_is_property_exist(this, path, async (result) => {
                resolve(result);
            });
        });
    }
    async getPropertyToJson(path) {
        return new Promise((resolve) => {
            const err = ten_addon.ten_nodejs_ten_env_get_property_to_json(this, path, async (result, error) => {
                resolve([result, error]);
            });
            if (err) {
                resolve(["", err]);
            }
        });
    }
    async setPropertyFromJson(path, jsonStr) {
        return new Promise((resolve) => {
            const err = ten_addon.ten_nodejs_ten_env_set_property_from_json(this, path, jsonStr, async (error) => {
                resolve(error);
            });
            if (err) {
                resolve(err);
            }
        });
    }
    async getPropertyNumber(path) {
        return new Promise((resolve) => {
            const err = ten_addon.ten_nodejs_ten_env_get_property_number(this, path, async (result, error) => {
                resolve([result, error]);
            });
            if (err) {
                resolve([0, err]);
            }
        });
    }
    async setPropertyNumber(path, value) {
        return new Promise((resolve) => {
            const err = ten_addon.ten_nodejs_ten_env_set_property_number(this, path, value, async (error) => {
                resolve(error);
            });
            if (err) {
                resolve(err);
            }
        });
    }
    async getPropertyString(path) {
        return new Promise((resolve) => {
            const err = ten_addon.ten_nodejs_ten_env_get_property_string(this, path, async (result, error) => {
                resolve([result, error]);
            });
            if (err) {
                resolve(["", err]);
            }
        });
    }
    async setPropertyString(path, value) {
        return new Promise((resolve) => {
            const err = ten_addon.ten_nodejs_ten_env_set_property_string(this, path, value, async (error) => {
                resolve(error);
            });
            if (err) {
                resolve(err);
            }
        });
    }
    async initPropertyFromJson(jsonStr) {
        return new Promise((resolve) => {
            const err = ten_addon.ten_nodejs_ten_env_init_property_from_json(this, jsonStr, async (error) => {
                resolve(error);
            });
            if (err) {
                resolve(err);
            }
        });
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
        return ten_addon.ten_nodejs_ten_env_log_internal(this, level, callerFunction, callerFile, callerLine, category, message, fieldsBuf);
    }
}
ten_addon.ten_nodejs_ten_env_register_class(TenEnv);
//# sourceMappingURL=ten_env.js.map
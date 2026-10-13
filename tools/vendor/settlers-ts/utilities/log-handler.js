"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.LogHandler = void 0;
const log_manager_1 = require("./log-manager");
/** handel error logging */
class LogHandler {
    constructor(moduleName) {
        this._moduleName = moduleName;
    }
    /** log an error */
    error(msg, exception) {
        LogHandler.manager.push({
            type: log_manager_1.LogType.Error,
            source: this._moduleName,
            msg,
            exception
        });
    }
    /** write a debug message. If [msg] is not a String it is displayed: as {prop:value} */
    debug(msg) {
        LogHandler.manager.push({
            type: log_manager_1.LogType.Debug,
            source: this._moduleName,
            msg
        });
    }
    static getLogManager() {
        return this.manager;
    }
}
exports.LogHandler = LogHandler;
LogHandler.manager = new log_manager_1.LogManager();

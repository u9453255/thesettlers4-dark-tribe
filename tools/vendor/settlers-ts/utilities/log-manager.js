"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.LogManager = exports.LogType = void 0;
var LogType;
(function (LogType) {
    LogType[LogType["Error"] = 0] = "Error";
    LogType[LogType["Debug"] = 1] = "Debug";
})(LogType || (exports.LogType = LogType = {}));
class LogManager {
    constructor() {
        this.log = [];
        this.logMsgCount = 0;
        this.listener = null;
    }
    onLogMessage(callback) {
        this.listener = callback;
        if (!callback) {
            return;
        }
        // send old messages
        for (const msg of this.log) {
            callback(msg);
        }
    }
    push(msg) {
        msg.index = this.logMsgCount++;
        // save message to log
        this.log.push(msg);
        if (this.log.length > 100) {
            this.log.shift();
        }
        // publish to listener
        if (this.listener) {
            this.listener(msg);
        }
        // write out to console
        if (typeof msg.msg !== 'string') {
            console.dir(msg.msg);
        }
        else {
            if (msg.type === LogType.Debug) {
                console.log(msg.source + '\t' + msg.msg);
            }
            else {
                console.error(msg.source + '\t' + msg.msg);
            }
        }
        if (msg.exception) {
            console.error(msg.source + '\t' + msg.exception.message);
        }
    }
}
exports.LogManager = LogManager;

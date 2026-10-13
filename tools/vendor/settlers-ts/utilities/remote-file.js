"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.RemoteFile = void 0;
const log_handler_1 = require("./log-handler");
const path_1 = require("./path");
const binary_reader_1 = require("../resources/file/binary-reader");
class RequestError extends Error {
    constructor(state, msg) {
        super(msg);
        this.statusText = msg;
        this.state = state;
        Object.seal(this);
    }
}
/**
* Handle Files loading from remote/web
*/
class RemoteFile {
    constructor(rootPath) {
        this.rootPath = rootPath;
        Object.seal(this);
    }
    /** load binary data from URL: rootPath + [path] + filename */
    loadBinary(path, filename) {
        const url = path_1.Path.combine(this.rootPath, path, filename);
        RemoteFile.log.debug('loading: ' + url);
        return new Promise((resolve, reject) => {
            const xhr = new XMLHttpRequest();
            xhr.onload = () => {
                if (xhr.status >= 200 && xhr.status < 300) {
                    const reader = new binary_reader_1.BinaryReader(xhr.response, 0, undefined, this.filenameFormUrl(url));
                    resolve(reader);
                }
                else {
                    RemoteFile.log.error('error load file:' + url);
                    reject(new RequestError(xhr.status, xhr.statusText));
                }
            };
            xhr.onerror = () => {
                RemoteFile.log.error('error load file:' + url);
                reject(new RequestError(xhr.status, xhr.statusText));
            };
            xhr.open('GET', url);
            xhr.responseType = 'arraybuffer';
            xhr.send();
        });
    }
    /** load string data from URL */
    loadString(url) {
        RemoteFile.log.debug('Load file as string: ' + url);
        return new Promise((resolve, reject) => {
            const xhr = new XMLHttpRequest();
            xhr.onload = () => {
                resolve(xhr.response);
            };
            xhr.onerror = () => {
                RemoteFile.log.error('error load file:' + url);
                reject(new RequestError(xhr.status, xhr.statusText));
            };
            /// setup query
            xhr.open('GET', url, true);
            xhr.responseType = 'text';
            /// call url
            xhr.send(null);
        });
    }
    /** Extracts the filename form an URL */
    filenameFormUrl(url) {
        if (url === '') {
            return '';
        }
        url = url.substring(0, (url.indexOf('#') === -1) ? url.length : url.indexOf('#'));
        url = url.substring(0, (url.indexOf('?') === -1) ? url.length : url.indexOf('?'));
        url = url.substring(url.lastIndexOf('/') + 1, url.length);
        return url;
    }
}
exports.RemoteFile = RemoteFile;
RemoteFile.log = new log_handler_1.LogHandler('RemoteFile');

"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.LibFileProvider = void 0;
const lib_file_reader_1 = require("../resources/lib/lib-file-reader");
const log_handler_1 = require("./log-handler");
const path_1 = require("./path");
class LibFileFile {
    get name() {
        return this.libFileItem.fileName;
    }
    get path() {
        return path_1.Path.combine(this.baseLibPath, this.libFileItem.getFullName());
    }
    constructor(libFileItem, baseLibPath) {
        this.libFileItem = libFileItem;
        this.baseLibPath = baseLibPath;
        Object.seal(this);
    }
    async readBinary() {
        return this.libFileItem.getReader();
    }
}
/** provides access to files in a .lib */
class LibFileProvider {
    constructor() {
        this.fileNameFilter = '.lib';
    }
    async processFile(newFiles) {
        if (!newFiles) {
            return null;
        }
        LibFileProvider.logHandler.debug('Read lib ' + newFiles.name);
        const baseLibPath = path_1.Path.getPathName(newFiles.name);
        const libFile = await newFiles.readBinary();
        const reader = new lib_file_reader_1.LibFileReader(libFile);
        const count = reader.getFileCount();
        const files = [];
        for (let i = 0; i < count; i++) {
            const file = reader.getFileInfo(i);
            files.push(new LibFileFile(file, baseLibPath));
        }
        return files;
    }
}
exports.LibFileProvider = LibFileProvider;
LibFileProvider.logHandler = new log_handler_1.LogHandler('LibFileProvider');

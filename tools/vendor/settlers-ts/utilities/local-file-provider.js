"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.LocalFileProvider = void 0;
const binary_reader_1 = require("../resources/file/binary-reader");
class LocalFileFile {
    get name() {
        return this.file.name;
    }
    get path() {
        return this.file.webkitRelativePath ?? this.file.name;
    }
    constructor(file) {
        this.file = file;
        Object.seal(this);
    }
    async readBinary() {
        const data = await this.file.arrayBuffer();
        return new binary_reader_1.BinaryReader(data, 0, null, this.name);
    }
}
class LocalFileProvider {
    constructor(fileList) {
        this.fileList = fileList;
    }
    async readFiles() {
        const files = [];
        for (const f of this.fileList) {
            files.push(new LocalFileFile(f));
        }
        return files;
    }
}
exports.LocalFileProvider = LocalFileProvider;

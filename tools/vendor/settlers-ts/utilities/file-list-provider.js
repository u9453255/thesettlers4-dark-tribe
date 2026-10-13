"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.FileListProvider = void 0;
const remote_file_1 = require("./remote-file");
const path_1 = require("./path");
class FileListFile {
    constructor(path) {
        this.name = path_1.Path.getFileName(path);
        this.path = path;
        Object.seal(this);
    }
    readBinary() {
        const fileProvider = new remote_file_1.RemoteFile();
        return fileProvider.loadBinary(this.path);
    }
}
class FileListProvider {
    async readFiles() {
        const fileNames = await this.loadFileList();
        return fileNames.map((f) => new FileListFile(f));
    }
    async loadFileList() {
        const fp = new remote_file_1.RemoteFile(process.env.BASE_URL);
        const fileListText = await fp.loadString('file-list.txt');
        return fileListText
            .split(/\r?\n/)
            .map((f) => path_1.Path.fixPath(f));
    }
}
exports.FileListProvider = FileListProvider;

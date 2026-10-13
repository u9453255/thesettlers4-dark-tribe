"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.FileManager = void 0;
const path_1 = require("./path");
const binary_reader_1 = require("../resources/file/binary-reader");
/** manages all files form all sources */
class FileManager {
    constructor() {
        this.files = [];
        this.proxys = [];
        Object.seal(this);
    }
    async registerProxy(proxy) {
        // send all previous added files to proxy
        for (const f of this.files) {
            await this.callProxyie(proxy, f);
        }
        // register proxy
        this.proxys.push(proxy);
    }
    /** call a proxy with a new file */
    async callProxyie(proxy, file) {
        if (!this.checkFileNameFilter(file.name, proxy.fileNameFilter, false)) {
            return;
        }
        const newFiles = await proxy.processFile(file);
        if (!newFiles) {
            return;
        }
        for (const newF of newFiles) {
            this.addFile(newF);
        }
    }
    /** add one file to the manager */
    async addFile(file) {
        this.files.push(file);
        // call proxies
        for (const p of this.proxys) {
            await this.callProxyie(p, file);
        }
    }
    /** add an file provider that gain access to files */
    async addSource(provider) {
        const list = await provider.readFiles();
        for (const file of list) {
            this.addFile(file);
        }
    }
    /** compaire a given fileName to a given filter string */
    checkFileNameFilter(fileName, filterName, exactMatch = true) {
        if (exactMatch) {
            return fileName.toLowerCase() === filterName.toLowerCase();
        }
        return fileName.toLowerCase().endsWith(filterName.toLowerCase());
    }
    /** return a file matching a given file name */
    findFile(filePath, exactMatch = true) {
        const file = this.files.find((f) => this.checkFileNameFilter(f.name, filePath, exactMatch));
        return file ?? null;
    }
    /** return the file content to a given file-name (full path) */
    readFile(filePath, exactMatch = true) {
        const f = this.findFile(filePath, exactMatch);
        if (!f) {
            return Promise.resolve(null);
        }
        return f.readBinary();
    }
    /** return a map of files content for a given file-name-map */
    async readFiles(fileNames, exactMatch = true) {
        const fileList = [];
        const fileKeys = [];
        for (const key of Object.keys(fileNames)) {
            const f = this.readFile(fileNames[key], exactMatch);
            fileList.push(f);
            fileKeys.push(key);
        }
        // wait for all files to be loaded
        const resultFiles = await Promise.all(fileList);
        const result = {};
        for (let i = 0; i < fileKeys.length; i++) {
            result[fileKeys[i]] = resultFiles[i] ?? new binary_reader_1.BinaryReader();
        }
        // return the result file map
        return result;
    }
    /** return a all files matching a given filter string */
    filter(filterStr) {
        const filterArray = (filterStr ?? '')
            .split('|')
            .map((f) => path_1.Path.fixPath(f));
        return this.files
            .filter((f) => filterArray
            .find((filter) => this.checkFileNameFilter(f.name, filter, false)));
    }
}
exports.FileManager = FileManager;

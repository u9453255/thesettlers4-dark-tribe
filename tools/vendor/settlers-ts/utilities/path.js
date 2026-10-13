"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.Path = void 0;
/**
* Handle path name modification
*/
class Path {
    /** fix the path separator */
    static fixPath(path) {
        return path.split('\\').join('/');
    }
    /** add an ending path separator */
    static addEndingSlash(path) {
        if ((path[path.length - 1] === '/') || (path[path.length - 1] === '\\')) {
            return path;
        }
        return path + '/';
    }
    /** add a filename to a path */
    static combine(path1, path2, path3) {
        if (!path1 && !path2 && !path3) {
            return '';
        }
        if (!path1) {
            return Path.combine(path2, path3);
        }
        let p = path1;
        if (path2) {
            p = Path.addEndingSlash(p) + path2;
        }
        if (path3) {
            p = Path.addEndingSlash(p) + path3;
        }
        return p;
    }
    /** return the filename of a file path */
    static getFileName(fullFileName) {
        const pos = fullFileName.lastIndexOf('/');
        if (pos < 0) {
            return fullFileName;
        }
        if ((pos + 1) >= fullFileName.length) {
            return '';
        }
        return fullFileName.substr(pos + 1);
    }
    /** return only the filename without the Extension e.g. "c:/abc/test.txt" -> "test" */
    static getFileNameWithoutExtension(fullFileName) {
        const fileName = Path.getFileName(fullFileName);
        const pos = fileName.lastIndexOf('.');
        if (pos < 0) {
            return fileName;
        }
        return fileName.substr(0, pos);
    }
    /** return the path of a filename */
    static getPathName(fullFileName) {
        const pos = fullFileName.lastIndexOf('/');
        if (pos <= 0) {
            return '';
        }
        return fullFileName.substr(0, pos);
    }
}
exports.Path = Path;

"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.OriginalLandscape = void 0;
const log_handler_1 = require("../../../utilities/log-handler");
/** provides access to the original landscape data */
class OriginalLandscape {
    constructor(mapFile, mapSize, mapChunkType) {
        this.mapSize = mapSize;
        const reader = mapFile.getChunkReader(mapChunkType);
        if (!reader) {
            OriginalLandscape.log.error('No landscape data in this file!');
            this.data = new Uint8Array(0);
            return;
        }
        this.data = reader.getBuffer();
    }
    /** returns every n-th byte of the data buffer */
    getSlice(offset) {
        const land = this.data;
        const result = new Uint8Array(this.mapSize.width * this.mapSize.height);
        if (land.length !== (result.length * 4)) {
            OriginalLandscape.log.error('Size of landscape Data is wrong: ' + land.length + '!==' + (result.length * 4));
            // return new Uint8Array(0);
        }
        for (let i = offset, j = 0; i < land.length; i += 4, j++) {
            result[j] = land[i];
        }
        return result;
    }
    getGroundHeight() {
        return this.getSlice(0);
    }
    getGroundType() {
        return this.getSlice(1);
    }
}
exports.OriginalLandscape = OriginalLandscape;
OriginalLandscape.log = new log_handler_1.LogHandler('OriginalLandscape');

"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.OriginalMapFile = void 0;
const checksum_calculator_1 = require("../../file/checksum-calculator");
const log_handler_1 = require("../../../utilities/log-handler");
const map_file_source_type_1 = require("../map-file-source-type");
const map_file_version_1 = require("../map-file-version");
const map_chunk_1 = require("./map-chunk");
const map_chunk_type_1 = require("./map-chunk-type");
/** provides access to a settlers map file (save game or map) */
class OriginalMapFile {
    constructor(data) {
        this.log = new log_handler_1.LogHandler('OriginalMapFile');
        this.checksum = -1;
        this.mapFileVersion = map_file_version_1.MapFileVersion.Normal;
        this.mapFileSourceType = map_file_source_type_1.MapFileSourceType.Unknown;
        this.mapChunks = [];
        this.processFileChunks(data);
    }
    getChunkCount() {
        return this.mapChunks.length;
    }
    /** return a chunk by it's index */
    getChunkByIndex(index) {
        return this.mapChunks[index];
    }
    /** return a chunk by it's type */
    getChunkByType(type) {
        const s = this.mapChunks;
        for (let i = 0; i < s.length; i++) {
            if (s[i].chunkType === type) {
                return s[i];
            }
        }
        return null;
    }
    /** return a reader to the chunk of a given type */
    getChunkReader(chunkType, minLength = null) {
        const chunk = this.getChunkByType(chunkType);
        if (!chunk) {
            this.log.error("Unable to find chunk '" + map_chunk_type_1.MapChunkType[chunkType] + "' in map file");
            return null;
        }
        const reader = chunk.getReader();
        if (minLength === null) {
            return reader;
        }
        if (reader.length < minLength) {
            this.log.error("Bad length of chunk '" + map_chunk_type_1.MapChunkType[chunkType] +
                "' in map file. Expect size of " + minLength + ' get ' + reader.length);
            return null;
        }
        return reader;
    }
    /** read the file and all chunks */
    processFileChunks(data) {
        /// settler4 save games are prefixed by a windows executable
        if ((data.length < 100)) {
            this.log.error('Not a Settlers save game: ' + data.filename);
            return false;
        }
        let dataStartOffset;
        if (data.readString(4, 0) === 'MZ\x90\x00') {
            /// settler4 save games are prefixed by a windows executable
            this.mapFileSourceType = map_file_source_type_1.MapFileSourceType.SaveGame;
            dataStartOffset = 6656;
        }
        else {
            this.mapFileSourceType = map_file_source_type_1.MapFileSourceType.GameMap;
            dataStartOffset = 0;
        }
        if ((data.length <= dataStartOffset + 8)) {
            this.log.error('Not a Settlers save game: ' + data.filename);
        }
        const headerSize = this.readFileHeader(data, dataStartOffset);
        const realChecksum = checksum_calculator_1.ChecksumCalculator.calc(data, dataStartOffset + headerSize);
        if (this.checksum !== realChecksum) {
            this.log.error('Checksum mismatch! real: ' + realChecksum + ' != file: ' + this.checksum);
        }
        this.readFileChunks(data, dataStartOffset + headerSize);
        return true;
    }
    /** read the main file header */
    readFileHeader(data, offset) {
        /// start of the save game data
        data.setOffset(offset);
        this.checksum = data.readIntBE();
        this.mapFileVersion = data.readIntBE();
        this.log.debug('Save game version: ' + this.mapFileVersion + ' checksum: ' + this.checksum);
        /// size of header
        return 4 * 2;
    }
    readFileChunks(data, offset) {
        while (offset > 0) {
            const newSection = new map_chunk_1.MapChunk();
            if (!newSection.readFromFile(data, offset)) {
                return;
            }
            this.mapChunks.push(newSection);
            offset = newSection.calcNextChunkOffset();
        }
    }
}
exports.OriginalMapFile = OriginalMapFile;

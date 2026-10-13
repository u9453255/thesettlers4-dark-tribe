"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.MapChunk = void 0;
const binary_reader_1 = require("../../file/binary-reader");
const checksum_calculator_1 = require("../../file/checksum-calculator");
const decode_settlers_1 = require("../../file/decode-settlers");
const decompress_1 = require("../../file/decompress");
const map_chunk_type_1 = require("./map-chunk-type");
/** provides information about a section in a map file */
class MapChunk {
    constructor() {
        this.length = 0;
        this.offset = 0;
        this.chunkType = map_chunk_type_1.MapChunkType.UnknownType;
        this.unpackedLength = 0;
        this.checksum = 0;
        this.unknown1 = 0;
        this.unknown2 = 0;
    }
    get chunkTypeAsString() {
        return map_chunk_type_1.MapChunkType[this.chunkType] ?? '';
    }
    getReader() {
        if (!this.reader) {
            return new binary_reader_1.BinaryReader();
        }
        const c = new decompress_1.Decompress();
        const file = c.unpack(this.reader, this.offset, this.length, this.unpackedLength);
        file.filename = this.reader.filename + ' [' + this.chunkTypeAsString + ']';
        return file;
    }
    /** return the position of the next section header in the file */
    calcNextChunkOffset() {
        return this.offset + this.length;
    }
    checkChecksum() {
        if (!this.reader) {
            return false;
        }
        const c1 = checksum_calculator_1.ChecksumCalculator.calc(this.reader, this.offset, this.length);
        return (c1 === this.checksum);
    }
    /** read the header of a section */
    readFromFile(data, offset) {
        const SectionHeaderSize = 24;
        const plainData = decode_settlers_1.DecodeSettlers.getReader(data, SectionHeaderSize, offset);
        if (plainData.length !== SectionHeaderSize) {
            return false;
        }
        this.reader = data;
        this.offset = SectionHeaderSize + offset;
        /// all sections have a type and a length... also the "end of file" section
        this.chunkType = plainData.readIntBE();
        this.length = plainData.readIntBE();
        if (this.chunkType === 0) {
            /// this is the "end of file" section
            return false;
        }
        this.unpackedLength = plainData.readIntBE();
        this.checksum = plainData.readIntBE();
        this.unknown1 = plainData.readIntBE();
        this.unknown2 = plainData.readIntBE();
        return true;
    }
    toString() {
        return 'Chunk @ ' + this.offset + ', size: ' + this.unpackedLength + '; Type=' + this.chunkType + '; checksum=' +
            this.checksum + ', unknown1=' + this.unknown1 + ', unknown2=' + this.unknown2;
    }
}
exports.MapChunk = MapChunk;

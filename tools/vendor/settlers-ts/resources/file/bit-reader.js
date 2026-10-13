"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.BitReader = void 0;
const log_handler_1 = require("../../utilities/log-handler");
/** Reads bits from a BinaryReader */
class BitReader {
    constructor(fileReader, offset, sourceLength) {
        // - get the data from the source
        this.data = fileReader.getBuffer(offset, sourceLength);
        this.pos = 0;
        this.buffer = 0;
        this.bufferLen = 0;
        Object.seal(this);
    }
    /** return the current curser position */
    getSourceOffset() {
        return this.pos;
    }
    sourceLeftLength() {
        return Math.max(0, (this.data.length - this.pos));
    }
    getBufferLength() {
        return this.bufferLen;
    }
    resetBitBuffer() {
        this.pos = this.pos - (this.bufferLen >> 3); // - move back the in-buffer for all not used byte
        this.bufferLen = 0; // - clear bit-buffer
        this.buffer = 0;
    }
    /** read and return [bitCount] bits from the stream */
    read(bitCount) {
        // - fill bit buffer
        if (this.bufferLen < bitCount) {
            // - read next byte
            if (this.pos >= this.data.length) {
                BitReader.log.error('Unable to read more data - End of data!');
                return 0;
            }
            const readInByte = this.data[this.pos];
            this.pos++;
            this.buffer |= (readInByte << (24 - this.bufferLen));
            this.bufferLen += 8;
        }
        // - read [bitCount] bits
        const bitValue = this.buffer >>> (32 - bitCount);
        this.buffer = this.buffer << bitCount;
        this.bufferLen -= bitCount;
        return bitValue;
    }
    eof() {
        return ((this.pos >= this.data.length) && (this.bufferLen <= 0));
    }
    toString() {
        return 'pos: ' + this.pos + ' len: ' + this.data.length + ' eof?: ' + this.eof();
    }
}
exports.BitReader = BitReader;
BitReader.log = new log_handler_1.LogHandler('BitReader');

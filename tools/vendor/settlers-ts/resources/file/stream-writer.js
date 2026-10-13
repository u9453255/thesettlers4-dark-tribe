"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.StreamWriter = void 0;
const log_handler_1 = require("../../utilities/log-handler");
const binary_reader_1 = require("./binary-reader");
/** Handles a buffer and a position pointer to write to */
class StreamWriter {
    constructor(length) {
        this.data = new Uint8Array(length);
        this.pos = 0;
        Object.seal(this);
    }
    getReader() {
        return new binary_reader_1.BinaryReader(this.data, 0, this.pos);
    }
    /** Read one Byte from the output stream */
    getByte(offset) {
        if (offset < 0) {
            StreamWriter.log.error('Read before data: @ ' + offset);
            return 0;
        }
        if (offset > this.data.length) {
            StreamWriter.log.error('Read behind data: size: ' + this.data.length + ' @ ' + offset);
            return 0;
        }
        return this.data[offset];
    }
    /** return the position the stream is writing to */
    getWriteOffset() {
        return this.pos;
    }
    getLength() {
        return this.data.length;
    }
    eof() {
        return ((this.pos < 0) || (this.pos >= this.data.length));
    }
    getLeftSize() {
        return (this.data.length - this.pos);
    }
    /** Write one byte to the stream */
    setByte(value) {
        if ((this.pos < 0) || (this.pos > this.data.length)) {
            StreamWriter.log.error('write out of data: size: ' + this.data.length + ' @ ' + this.pos);
            return false;
        }
        this.data[this.pos] = value;
        this.pos++;
        return true;
    }
}
exports.StreamWriter = StreamWriter;
StreamWriter.log = new log_handler_1.LogHandler('StreamWriter');

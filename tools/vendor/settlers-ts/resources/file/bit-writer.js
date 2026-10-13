"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.BitWriter = void 0;
const binary_reader_1 = require("./binary-reader");
class BitWriter {
    constructor() {
        this.output = [];
        this.currentByte = 0;
        this.currentByteLength = 0;
    }
    /**
     * not sure... should result in same way as "write()" but
     *   - seems not to be faster
     *   - is not as readable as "write"
     */
    writeNew(value, length) {
        let chByte = this.currentByte;
        let chByteLen = this.currentByteLength;
        while (length > 0) {
            const spaceBitCount = 8 - chByteLen;
            const needBitCount = (spaceBitCount < length) ? spaceBitCount : length;
            // shift to the right or to the left?
            const shiftNum = spaceBitCount - length;
            if (shiftNum < 0) {
                // the value we need to write to current byte
                //  needs more bits than remain
                chByte = chByte | (value >> (-shiftNum));
                value = value & ((1 << (-shiftNum)) - 1);
                length -= needBitCount;
            }
            else {
                // the value we need to write to current byte
                //  needs less bits than remain
                chByte = chByte | (value << shiftNum);
                value = 0;
                length = 0;
            }
            chByteLen = chByteLen + needBitCount;
            if (chByteLen >= 8) {
                this.output.push(chByte);
                chByte = 0;
                chByteLen = 0;
            }
        }
        this.currentByte = chByte;
        this.currentByteLength = chByteLen;
    }
    write(value, length) {
        let chByte = this.currentByte;
        let chByteLen = this.currentByteLength;
        for (let i = length - 1; i >= 0; i--) {
            chByte = (chByte << 1) | ((value >> i) & 0x1);
            chByteLen++;
            if (chByteLen >= 8) {
                this.output.push(chByte);
                chByte = 0;
                chByteLen = 0;
            }
        }
        this.currentByte = chByte;
        this.currentByteLength = chByteLen;
    }
    toReader() {
        // fill up last byte
        while (this.currentByteLength > 0) {
            this.write(0, 1);
        }
        return new binary_reader_1.BinaryReader(new Uint8Array(this.output));
    }
}
exports.BitWriter = BitWriter;

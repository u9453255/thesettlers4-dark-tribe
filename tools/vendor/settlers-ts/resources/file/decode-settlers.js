"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.DecodeSettlers = void 0;
const ara_crypt_1 = require("./ara-crypt");
const binary_reader_1 = require("./binary-reader");
class DecodeSettlers {
    static getReader(source, size, offset = -1) {
        const araCrypt = new ara_crypt_1.AraCrypt();
        if (offset >= 0) {
            source.setOffset(offset);
        }
        /// avoid reading behind eof
        const maxSize = Math.max(0, source.length - source.getOffset());
        size = Math.min(maxSize, size);
        /// set the settlers key
        araCrypt.reset(0x30313233, 0x34353637, 0x38393031);
        /// encrypt
        const data = new Uint8Array(size);
        for (let i = 0; i < size; i++) {
            data[i] = (source.readByte() ^ araCrypt.getNextKey()) & 0xFF;
        }
        return new binary_reader_1.BinaryReader(data);
    }
}
exports.DecodeSettlers = DecodeSettlers;

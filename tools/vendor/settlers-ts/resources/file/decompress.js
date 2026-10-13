"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.Decompress = void 0;
const log_handler_1 = require("../../utilities/log-handler");
const bit_reader_1 = require("./bit-reader");
const packer_1 = require("./packer");
const stream_writer_1 = require("./stream-writer");
/**
 *    Lz + Huffman decompressing
 */
class Decompress extends packer_1.Packer {
    constructor() {
        super();
        Object.seal(this);
    }
    unpack(inDataSrc, inOffset, inLength, outLength) {
        const inData = new bit_reader_1.BitReader(inDataSrc, inOffset, inLength);
        const writer = new stream_writer_1.StreamWriter(outLength);
        let huffmanTable = packer_1.Packer.DefaultHuffmanTable;
        let done = false;
        const codeTable = packer_1.Packer.createSymbolDirectory();
        while (!inData.eof()) {
            // -------
            // - read code type
            const codeType = inData.read(4);
            if (codeType < 0) {
                Decompress.log.error('CodeType == 0 -> out of sync!');
                break;
            }
            // -------
            // - read Code Word
            const codeWordLength = huffmanTable.index[codeType];
            let codeWordIndex = huffmanTable.value[codeType];
            if (codeWordLength > 0) {
                // - the codeword is bigger then 4 bits -> read more!
                codeWordIndex += inData.read(codeWordLength);
                if (codeWordIndex >= 0x0112) {
                    Decompress.log.error('CodeType(' + codeWordIndex + ') >= 0x0112 -> out of sync!');
                    break;
                }
            }
            const codeWord = codeTable.codeTable[codeWordIndex];
            // -------
            // - Histogram of code using
            codeTable.inc(codeWord);
            // -------
            // - execute codeword
            if (codeWord < 0x0100) {
                if (writer.eof()) {
                    Decompress.log.error('OutBuffer is to small!');
                    break;
                }
                // - this is a normal letter
                writer.setByte(codeWord);
            }
            else if (codeWord === 0x110) {
                // - create new entropy encoding table
                codeTable.generateCodes();
                const newIndex = [];
                const newValues = [];
                let base = 0;
                let length = 0;
                for (let i = 0; i < 16; i++) {
                    length--;
                    let bitValue = 0;
                    do {
                        length++;
                        bitValue = inData.read(1);
                    } while (bitValue === 0);
                    newIndex.push(length);
                    newValues.push(base);
                    base += (1 << length);
                }
                huffmanTable = new packer_1.IndexValueTable(newIndex, newValues);
            }
            else if (codeWord === 0x0111) {
                if (inData.sourceLeftLength() > 2) {
                    // - sometimes there is a end-of-stream (eos) codeword (Codeword == 273) if the out data is "too" long.
                    if (writer.eof()) {
                        Decompress.log.error('End-of-stream but Data buffer is not empty (' + inData.sourceLeftLength() + ' IN bytes left; ' + writer.getLeftSize() + ' OUT bytes left)? Out of sync!');
                        break;
                    }
                    // - in this case we ignore this eos and read the next part
                    inData.resetBitBuffer();
                }
                else {
                    if (!writer.eof()) {
                        Decompress.log.error('Done decompress (' + inData.sourceLeftLength() + ' IN bytes left; ' + writer.getLeftSize() + ' OUT bytes left)!');
                    }
                    // - end of data indicator
                    done = true;
                    break;
                }
            }
            else {
                // - copy from dictionary
                if (!this.fromDictionary(inData, writer, codeWord)) {
                    Decompress.log.error('Bad dictionary entry!');
                    break;
                }
            }
        }
        // - did an Error happen?
        if (!done) {
            Decompress.log.error('Unexpected End of Data in ' + inDataSrc.filename + ' eof: ' + inData.toString());
        }
        // - create new Reader from Data and return it
        return writer.getReader();
    }
    fromDictionary(inData, writer, codeWord) {
        // write out 4 additional bytes to the output
        let entryLength = 4;
        if (codeWord < 0x108) {
            entryLength += codeWord - 0x0100;
        }
        else {
            const index = codeWord - 0x0108;
            const bitCount = packer_1.Packer.LengthTable.index[index];
            const readInByte = inData.read(bitCount);
            entryLength += packer_1.Packer.LengthTable.value[index] + readInByte;
        }
        const distanceIndex = inData.read(3);
        const distanceLength = packer_1.Packer.DistanceTable.index[distanceIndex] + 1;
        const baseValue = packer_1.Packer.DistanceTable.value[distanceIndex];
        const base = inData.read(8);
        const offsetValue = inData.read(distanceLength);
        // - check if destination is big enough:
        if (writer.getWriteOffset() + entryLength > writer.getLength()) {
            Decompress.log.error('Out buffer is to small!');
            return false;
        }
        // source position in out buffer
        let srcPos = writer.getWriteOffset() - ((offsetValue | (base << distanceLength)) + (baseValue << 9));
        // we need to use single-byte-copy the data case, the src and dest can be overlapped
        for (let i = entryLength; i > 0; i--) {
            writer.setByte(writer.getByte(srcPos));
            srcPos++;
        }
        return true;
    }
}
exports.Decompress = Decompress;
Decompress.log = new log_handler_1.LogHandler('Decompress');

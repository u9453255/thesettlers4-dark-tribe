"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.MapSize = void 0;
const log_handler_1 = require("./log-handler");
/**
 * Stores height and width of the map
*/
class MapSize {
    // allowed sizes: 256, 320, 384, 448, 512, 576, 640, 704, 768, 832, 896, 960, 1024
    constructor(width, height) {
        this.width = width;
        this.height = height;
        const log = new log_handler_1.LogHandler('MapSize');
        if (width !== height) {
            log.error('width and height of a map should equal! width ' + width + ' != height ' + height);
        }
        if (!Number.isInteger(width / 64)) {
            log.error('map width and height should by multiplier of 64!');
        }
    }
    /** geht the map index position form a x,y coordinates  */
    toIndex(x, y) {
        return (x % this.width) + ((y % this.height) * this.width);
    }
    toString() {
        return 'size: [' + this.width + ' x ' + this.height + ']';
    }
}
exports.MapSize = MapSize;

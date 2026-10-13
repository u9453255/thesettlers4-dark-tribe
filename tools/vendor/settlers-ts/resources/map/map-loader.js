"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.MapLoader = void 0;
const game_map_loader_1 = require("./original/original-map/game-map-loader");
const savegame_loader_1 = require("./original/original-savegame/savegame-loader");
/** handel the loading of a map */
class MapLoader {
    /** return the map content of a loaded game */
    static getLoader(reader) {
        switch (MapLoader.getFileExtension(reader.filename).toLowerCase()) {
            case 'exe':
                return new savegame_loader_1.SaveGameLoader(reader);
            case 'map':
                return new game_map_loader_1.OriginalMapLoader(reader);
            case 'edm':
                return new game_map_loader_1.OriginalMapLoader(reader);
            default:
                return null;
        }
    }
    /** return the file extension e.g. "map" of "test.map" */
    static getFileExtension(fileName) {
        const pos = fileName.lastIndexOf('.');
        if (pos < 0) {
            return '';
        }
        return fileName.substr(pos + 1);
    }
}
exports.MapLoader = MapLoader;

"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.MapGameType = void 0;
/** Type of the game */
var MapGameType;
(function (MapGameType) {
    MapGameType[MapGameType["multiplayer"] = 0] = "multiplayer";
    MapGameType[MapGameType["singlePlayer"] = 1] = "singlePlayer";
    MapGameType[MapGameType["cooperation"] = 2] = "cooperation";
})(MapGameType || (exports.MapGameType = MapGameType = {}));

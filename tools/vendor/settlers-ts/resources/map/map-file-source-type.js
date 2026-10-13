"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.MapFileSourceType = void 0;
var MapFileSourceType;
(function (MapFileSourceType) {
    MapFileSourceType[MapFileSourceType["Unknown"] = 0] = "Unknown";
    MapFileSourceType[MapFileSourceType["GameMap"] = 1] = "GameMap";
    MapFileSourceType[MapFileSourceType["EditorMap"] = 2] = "EditorMap";
    MapFileSourceType[MapFileSourceType["SaveGame"] = 3] = "SaveGame";
})(MapFileSourceType || (exports.MapFileSourceType = MapFileSourceType = {}));

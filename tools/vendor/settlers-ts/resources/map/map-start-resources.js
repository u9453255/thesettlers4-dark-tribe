"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.MapStartResources = void 0;
/** How much resources a player has on game start */
var MapStartResources;
(function (MapStartResources) {
    MapStartResources[MapStartResources["unknown"] = 0] = "unknown";
    MapStartResources[MapStartResources["low"] = 1] = "low";
    MapStartResources[MapStartResources["medium"] = 2] = "medium";
    MapStartResources[MapStartResources["high"] = 3] = "high";
})(MapStartResources || (exports.MapStartResources = MapStartResources = {}));

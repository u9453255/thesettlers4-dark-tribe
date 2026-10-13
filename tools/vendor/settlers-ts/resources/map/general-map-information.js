"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.GeneralMapInformation = void 0;
const map_game_type_1 = require("./map-game-type");
const map_start_resources_1 = require("./map-start-resources");
/** store general map information */
class GeneralMapInformation {
    constructor() {
        this.gameType = map_game_type_1.MapGameType.multiplayer;
        this.playerCount = 0;
        this.startResources = map_start_resources_1.MapStartResources.unknown;
    }
    toString() {
        return 'gameType: ' + map_game_type_1.MapGameType[this.gameType] + '; ' +
            'playerCount: ' + this.playerCount + '; ' +
            'startResources: ' + map_start_resources_1.MapStartResources[this.startResources];
    }
}
exports.GeneralMapInformation = GeneralMapInformation;

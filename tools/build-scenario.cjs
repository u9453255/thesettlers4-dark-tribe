const fs=require('fs'),path=require('path'),assert=require('assert'),crypto=require('crypto');
const {readMap,writeMap}=require('./map-format.cjs');
const {prepareOpponent,verifyOpponent}=require('./scenario-opponent.cjs');
const {prepareGrassCell}=require('./scenario-terrain.cjs');
const {execFileSync}=require('child_process');
const {gameDir,python}=require('./project-paths.cjs');
const project=path.resolve(__dirname,'..'),game=process.argv[2]&&!process.argv[2].startsWith('--')?process.argv[2]:gameDir();
const source=path.join(game,'Map/Campaign/dark03.map');
const map=readMap(source),changes=new Map();
assert(crypto.createHash('sha256').update(map.raw).digest('hex')==='237a1df871a31818e00496a8b1b802cb414902136657aeb5e10561c11ec854fb','Unexpected source campaign map');
const get=t=>map.chunks.find(c=>c.type===t).data;
assert(get(2).length===90&&get(2).readUInt32LE(0)===1&&get(2).readUInt32LE(45)===3,'Unexpected base map players');
changes.set(2,{data:Buffer.concat([get(2).subarray(45),get(2).subarray(0,45)])});
for(const [type,step,offset] of [[7,6,5],[8,20,5]]) {
  const data=Buffer.from(get(type));assert(data.length%step===0);
  for(let i=offset;i<data.length;i+=step) {assert(data[i]===1||data[i]===2);data[i]=3-data[i];}
  changes.set(type,{data});
}
// An additional Dark clearing for unrestricted player-chosen construction.
// Native units need valid land; the old map's lakes cross the first trial spawn.
const terrain=Buffer.from(get(13)),objects=Buffer.from(get(6));
for(let y=284;y<=332;y++) for(let x=384;x<=432;x++) {
  const at=4*(y*576+x);
  prepareGrassCell(terrain,objects,at,true);
}
changes.set(13,{data:terrain});changes.set(6,{data:objects});
const masks=JSON.parse(execFileSync(python(),[path.join(__dirname,'scenario-footprints.py')],{encoding:'utf8'}));
const opponent=prepareOpponent(map,changes,masks);
const script=process.argv.includes('--minimal')?Buffer.from('function new_game() end\nfunction register_functions() end\n'):fs.readFileSync(path.join(project,'scenario/DarkTribe_FirstDawn.txt'));
changes.set(16,{data:Buffer.concat([script,Buffer.from([0])])});
const briefing='DARK TRIBE - FIRST DAWN\r\n\r\nCommand the Dark Tribe. Protect your base and defeat every Viking stronghold. The Vikings start in their own settlement in the west, with occupied towers and a working economy.\r\n\r\nMushroom Farmers carry mushrooms to your Dark Temple or Fortress; every delivery adds mana. Spend mana in the construction menu on Mushroom Farms, Manacopter Halls and one Dark Temple and one Dark Fortress, built on Dark land.\r\n\r\nSelect your Temple or Fortress to recruit warriors and specialists. Select a Manacopter Hall to build Manacopters that carry your units across the map.\0';
const tips='Construction contains Dark Tribe buildings. Farms cost 25 mana, Temples 100, Fortresses 150, Halls 120. Warriors/Throwers cost 15, Gardeners 5, Shamans 10. Manacopters cost 300. Native Mushroom Farmers deliver mushrooms for mana; no extra courier is needed.\0';
for(const t of [11,14]) changes.set(t,{data:Buffer.from(briefing,'latin1')});
for(const t of [12,15]) changes.set(t,{data:Buffer.from(tips,'latin1')});
const out=path.join(project,'work/generated/Map/User/DarkTribe_FirstDawn.map');fs.mkdirSync(path.dirname(out),{recursive:true});fs.writeFileSync(out,writeMap(map,changes));
const checked=readMap(out);assert(checked.chunks.find(c=>c.type===2).data.readUInt32LE(0)===3);
const opponentChecks=verifyOpponent(checked,masks);
for(const c of checked.chunks) {
  const original=map.chunks.find(o=>o.type===c.type);
  if(!changes.has(c.type)) assert(c.packed.equals(original.packed),'Unchanged chunk changed');
}
fs.writeFileSync(path.join(project,'research/scenario-build.json'),JSON.stringify({version:'0.2.15',source,source_sha256:crypto.createHash('sha256').update(map.raw).digest('hex'),output:out,output_sha256:crypto.createHash('sha256').update(checked.raw).digest('hex'),changedChunks:[...changes.keys()],nativeHumanRace:3,playerRecordBytes:45,unitRecordBytes:6,buildingRecordBytes:20,farms:8,temples:1,opponentRevision:2,opponent,opponentChecks,runtimeVerified:false},null,2));
console.log('Built First Dawn: human Dark Tribe; separated Viking start at (75,401), 4 occupied towers, 6 economy buildings and 68 units; offline checks passed');

// Iron Kingdom: the Trojan campaign map AO_trojan10 as a Dark Tribe scenario.
// Source layout: player 1 Trojans (the campaign's human start: one tower), player 2 a complete
// computer Viking kingdom, players 3 and 4 two Dark Tribe camps. The scenario keeps two players:
// both Dark camps become the human player 1, the Vikings stay player 2, the Trojans are removed.
// Landscape and the Viking settlement stay as authored (only the ground under the added barracks is
// levelled); the Vikings get full tower garrisons and a field army so they can attack early. Map contents
// come from the user's local game files.
const fs=require('fs'),path=require('path'),assert=require('assert'),crypto=require('crypto');
const {readMap,writeMap}=require('./map-format.cjs');
const {execFileSync}=require('child_process');
const {gameDir,python}=require('./project-paths.cjs');
const {removeFarmsInFire,assertNoFarmInFire,FarmLock,FARM}=require('./citadel-fire.cjs');
const {GOOD,TOOLSMITH,BARRACKS,reachable,footprintCells,recordCells,pileList,pileByte7,buildingRecord,FLAT_SPAN,FLAT_RADIUS,authoredGround,levelSite,siteFor,clearSite,placePiles,
  placeToolsmithPiles,describePile}=require('./scenario-supply.cjs');
const project=path.resolve(__dirname,'..'),game=process.argv[2]&&!process.argv[2].startsWith('--')?process.argv[2]:gameDir();
const SOURCE_SHA256='05f34cbb22aad2e8e390cf9f3d38dcd13e3513d69ccfa6f54c67450fc0b7b122';
const DARK_START={x:378,y:134},VIKING_START={x:475,y:353},N=576;
// Source owner -> scenario owner (0 = removed with all its units, buildings, land and goods).
const OWNER={1:0,2:2,3:1,4:1};
// Viking garrisons as in First Dawn: small tower 2 swordsmen + 1 bowman, large tower 3 + 3.
// Building record bytes 7..15 count the soldier types 29..37 (byte 6 enables the garrison).
const GARRISON={46:{7:2,10:1},47:{7:3,10:3}};
// Field army next to the Viking castle area: swordsmen L1/L2 and bowmen L1 (settler types 29, 30, 32).
const FIELD_ARMY=[[29,12],[30,6],[32,6]];
// Gardeners (settler type 50) next to the field army: the native AI never recruits them
// (CAIAgentGardeners::Execute 0x31740 is empty); the plugin's offence director uses them against Dark land.
const GARDENERS=8;
// Start supplies for new armies (r5): the source kingdom has weaponsmiths but no barracks. One Viking barracks
// west of the castle area (site searched from BARRACKS_NEAR) turns idle carriers and weapons into soldiers;
// weapon and shovel piles lie at the start. Map start content only, nothing is spawned later.
// Its ground is levelled like the kingdom's own buildings (scenario-supply.cjs authoredGround: here the whole
// footprint at the hotspot height, shading recomputed); a site whose ground (footprint and its one-cell ring)
// spans more than FLAT_SPAN gives way to the flattest one within FLAT_RADIUS cells (siteFor).
const BARRACKS_NEAR=[441,358];
const SUPPLY=[GOOD.SWORD,GOOD.SWORD,GOOD.BOW,GOOD.BATTLEAXE,GOOD.SHOVEL,GOOD.SHOVEL];
// Toolsmith start buffer (r29): the kingdom's own TOOLSMITH (source record, unchanged) gets IRONBAR x16 and COAL x16
// beside its entrance, so the plugin's shovel orders are made before the iron chain delivers. r40: HAMMER x2 as well,
// like the other maps: the kingdom starts with 2 SMITHs and 15 hammers, but in the field (pid 8520) the toolsmith lost
// its smith around tick 22,500 and no replacement came for the rest of the game (the AI's "waiting for its smith"
// task cleared the shovel order 17,519 times), because a new smith needs a carrier plus a hammer and the toolsmith is
// the only hammer maker.
const source=path.join(game,'Map/Campaign/AO_trojan10.map');
const map=readMap(source),changes=new Map();
assert(crypto.createHash('sha256').update(map.raw).digest('hex')===SOURCE_SHA256,'Unexpected source campaign map');
const get=t=>map.chunks.find(c=>c.type===t).data;
const masks=JSON.parse(execFileSync(python(),[path.join(__dirname,'scenario-footprints.py')],{encoding:'utf8',maxBuffer:1<<26}));

const players=get(2),record=(i,o)=>players.readUInt32LE(45*i+o);
assert(players.length===180&&[0,1,2,3].map(i=>record(i,0)).join()==='4,1,3,3','Unexpected source players');
assert(record(2,4)===DARK_START.x&&record(2,8)===DARK_START.y&&record(1,4)===VIKING_START.x&&record(1,8)===VIKING_START.y,'Unexpected source starts');
changes.set(2,{data:Buffer.concat([players.subarray(90,135),players.subarray(45,90)]),count:2});
const info=Buffer.from(get(1));
assert(info.readUInt32LE(4)===4&&info.readUInt32LE(12)===N,'Unexpected map info');
info.writeUInt32LE(2,4);changes.set(1,{data:info});
// Slot/team table: 33 header bytes, then (slot, team) per player. Same as First Dawn:
// human Dark player 1 in team 1, computer Vikings in team 2.
const teams=get(3);assert(teams.length===41&&teams.subarray(0,33).every(b=>b===0),'Unexpected team table');
changes.set(3,{data:Buffer.concat([Buffer.alloc(33),Buffer.from([0,1,1,2])]),count:2});

function remap(type,step) {
  const data=get(type),kept=[];assert(data.length%step===0);
  for(let i=0;i<data.length;i+=step) {
    const r=Buffer.from(data.subarray(i,i+step)),owner=OWNER[r[5]];
    assert(owner!==undefined,'Unexpected owner '+r[5]);
    if(!owner)continue;r[5]=owner;kept.push(r);
  }
  return kept;
}
const units=remap(7,6),buildings=remap(8,20);
let garrisoned=0;
for(const b of buildings)if(b[5]===2&&GARRISON[b[4]]) {
  assert(b[6]===1&&b[7]===1&&b.subarray(8,20).every(v=>v===0),'Unexpected original tower garrison');
  for(const [at,count] of Object.entries(GARRISON[b[4]]))b[at]=count;garrisoned++;
}
// Initial land owner (objects byte 1) seeds the native sectors before buildings load.
const objects=Buffer.from(get(6)),land=Buffer.from(get(13));
const sourceOwner=k=>get(6)[4*k+1];
for(let at=1;at<objects.length;at+=4) {
  assert([0,1,2].includes(objects[at]),'Unexpected initial owner');
  if(objects[at]===1)objects[at]=0;
}
const goods=[],sourceGoods=get(9);
for(let i=0;i<sourceGoods.length;i+=8) {
  const g=sourceGoods.subarray(i,i+8),k=g.readUInt16LE(2)*N+g.readUInt16LE(0);
  assert([1,2].includes(sourceOwner(k)),'Goods outside a settlement');
  if(sourceOwner(k)===2)goods.push(Buffer.from(g));
}
// Extra Viking soldiers on free, owned grass next to the start: no object, no blocking or
// keep-clear flag, not under any building footprint, not on another unit, not Dark land.
const occupied=new Set(),unitCells=new Set(units.map(u=>u.readUInt16LE(2)*N+u.readUInt16LE(0)));
for(const b of buildings) {
  const cells=b[5]===2?masks._viking[b[4]]:masks[b[4]];assert(cells,'Missing footprint '+b[4]);
  for(const [dx,dy] of cells)occupied.add((b.readUInt16LE(2)+dy)*N+b.readUInt16LE(0)+dx);
}
function soldierSpot() {
  for(let r=0;r<60;r++)for(let y=VIKING_START.y-r;y<=VIKING_START.y+r;y++)for(let x=VIKING_START.x-r;x<=VIKING_START.x+r;x++) {
    if(Math.max(Math.abs(x-VIKING_START.x),Math.abs(y-VIKING_START.y))!==r)continue;
    const k=y*N+x,at=4*k,t=land[at+1];
    if(objects[at+1]!==2||occupied.has(k)||unitCells.has(k)||objects[at]||(objects[at+2]&0x5b)||(land[at+2]&64)||t<16||t>=32||t===21)continue;
    unitCells.add(k);return {x,y};
  }
  throw Error('No free cell for a Viking soldier');
}
// A lone Viking priest of the campaign story stands between the Dark camps: bring it home.
let relocated=0;
for(const u of units)if(u[5]===2&&objects[4*(u.readUInt16LE(2)*N+u.readUInt16LE(0))+1]!==2) {
  unitCells.delete(u.readUInt16LE(2)*N+u.readUInt16LE(0));const {x,y}=soldierSpot();u.writeUInt16LE(x,0);u.writeUInt16LE(y,2);relocated++;
}
assert(relocated===1,'Expected one Viking unit outside the kingdom');
const army=[];
for(const [type,count] of FIELD_ARMY)for(let i=0;i<count;i++) {
  const {x,y}=soldierSpot(),u=Buffer.alloc(6);u.writeUInt16LE(x,0);u.writeUInt16LE(y,2);u[4]=type;u[5]=2;army.push(u);
}
const gardeners=[];
for(let i=0;i<GARDENERS;i++) {
  const {x,y}=soldierSpot(),u=Buffer.alloc(6);u.writeUInt16LE(x,0);u.writeUInt16LE(y,2);u[4]=50;u[5]=2;gardeners.push(u);
}
const keptGoods=goods.length,allUnits=Buffer.concat([...units,...army,...gardeners]);
const maskOf=(type,owner)=>owner===2?masks._viking[type]:masks[type],vikingStart=[VIKING_START.x,VIKING_START.y];
const blockOf=(type,owner)=>masks._raceBlocks[owner===2?1:3][type],ground=authoredGround(N,land,Buffer.concat(buildings),maskOf,blockOf);
const barracksLevel=ground.levelOf(masks._viking[BARRACKS],blockOf(BARRACKS,2));
const barracksSite=siteFor({N,land,objects,owner:2,mask:masks._viking[BARRACKS],door:masks._vikingDoors[BARRACKS],level:barracksLevel,near:BARRACKS_NEAR,
  blocked:footprintCells(N,Buffer.concat(buildings),maskOf,2),units:recordCells(N,allUnits,6),piles:recordCells(N,Buffer.concat(goods),8),
  seen:reachable(N,land,objects,2,vikingStart),maxStep:ground.step,maxSpan:FLAT_SPAN,flatRadius:FLAT_RADIUS});
const barracksObjectsCleared=clearSite(N,objects,masks._viking[BARRACKS],barracksSite.x,barracksSite.y);
const barracksGround=levelSite(N,land,barracksLevel,barracksSite.x,barracksSite.y);
buildings.push(buildingRecord(barracksSite.x,barracksSite.y,BARRACKS,2));
const supply=placePiles({N,land,objects,owner:2,anchor:vikingStart,goods:SUPPLY,byte7:pileByte7(Buffer.concat(goods)),
  blocked:footprintCells(N,Buffer.concat(buildings),maskOf,1),units:recordCells(N,allUnits,6),piles:pileList(Buffer.concat(goods)),
  seen:reachable(N,land,objects,2,vikingStart)});
goods.push(...supply);
const toolsmiths=buildings.filter(b=>b[5]===2&&b[4]===TOOLSMITH);assert(toolsmiths.length===1,'Expected one Viking toolsmith');
const toolsmith={x:toolsmiths[0].readUInt16LE(0),y:toolsmiths[0].readUInt16LE(2)};
toolsmith.entrance=[toolsmith.x+masks._vikingDoors[TOOLSMITH][0],toolsmith.y+masks._vikingDoors[TOOLSMITH][1]];
const toolInputs=placeToolsmithPiles({N,land,objects,owner:2,byte7:pileByte7(Buffer.concat(goods)),blocked:footprintCells(N,Buffer.concat(buildings),maskOf,1),
  units:recordCells(N,allUnits,6),piles:pileList(Buffer.concat(goods)),seen:reachable(N,land,objects,2,vikingStart)},toolsmith.entrance);
goods.push(...toolInputs);
// Clean start (r31): no Dark Mushroom Farm inside the fire field of a Dark Temple/Fortress (citadel-fire.cjs, FarmLock 33).
// Removed last, so every placement above sees the source layout; the record goes and the flags the editor baked under
// it are cleared. The map stores no Mushroom Farmer or mushroom bound to a farm.
const fire=removeFarmsInFire({N,buildings,objects,farmMask:masks[FARM],farmCore:masks._raceBlocks['3'][FARM],maskOf});
buildings.splice(0,buildings.length,...fire.kept);
changes.set(7,{data:allUnits,count:units.length+army.length+gardeners.length});
changes.set(8,{data:Buffer.concat(buildings),count:buildings.length});
changes.set(9,{data:Buffer.concat(goods),count:goods.length});
changes.set(6,{data:objects});
changes.set(13,{data:land});
const script=fs.readFileSync(path.join(project,'scenario/DarkTribe_IronKingdom.txt'));
changes.set(16,{data:Buffer.concat([script,Buffer.from([0])])});
const briefing='DARK TRIBE - IRON KINGDOM\r\n\r\nThe Vikings rule the south-east: mines, smelters and weaponsmiths feed an army behind seventeen manned towers. Your Dark Tribe holds two camps in the north, each with its own Temple. Defend both and destroy every Viking tower.\r\n\r\nThe Vikings grow stronger the longer they are left alone. The open Dark land between your camps has room for new buildings. Build Manacopters to strike where the Vikings do not expect you.\0';
const tips='Farms cost 25 mana, Temples 100, Fortresses 150, Halls 120. Warriors/Throwers cost 15, Gardeners 5, Shamans 10. Manacopters cost 300. You start with two Temples and can add one Fortress.\0';
for(const t of [11,14])changes.set(t,{data:Buffer.from(briefing,'latin1')});
for(const t of [12,15])changes.set(t,{data:Buffer.from(tips,'latin1')});
const out=path.join(project,'work/generated/Map/User/DarkTribe_IronKingdom.map');fs.mkdirSync(path.dirname(out),{recursive:true});fs.writeFileSync(out,writeMap(map,changes));
const checked=readMap(out);
assertNoFarmInFire(checked.chunks.find(c=>c.type===8).data,'Iron Kingdom');
for(const c of checked.chunks) {
  const original=map.chunks.find(o=>o.type===c.type);
  if(!changes.has(c.type))assert(c.packed.equals(original.packed),'Unchanged chunk changed');
}
const count=(list,owner)=>list.filter(r=>r[5]===owner).length;
fs.writeFileSync(path.join(project,'research/iron-kingdom-build.json'),JSON.stringify({version:'0.2.15',scenario:'iron_kingdom',source,source_sha256:SOURCE_SHA256,output:out,
  output_sha256:crypto.createHash('sha256').update(checked.raw).digest('hex'),changedChunks:[...changes.keys()],darkStart:DARK_START,vikingStart:VIKING_START,
  darkUnits:count(units,1),darkBuildings:count(buildings,1),vikingUnits:count(units,2)+army.length+gardeners.length,vikingBuildings:count(buildings,2),
  garrisonedTowers:garrisoned,fieldArmy:army.length,gardeners:gardeners.length,gardenerCells:gardeners.map(u=>[u.readUInt16LE(0),u.readUInt16LE(2)]),relocatedVikingUnits:relocated,vikingGoods:goods.length,sourceVikingGoods:keptGoods,removedTrojanGoods:sourceGoods.length/8-keptGoods,
  barracks:{type:BARRACKS,x:barracksSite.x,y:barracksSite.y,entrance:barracksSite.door,objectsCleared:barracksObjectsCleared,
    ground:{levelled:ground.mode,height:barracksGround.height,originalSpan:barracksGround.span,groundSpan:barracksSite.groundSpan,maxSpan:FLAT_SPAN,flatRadius:FLAT_RADIUS,
      cells:barracksGround.cells,heightsChanged:barracksGround.changed,shadingsChanged:barracksGround.shaded,step:barracksSite.step,authoredStep:ground.step,
      authoredBuildings:ground.buildings,searchRing:barracksSite.ring}},
  weaponPiles:supply.filter(r=>r[4]!==GOOD.SHOVEL).map(describePile),shovelPiles:supply.filter(r=>r[4]===GOOD.SHOVEL).map(describePile),
  toolsmith,toolsmithPiles:toolInputs.map(describePile),extraCarriers:0,fireCleanStart:{farmLock:FarmLock,removedFarms:fire.removed},runtimeVerified:false},null,2));
console.log(`Built Iron Kingdom: Dark camps merged (${count(buildings,1)} buildings, ${count(units,1)} units); Viking kingdom ${count(buildings,2)} buildings, ${garrisoned} manned towers, ${army.length} field soldiers, ${gardeners.length} gardeners ${gardeners.map(u=>`(${u.readUInt16LE(0)},${u.readUInt16LE(2)})`).join(' ')}; Trojans removed`);
console.log(`Viking barracks at (${barracksSite.x},${barracksSite.y}), entrance (${barracksSite.door}), ${barracksObjectsCleared} objects cleared, `+
  `${ground.mode} levelled at height ${barracksGround.height} (span ${barracksGround.span}, ground span ${barracksSite.groundSpan}, ${barracksGround.changed} heights and ${barracksGround.shaded} shadings changed, step ${barracksSite.step} <= ${ground.step}); supply piles (good x amount) ${supply.map(describePile).map(p=>`${p.good}x${p.amount}@(${p.x},${p.y})`).join(' ')}`);
console.log(`Viking toolsmith (source) at (${toolsmith.x},${toolsmith.y}), entrance (${toolsmith.entrance}); inputs ${toolInputs.map(describePile).map(p=>`${p.good}x${p.amount}@(${p.x},${p.y})`).join(' ')}`);
console.log(`Clean start: ${fire.removed.length} Dark farm(s) inside a Temple/Fortress fire field removed (FarmLock ${FarmLock}) `+fire.removed.map(f=>`(${f.x},${f.y}) Euclid ${f.euclid} from (${f.citadel.x},${f.citadel.y})`).join(' '));

// The Alliance: Dark campaign map 7 ("Helping Hands", 512x512) as a Dark Tribe scenario.
// Source layout: player 1 Mayas (the campaign's human start: towers and a stonecutter), 2 Romans
// and 3 Vikings (computer settlements), 4 the Dark Tribe (one Temple and an army, no farms).
// The scenario keeps all four players: the Dark Tribe becomes the human player 1, the Mayas move
// to player 4, and the three peoples are allied computer players (team 2). Landscape and the
// settlements stay as authored (only the ground under the added barracks is levelled); every tower gets
// at least a full garrison and each people a small field army next to its start. Map contents come from
// the user's local game files.
const fs=require('fs'),path=require('path'),assert=require('assert'),crypto=require('crypto');
const {readMap,writeMap}=require('./map-format.cjs');
const {execFileSync}=require('child_process');
const {gameDir,python}=require('./project-paths.cjs');
const {removeFarmsInFire,assertNoFarmInFire,FarmLock,FARM}=require('./citadel-fire.cjs');
const {GOOD,TOOLSMITH,BARRACKS,CARRIER,reachable,footprintCells,recordCells,pileList,pileByte7,buildingRecord,unitRecord,FLAT_SPAN,FLAT_RADIUS,authoredGround,levelSite,siteFor,
  clearSite,placePiles,placeToolsmithPiles,describePile}=require('./scenario-supply.cjs');
const project=path.resolve(__dirname,'..'),game=process.argv[2]&&!process.argv[2].startsWith('--')?process.argv[2]:gameDir();
const SOURCE_SHA256='7d95a9fa2b8ee7019144e980b240a43d032bd2cf7c724037bcfb5ba9f4dcb403';
const N=512;
// Source player -> scenario player; source record order of the new players 1..4.
const OWNER={1:4,2:2,3:3,4:1},ORDER=[3,1,2,0];
// Scenario players: race and start (tools/scenarios.py, native/scenario-model.hpp).
const PLAYERS=[[3,303,423],[0,98,353],[1,367,116],[2,260,263]];
// Minimum garrisons (bytes 7..9 swordsmen L1..L3, 10..12 bowmen L1..L3): missing soldiers are
// added at level 1, authored higher-level soldiers stay.
const GARRISON={46:[2,1],47:[3,3]};
// Field army of each computer people next to its start: swordsmen L1/L2, bowmen L1 (types 29, 30, 32).
const FIELD_ARMY=[[29,6],[30,3],[32,3]];
// Gardeners (settler type 50) of each computer people: the native AI never recruits them
// (CAIAgentGardeners::Execute 0x31740 is empty); the plugin's offence director uses them against Dark land.
const GARDENERS=4;
// Start supplies for new armies (r5): the three peoples have no barracks, weapons or residences in the source.
// Each gets one barracks of its race (site searched from BARRACKS_NEAR; objects and their flags on footprint and
// halo cleared, ground type as authored), weapon piles beside its entrance, two shovel piles at the start and
// 40 more carriers after the gardeners. Map start content only, nothing is spawned later. The barracks ground
// is levelled like the peoples' own buildings (scenario-supply.cjs authoredGround: here the blocked core at the
// hotspot height, shading recomputed); a site whose ground (core and its one-cell ring) spans more than FLAT_SPAN
// gives way to the flattest one within FLAT_RADIUS cells (siteFor).
const BARRACKS_NEAR={2:[122,364],3:[367,103],4:[299,272]};
const WEAPONS={2:[GOOD.SWORD,GOOD.SWORD,GOOD.BOW,GOOD.BOW],3:[GOOD.SWORD,GOOD.SWORD,GOOD.BOW,GOOD.BATTLEAXE],4:[GOOD.SWORD,GOOD.SWORD,GOOD.BOW,GOOD.BLOWGUN]};
const SHOVELS=[GOOD.SHOVEL,GOOD.SHOVEL],EXTRA_CARRIERS=40;
// Toolsmith (r29): no people has one, so the plugin's shovel orders could never be made. Each gets a TOOLSMITH of its
// race placed like its barracks (siteFor searched from the barracks hotspot, objects cleared, blocked core levelled)
// after every unit and r28 pile (their positions stay), with IRONBAR x16, COAL x16 and HAMMER x2 beside its entrance.
// Every people already has idle SMITHs (2/3/2) and 70+ carriers.
const source=path.join(game,'Map/Campaign/dark07.map');
const map=readMap(source),changes=new Map();
assert(crypto.createHash('sha256').update(map.raw).digest('hex')===SOURCE_SHA256,'Unexpected source campaign map');
const get=t=>map.chunks.find(c=>c.type===t).data;
const masks=JSON.parse(execFileSync(python(),[path.join(__dirname,'scenario-footprints.py')],{encoding:'utf8',maxBuffer:1<<26}));

const players=get(2),record=(i,o)=>players.readUInt32LE(45*i+o);
assert(players.length===180&&[0,1,2,3].map(i=>record(i,0)).join()==='2,0,1,3','Unexpected source players');
const reordered=Buffer.concat(ORDER.map(i=>players.subarray(45*i,45*i+45)));
PLAYERS.forEach(([race,x,y],i)=>assert(reordered.readUInt32LE(45*i)===race&&reordered.readUInt32LE(45*i+4)===x&&reordered.readUInt32LE(45*i+8)===y,'Unexpected source start'));
changes.set(2,{data:reordered,count:4});
const info=get(1);assert(info.readUInt32LE(4)===4&&info.readUInt32LE(12)===N,'Unexpected map info');
// Slot/team table (33 header bytes, then slot and team per player) as in First Dawn:
// human Dark player in team 1, the three computer peoples together in team 2.
const teams=get(3);assert(teams.length===41&&teams.subarray(0,33).every(b=>b===0),'Unexpected team table');
changes.set(3,{data:Buffer.concat([Buffer.alloc(33),Buffer.from([0,1,1,2,1,2,1,2])]),count:4});

function remap(type,step) {
  const data=get(type),out=[];assert(data.length%step===0);
  for(let i=0;i<data.length;i+=step) {
    const r=Buffer.from(data.subarray(i,i+step));assert(OWNER[r[5]],'Unexpected owner '+r[5]);r[5]=OWNER[r[5]];out.push(r);
  }
  return out;
}
const units=remap(7,6),buildings=remap(8,20);
let reinforced=0;
for(const b of buildings)if(b[5]!==1&&GARRISON[b[4]]) {
  assert(b[6]===1&&b.subarray(13,20).every(v=>v===0),'Unexpected tower record');
  const [swords,bows]=GARRISON[b[4]],have=[b[7]+b[8]+b[9],b[10]+b[11]+b[12]];
  if(have[0]<swords||have[1]<bows)reinforced++;
  b[7]+=Math.max(0,swords-have[0]);b[10]+=Math.max(0,bows-have[1]);
}
// Initial land owner (objects byte 1) follows the players; the Dark Tribe owns no land.
const objects=Buffer.from(get(6)),land=Buffer.from(get(13));
for(let at=1;at<objects.length;at+=4) {
  assert([0,1,2,3].includes(objects[at]),'Unexpected initial owner');
  if(objects[at]===1)objects[at]=4;
}
// Every computer unit already stands on its own land, every Dark unit on unowned Dark land.
for(const u of units)assert(objects[4*(u.readUInt16LE(2)*N+u.readUInt16LE(0))+1]===(u[5]===1?0:u[5]),'Unit outside its land');
const occupied=new Set(),unitCells=new Set(units.map(u=>u.readUInt16LE(2)*N+u.readUInt16LE(0)));
for(const b of buildings) {
  const cells=masks._races[PLAYERS[b[5]-1][0]][b[4]];assert(cells,'Missing footprint '+b[4]);
  for(const [dx,dy] of cells)occupied.add((b.readUInt16LE(2)+dy)*N+b.readUInt16LE(0)+dx);
}
// Extra soldiers on free, owned grass next to the start: no object, no blocking or keep-clear
// flag (0x5B), not under a footprint, not on another unit, not Dark land.
function soldierSpot(owner) {
  const [,cx,cy]=PLAYERS[owner-1];
  for(let r=0;r<60;r++)for(let y=cy-r;y<=cy+r;y++)for(let x=cx-r;x<=cx+r;x++) {
    if(Math.max(Math.abs(x-cx),Math.abs(y-cy))!==r||x<2||y<2||x>=N-2||y>=N-2)continue;
    const k=y*N+x,at=4*k,t=land[at+1];
    if(objects[at+1]!==owner||occupied.has(k)||unitCells.has(k)||objects[at]||(objects[at+2]&0x5b)||(land[at+2]&64)||t<16||t>=32||t===21)continue;
    unitCells.add(k);return {x,y};
  }
  throw Error('No free cell for a soldier of player '+owner);
}
const army=[];
for(const owner of [2,3,4])for(const [type,count] of FIELD_ARMY)for(let i=0;i<count;i++) {
  const {x,y}=soldierSpot(owner),u=Buffer.alloc(6);u.writeUInt16LE(x,0);u.writeUInt16LE(y,2);u[4]=type;u[5]=owner;army.push(u);
}
const gardeners=[];
for(const owner of [2,3,4])for(let i=0;i<GARDENERS;i++) {
  const {x,y}=soldierSpot(owner),u=Buffer.alloc(6);u.writeUInt16LE(x,0);u.writeUInt16LE(y,2);u[4]=50;u[5]=owner;gardeners.push(u);
}
const maskOf=(type,owner)=>masks._races[PLAYERS[owner-1][0]][type],startOf=owner=>PLAYERS[owner-1].slice(1);
const blockOf=(type,owner)=>masks._raceBlocks[PLAYERS[owner-1][0]][type],ground=authoredGround(N,land,Buffer.concat(buildings),maskOf,blockOf);
const sourceGoods=get(9),barracks=[];
for(const owner of [2,3,4]) {
  const race=PLAYERS[owner-1][0],mask=masks._races[race][BARRACKS],level=ground.levelOf(mask,blockOf(BARRACKS,owner));
  const site=siteFor({N,land,objects,owner,mask,door:masks._raceDoors[race][BARRACKS],level,near:BARRACKS_NEAR[owner],
    blocked:footprintCells(N,Buffer.concat(buildings),maskOf,2),units:recordCells(N,Buffer.concat([...units,...army,...gardeners]),6),
    piles:recordCells(N,sourceGoods,8),seen:reachable(N,land,objects,owner,startOf(owner)),maxStep:ground.step,maxSpan:FLAT_SPAN,flatRadius:FLAT_RADIUS});
  const objectsCleared=clearSite(N,objects,mask,site.x,site.y),levelled=levelSite(N,land,level,site.x,site.y);
  buildings.push(buildingRecord(site.x,site.y,BARRACKS,owner));
  for(const [dx,dy] of mask)occupied.add((site.y+dy)*N+site.x+dx);
  barracks.push({owner,type:BARRACKS,x:site.x,y:site.y,entrance:site.door,objectsCleared,
    ground:{levelled:ground.mode,height:levelled.height,originalSpan:levelled.span,groundSpan:site.groundSpan,maxSpan:FLAT_SPAN,flatRadius:FLAT_RADIUS,cells:levelled.cells,
      heightsChanged:levelled.changed,shadingsChanged:levelled.shaded,step:site.step,authoredStep:ground.step,authoredBuildings:ground.buildings,searchRing:site.ring}});
}
// Carriers after the gardeners (earlier positions stay), off every pile.
for(const [x,y] of pileList(sourceGoods))unitCells.add(y*N+x);
const carriers=[];
for(const owner of [2,3,4])for(let i=0;i<EXTRA_CARRIERS;i++){const {x,y}=soldierSpot(owner);carriers.push(unitRecord(x,y,CARRIER,owner));}
const allUnits=Buffer.concat([...units,...army,...gardeners,...carriers]),piles=pileList(sourceGoods),supply={};
const blocked=footprintCells(N,Buffer.concat(buildings),maskOf,1),unitSet=recordCells(N,allUnits,6),byte7=pileByte7(sourceGoods);
for(const owner of [2,3,4]) {
  const common={N,land,objects,owner,byte7,blocked,units:unitSet,piles,seen:reachable(N,land,objects,owner,startOf(owner))};
  supply[owner]={weapons:placePiles({...common,anchor:barracks.find(b=>b.owner===owner).entrance,goods:WEAPONS[owner]}),
                 shovels:placePiles({...common,anchor:startOf(owner),goods:SHOVELS})};
}
const toolsmiths=[];
for(const owner of [2,3,4]) {
  const race=PLAYERS[owner-1][0],mask=masks._races[race][TOOLSMITH],level=ground.levelOf(mask,blockOf(TOOLSMITH,owner)),b=barracks.find(r=>r.owner===owner);
  const site=siteFor({N,land,objects,owner,mask,door:masks._raceDoors[race][TOOLSMITH],level,near:[b.x,b.y],
    blocked:footprintCells(N,Buffer.concat(buildings),maskOf,2),units:unitSet,piles:new Set(piles.map(([x,y])=>y*N+x)),
    seen:reachable(N,land,objects,owner,startOf(owner)),maxStep:ground.step,maxSpan:FLAT_SPAN,flatRadius:FLAT_RADIUS});
  const objectsCleared=clearSite(N,objects,mask,site.x,site.y),levelled=levelSite(N,land,level,site.x,site.y);
  buildings.push(buildingRecord(site.x,site.y,TOOLSMITH,owner));
  toolsmiths.push({owner,type:TOOLSMITH,x:site.x,y:site.y,entrance:site.door,objectsCleared,
    ground:{levelled:ground.mode,height:levelled.height,originalSpan:levelled.span,groundSpan:site.groundSpan,maxSpan:FLAT_SPAN,flatRadius:FLAT_RADIUS,cells:levelled.cells,
      heightsChanged:levelled.changed,shadingsChanged:levelled.shaded,step:site.step,authoredStep:ground.step,authoredBuildings:ground.buildings,searchRing:site.ring}});
}
const toolBlocked=footprintCells(N,Buffer.concat(buildings),maskOf,1);
for(const owner of [2,3,4])supply[owner].tools=placeToolsmithPiles({N,land,objects,owner,byte7,blocked:toolBlocked,units:unitSet,piles,
  seen:reachable(N,land,objects,owner,startOf(owner))},toolsmiths.find(t=>t.owner===owner).entrance);
const addedPiles=[2,3,4].flatMap(o=>[...supply[o].weapons,...supply[o].shovels]).concat([2,3,4].flatMap(o=>supply[o].tools));
// Clean start (r31): no Dark Mushroom Farm inside the fire field of a Dark Temple/Fortress (citadel-fire.cjs, FarmLock 33).
// The source has no Dark farm; the same rule as the other scenarios runs anyway.
const fire=removeFarmsInFire({N,buildings,objects,farmMask:masks[FARM],farmCore:masks._raceBlocks['3'][FARM],maskOf});
buildings.splice(0,buildings.length,...fire.kept);
changes.set(7,{data:allUnits,count:allUnits.length/6});
changes.set(8,{data:Buffer.concat(buildings),count:buildings.length});
changes.set(9,{data:Buffer.concat([sourceGoods,...addedPiles]),count:sourceGoods.length/8+addedPiles.length});
changes.set(6,{data:objects});
changes.set(13,{data:land});
const script=fs.readFileSync(path.join(project,'scenario/DarkTribe_Alliance.txt'));
changes.set(16,{data:Buffer.concat([script,Buffer.from([0])])});
const briefing='DARK TRIBE - THE ALLIANCE\r\n\r\nRomans in the south-west, Vikings in the north-east and Mayas in the heart of the land have sworn to drive the Dark Tribe out. Your Temple stands alone between them, your army waits in the south-east, and not a single Mushroom Farm grows yet.\r\n\r\nBuild farms on the Dark land around you before the allies strike. Destroy every tower of all three peoples to win; if every Temple and Fortress falls, the Dark Tribe is lost.\0';
const tips='Farms cost 25 mana, Temples 100, Fortresses 150, Halls 120. Warriors/Throwers cost 15, Gardeners 5, Shamans 10. Manacopters cost 300. You already have your one Temple; you can add one Fortress.\0';
for(const t of [11,14])changes.set(t,{data:Buffer.from(briefing,'latin1')});
for(const t of [12,15])changes.set(t,{data:Buffer.from(tips,'latin1')});
const out=path.join(project,'work/generated/Map/User/DarkTribe_Alliance.map');fs.mkdirSync(path.dirname(out),{recursive:true});fs.writeFileSync(out,writeMap(map,changes));
const checked=readMap(out);
assertNoFarmInFire(checked.chunks.find(c=>c.type===8).data,'The Alliance');
for(const c of checked.chunks) {
  const original=map.chunks.find(o=>o.type===c.type);
  if(!changes.has(c.type))assert(c.packed.equals(original.packed),'Unchanged chunk changed');
}
const count=(list,owner)=>list.filter(r=>r[5]===owner).length;
fs.writeFileSync(path.join(project,'research/alliance-build.json'),JSON.stringify({version:'0.2.15',scenario:'alliance',source,source_sha256:SOURCE_SHA256,output:out,
  output_sha256:crypto.createHash('sha256').update(checked.raw).digest('hex'),changedChunks:[...changes.keys()],players:PLAYERS,
  darkUnits:count(units,1),darkBuildings:count(buildings,1),romanBuildings:count(buildings,2),vikingBuildings:count(buildings,3),mayaBuildings:count(buildings,4),
  reinforcedTowers:reinforced,fieldArmy:army.length,gardeners:gardeners.length,gardenerCells:Object.fromEntries([2,3,4].map(o=>[o,gardeners.filter(u=>u[5]===o).map(u=>[u.readUInt16LE(0),u.readUInt16LE(2)])])),
  barracks,toolsmiths,toolsmithPiles:Object.fromEntries([2,3,4].map(o=>[o,supply[o].tools.map(describePile)])),extraCarriers:carriers.length,extraCarrierCells:Object.fromEntries([2,3,4].map(o=>[o,carriers.filter(u=>u[5]===o).map(u=>[u.readUInt16LE(0),u.readUInt16LE(2)])])),
  sourceGoods:sourceGoods.length/8,weaponPiles:Object.fromEntries([2,3,4].map(o=>[o,supply[o].weapons.map(describePile)])),
  shovelPiles:Object.fromEntries([2,3,4].map(o=>[o,supply[o].shovels.map(describePile)])),fireCleanStart:{farmLock:FarmLock,removedFarms:fire.removed},runtimeVerified:false},null,2));
console.log(`Built The Alliance: Dark Tribe (${count(buildings,1)} building, ${count(units,1)} units) against Romans, Vikings and Mayas (${buildings.length-count(buildings,1)} buildings, ${reinforced} towers reinforced, ${army.length} field soldiers, ${gardeners.length} gardeners)`);
for(const [o,name] of [[2,'Romans'],[3,'Vikings'],[4,'Mayas']])console.log(`${name} gardeners (player ${o}): ${gardeners.filter(u=>u[5]===o).map(u=>`(${u.readUInt16LE(0)},${u.readUInt16LE(2)})`).join(' ')}`);
const pile=r=>`${r[4]}x${r[5]}@(${r.readUInt16LE(0)},${r.readUInt16LE(2)})`;
for(const [o,name] of [[2,'Romans'],[3,'Vikings'],[4,'Mayas']]) {
  const b=barracks.find(b=>b.owner===o);
  const g=b.ground;
  console.log(`${name} barracks (${b.x},${b.y}) entrance (${b.entrance}), ${b.objectsCleared} objects cleared, ${g.levelled} levelled at height ${g.height} `+
    `(span ${g.originalSpan}, ground span ${g.groundSpan}, ${g.heightsChanged} heights and ${g.shadingsChanged} shadings changed, step ${g.step} <= ${g.authoredStep}); weapons ${supply[o].weapons.map(pile).join(' ')}; shovels ${supply[o].shovels.map(pile).join(' ')}; ${EXTRA_CARRIERS} carriers`);
  const t=toolsmiths.find(t=>t.owner===o),tg=t.ground;
  console.log(`${name} toolsmith (${t.x},${t.y}) entrance (${t.entrance}), ${t.objectsCleared} objects cleared, ${tg.levelled} levelled at height ${tg.height} `+
    `(span ${tg.originalSpan}, ground span ${tg.groundSpan}, ${tg.heightsChanged} heights and ${tg.shadingsChanged} shadings changed, step ${tg.step} <= ${tg.authoredStep}); inputs ${supply[o].tools.map(pile).join(' ')}`);
}
console.log(`Clean start: ${fire.removed.length} Dark farm(s) inside a Temple/Fortress fire field removed (FarmLock ${FarmLock}) `+fire.removed.map(f=>`(${f.x},${f.y}) Euclid ${f.euclid} from (${f.citadel.x},${f.citadel.y})`).join(' '));

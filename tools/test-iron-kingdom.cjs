// Offline checks of the generated Iron Kingdom map against its source; no game, process or input access.
const assert=require('assert'),fs=require('fs'),path=require('path');
const {execFileSync}=require('child_process');
const {readMap}=require('./map-format.cjs');
const {gameDir,python}=require('./project-paths.cjs');
const {CLEARED_FLAGS}=require('./scenario-terrain.cjs');
const {farmsInFire,assertNoFarmInFire,assertFarmCleared,checkNative,buildingRows,FarmLock,FARM,BAKED_FLAGS}=require('./citadel-fire.cjs');
const {GOOD,TOOLSMITH,BARRACKS,TOOL_INPUTS,TOOL_TOTALS,steps,reachable,footprintCells,pileByte7,authoredGround,checkPiles,checkSite,checkLevel,checkLandscape}=require('./scenario-supply.cjs');
const root=path.resolve(__dirname,'..'),N=576;
const DARK_START=[378,134],VIKING_START=[475,353];
// Start supplies of the Viking kingdom (build-iron-kingdom.cjs): weapon and shovel piles at the start.
const SUPPLY=[GOOD.SWORD,GOOD.SWORD,GOOD.BOW,GOOD.BATTLEAXE,GOOD.SHOVEL,GOOD.SHOVEL];
// Toolsmith start buffer (r29): IRONBAR x16 and COAL x16 in piles of 8 beside the source toolsmith's entrance.
const map=readMap(path.join(root,'work/generated/Map/User/DarkTribe_IronKingdom.map'));
const source=readMap(path.join(gameDir(),'Map/Campaign/AO_trojan10.map'));
const firstDawn=readMap(path.join(root,'work/generated/Map/User/DarkTribe_FirstDawn.map'));
const masks=JSON.parse(execFileSync(python(),[path.join(__dirname,'scenario-footprints.py')],{encoding:'utf8',maxBuffer:1<<26}));
const chunk=(m,t)=>m.chunks.find(c=>c.type===t),get=(m,t)=>chunk(m,t).data;
const rows=(m,t,step)=>{const d=get(m,t),r=[];for(let i=0;i<d.length;i+=step)r.push(d.subarray(i,i+step));assert(chunk(m,t).header.readUInt32LE(20)===r.length,'Record count header');return r;};

// Two players exactly as the plugin and launcher helper expect (tools/scenarios.py, native/scenario-model.hpp).
const players=get(map,2);assert(players.length===90&&chunk(map,2).header.readUInt32LE(20)===2);
assert.deepStrictEqual([0,4,8,45,49,53].map(o=>players.readUInt32LE(o)),[3,378,134,1,475,353]);
assert(players.subarray(0,45).equals(get(source,2).subarray(90,135))&&players.subarray(45,90).equals(get(source,2).subarray(45,90)),'Player records copied unchanged');
assert(get(map,1).readUInt32LE(4)===2&&get(map,1).readUInt32LE(12)===N,'Two players on a 576 map');
// Slot/team table identical to First Dawn's (human Dark player, computer Vikings).
assert(get(map,3).equals(get(firstDawn,3))&&chunk(map,3).header.readUInt32LE(20)===2,'Slot/team table as in First Dawn');

// Entities: Dark camps 3 and 4 become player 1 unchanged, Vikings stay player 2, Trojans are gone.
// Clean start (r31, citadel-fire.cjs): the Dark farms inside a Temple fire field (FarmLock 33) of the merged camps are gone.
const owners={1:0,2:2,3:1,4:1};
const sourceMapped=rows(source,8,20).filter(r=>owners[r[5]]).map(r=>{const c=Buffer.from(r);c[5]=owners[r[5]];return c;});
const removedFarms=farmsInFire(sourceMapped);
assert.deepStrictEqual(removedFarms.map(f=>[f.x,f.y,f.citadel.x,f.citadel.y]),[[391,124,387,133],[413,161,387,133]],'Iron Kingdom removes exactly the two farms by the northern Temple');
const removedIndex=new Set(removedFarms.map(f=>f.index));
for(const [t,step] of [[7,6],[8,20]]) {
  const before=rows(source,t,step).filter(r=>owners[r[5]]).filter((r,i)=>t!==8||!removedIndex.has(i)),after=rows(map,t,step);
  assert(after.every(r=>r[5]===1||r[5]===2),'Only players 1 and 2');
  // Building garrison bytes and the position of the one relocated Viking unit (outside its land) may differ.
  const outside=r=>t===7&&r[5]===2&&get(source,6)[4*(r.readUInt16LE(2)*N+r.readUInt16LE(0))+1]!==2;
  const strip=(r,o,moved)=>{const c=Buffer.from(r);c[5]=o;if(t===8)c.fill(0,6,20);if(moved)c.fill(0,0,4);return c.toString('hex');};
  const moved=before.map(outside);assert(moved.filter(Boolean).length===(t===7?1:0),'One relocated Viking priest');
  const expected=before.map((r,i)=>strip(r,owners[r[5]],moved[i])),kept=after.slice(0,before.length).map((r,i)=>strip(r,r[5],moved[i]));
  assert.deepStrictEqual(kept,expected,'Source entities kept in place, type and order');
}
const units=rows(map,7,6),buildings=rows(map,8,20);
const count=(list,owner,types)=>list.filter(r=>r[5]===owner&&(!types||types.includes(r[4]))).length;
assert(count(buildings,1)===12&&count(buildings,1,[50])===2&&count(buildings,1,[49])===9&&count(buildings,1,[80])===1,'Merged Dark base');
checkNative();assertNoFarmInFire(get(map,8),'Iron Kingdom');
const removedCells=new Set(removedFarms.flatMap(f=>masks[FARM].map(([dx,dy])=>(f.y+dy)*N+f.x+dx)));
for(const f of removedFarms)assertFarmCleared({N,objects:get(map,6),farm:[f.x,f.y],farmMask:masks[FARM]});
// Broken starts: a kept Dark farm moved into a fire field (Euclid 32 on the axis from a Temple centre) or a removed farm
// put back are rejected; Euclid 33 is allowed.
{
  const temple=buildingRows(get(map,8)).find(r=>r.owner===1&&r.type===50),aFarm=buildingRows(get(map,8)).find(r=>r.owner===1&&r.type===FARM);
  const farmAt=(x,y)=>{const d=Buffer.from(get(map,8));d.writeUInt16LE(x,20*aFarm.index);d.writeUInt16LE(y,20*aFarm.index+2);return d;};
  assert.throws(()=>assertNoFarmInFire(farmAt(temple.x+1+32,temple.y+2)),/fire field/);
  for(const f of removedFarms)assert.throws(()=>assertNoFarmInFire(farmAt(f.x,f.y)),/fire field/);
  assertNoFarmInFire(farmAt(temple.x+1+FarmLock,temple.y+2));
}
assert(count(units,1)===156&&count(buildings,2)===83,'Dark units and Viking kingdom with its added barracks');
// Garrisons: every Viking tower manned (bytes 7..15 count soldier types 29..37).
for(const b of buildings)if(b[5]===2&&[46,47].includes(b[4]))
  assert(b[6]===1&&b[7]===(b[4]===46?2:3)&&b[10]===(b[4]===46?1:3),'Viking tower garrison');
// Field army: 24 soldiers appended after the source units, then 8 gardeners (settler type 50).
const base=rows(source,7,6).filter(r=>owners[r[5]]).length,army=units.slice(base,base+24),gardeners=units.slice(base+24);
assert(units.length===base+24+8&&army.every(u=>u[5]===2&&[29,30,32].includes(u[4])),'Viking field army');
assert(gardeners.length===8&&gardeners.every(u=>u[5]===2&&u[4]===50),'Viking gardeners');

// Land: Trojan land released; Viking land unchanged. Only the added barracks site (footprint and one-cell halo)
// may lose objects and their flags (0x43), and only its levelled cells may change height: every authored non-mine
// building of AO_trojan10 stands with its whole footprint level at its hotspot height and steps of up to 12 next to
// it, so the barracks footprint is levelled the same way. The slope shading (landscape byte 2 bits 0..3) follows
// the new heights on the cells above and below them, as it follows the heights on every cell of the source.
// Ground type, flag bits and byte 3 stay.
const objects=get(map,6),sourceObjects=get(source,6);
const sourceBuildingCount=sourceMapped.length-removedFarms.length,addedBuildings=buildings.slice(sourceBuildingCount);
const maskOf=(t,o)=>o===2?masks._viking[t]:masks[t],blockOf=(t,o)=>masks._raceBlocks[o===2?1:3][t];
const ground=authoredGround(N,get(source,13),Buffer.concat(sourceMapped),maskOf,blockOf); // the builder levels against every authored building, removed farms included
assert(ground.mode==='footprint'&&ground.step===12&&ground.buildings===84,'Authored ground: level footprints, steps up to 12');
const levelOf=(t,o)=>ground.levelOf(maskOf(t,o),blockOf(t,o)),barracksLevel=levelOf(BARRACKS,2);
const levelled=footprintCells(N,Buffer.concat(addedBuildings),levelOf,0);
const landscape=checkLandscape(N,get(map,13),get(source,13),levelled);
assert(landscape.heights>0&&landscape.shading>0,'Barracks ground levelled and shaded');
const prepared=footprintCells(N,Buffer.concat(addedBuildings),(t,o)=>masks._viking[t],1);
for(let at=0;at<objects.length;at+=4) {
  const was=sourceObjects[at+1];
  assert(objects[at+1]===(was===1?0:was),'Initial owner remap');
  assert(objects[at+3]===sourceObjects[at+3],'Resources unchanged');
  if(prepared.has(at/4))assert(objects[at]===0&&objects[at+2]===(sourceObjects[at+2]&~CLEARED_FLAGS),'Barracks site cleared only of objects and their flags');
  else if(removedCells.has(at/4))assert(objects[at]===sourceObjects[at]&&objects[at+2]===(sourceObjects[at+2]&~BAKED_FLAGS),'Removed farm cleared only of its baked flags');
  else for(const o of [0,2])assert(objects[at+o]===sourceObjects[at+o],'Objects and flags unchanged');
}
// Every Viking entrance and unit on owned land (native economies need it), soldiers off footprints.
const footprint=new Set();
for(const b of buildings) {
  const x=b.readUInt16LE(0),y=b.readUInt16LE(2),cells=b[5]===2?masks._viking[b[4]]:masks[b[4]];
  for(const [dx,dy] of cells)footprint.add((y+dy)*N+x+dx);
  if(b[5]===2) {
    const [dx,dy]=masks._vikingDoors[b[4]];assert(objects[4*((y+dy)*N+x+dx)+1]===2,'Viking entrance outside its land');
  } else assert(land(x,y)&64,'Dark building on Dark land');
}
function land(x,y){return get(map,13)[4*(y*N+x)+2];}
const cells=new Set();
for(const u of units) {
  const k=u.readUInt16LE(2)*N+u.readUInt16LE(0);
  if(u[5]===2)assert(objects[4*k+1]===2,'Viking unit outside its land');
  if(army.includes(u)||gardeners.includes(u)){assert(!footprint.has(k)&&!cells.has(k)&&!objects[4*k]&&!(objects[4*k+2]&0x5b)&&!(get(map,13)[4*k+2]&64),'Soldier or gardener on a blocked or Dark cell');}
  cells.add(k);
}
// Goods: the Trojan starting goods are removed, the Viking goods kept as the unchanged prefix.
const goods=rows(map,9,8),sourceVikingGoods=rows(source,9,8).filter(g=>get(source,6)[4*(g.readUInt16LE(2)*N+g.readUInt16LE(0))+1]===2);
assert(goods.length===16+SUPPLY.length+TOOL_INPUTS.length&&goods.every(g=>objects[4*(g.readUInt16LE(2)*N+g.readUInt16LE(0))+1]===2),'Viking goods only');
assert(sourceVikingGoods.length===16&&goods.slice(0,16).every((g,i)=>g.equals(sourceVikingGoods[i])),'Source Viking goods kept as the prefix');

// Start supplies for new armies: one Viking barracks appended after the source buildings, on clear owned
// non-Dark grass with a reachable entrance and its footprint level (and shaded) at the authored hotspot height; sword, bow,
// battleaxe and shovel piles at the start, after the source goods, on free owned non-Dark grass off every footprint
// and unit. Mutations below must fail here.
function verifySupply(m) {
  const all=rows(m,8,20),added=all.slice(sourceBuildingCount);
  assert(added.length===1&&added[0][4]===BARRACKS&&added[0][5]===2,'One added Viking barracks');
  assert(all.filter(b=>b[5]===2&&b[4]===BARRACKS).length===1,'Viking barracks');
  const site=checkSite({N,land:get(m,13),objects:get(m,6),record:added[0],mask:masks._viking[BARRACKS],door:masks._vikingDoors[BARRACKS],
    others:Buffer.concat(all.slice(0,sourceBuildingCount)),maskOf,units:get(m,7),goods:get(m,9),start:VIKING_START});
  site.ground=checkLevel({N,land:get(m,13),sourceLand:get(source,13),record:added[0],level:barracksLevel,maxStep:ground.step});
  const piles=checkPiles({N,land:get(m,13),objects:get(m,6),goods:get(m,9),from:16,byte7:pileByte7(get(source,9)),buildings:get(m,8),maskOf,
    units:get(m,7),starts:{2:VIKING_START},expected:{2:[...SUPPLY,...TOOL_INPUTS]},totals:{2:TOOL_TOTALS}})[2];
  // The kingdom's one toolsmith is the source record (unchanged above), its entrance owned and reachable; the iron bar
  // and coal piles lie within 16 steps of that entrance.
  const smiths=all.filter(b=>b[5]===2&&b[4]===TOOLSMITH);assert(smiths.length===1,'One Viking toolsmith');
  const [dx,dy]=masks._vikingDoors[TOOLSMITH],toolsmith={x:smiths[0].readUInt16LE(0),y:smiths[0].readUInt16LE(2)};
  toolsmith.entrance=[toolsmith.x+dx,toolsmith.y+dy];
  const door=toolsmith.entrance[1]*N+toolsmith.entrance[0];
  assert(get(m,6)[4*door+1]===2&&reachable(N,get(m,13),get(m,6),2,VIKING_START)[door],'Toolsmith entrance not owned or unreachable');
  for(const p of piles)if(TOOL_INPUTS.includes(p.good))assert(steps(...toolsmith.entrance,p.x,p.y)<=16,'Toolsmith input away from its entrance');
  return {barracks:site,toolsmith,piles};
}
const supply=verifySupply(map);
let rejected=0;
function mutated(type,mutate,expected) {
  const data=Buffer.from(get(map,type));mutate(data);
  const check=()=>verifySupply({...map,chunks:map.chunks.map(c=>c.type===type?{...c,data}:c)});
  if(expected)assert.throws(check,expected);else assert.throws(check);
  rejected++;
}
const goodsData=get(map,9),addedPile=good=>{for(let i=8*16;i<goodsData.length;i+=8)if(goodsData[i+4]===good)return i;};
mutated(8,d=>{d[d.length-20+4]=1;});                                                         // barracks missing
mutated(9,d=>{const i=addedPile(GOOD.SWORD);d.writeUInt16LE(DARK_START[0],i);d.writeUInt16LE(DARK_START[1],i+2);}); // pile on the Dark start
mutated(13,d=>{const i=addedPile(GOOD.SWORD);d[4*(goodsData.readUInt16LE(i+2)*N+goodsData.readUInt16LE(i))+2]|=64;}); // land under a pile turned Dark
mutated(9,d=>{const i=addedPile(GOOD.BOW);d.writeUInt16LE(supply.barracks.x,i);d.writeUInt16LE(supply.barracks.y,i+2);}); // pile on the barracks
mutated(9,d=>{d[addedPile(GOOD.SHOVEL)+4]=GOOD.SWORD;});                                       // wrong goods
mutated(9,d=>{d[addedPile(GOOD.COAL)+4]=GOOD.SWORD;});                                         // coal replaced
mutated(9,d=>{d[addedPile(GOOD.IRONBAR)+5]=4;});                                               // iron bars short
mutated(9,d=>{const i=addedPile(GOOD.IRONBAR),j=addedPile(GOOD.SHOVEL);d.writeUInt16LE(goodsData.readUInt16LE(j)+1,i);d.writeUInt16LE(goodsData.readUInt16LE(j+2)+3,i+2);}); // iron bars at the start
mutated(8,d=>{for(let i=0;i<d.length;i+=20)if(d[i+5]===2&&d[i+4]===TOOLSMITH){d[i+4]=1;break;}}); // toolsmith missing
mutated(13,d=>{const x=supply.barracks.x,y=supply.barracks.y;d[4*(y*N+x)+2]|=64;});             // barracks on Dark land
mutated(6,d=>{const [x,y]=supply.barracks.entrance;d[4*(y*N+x)+1]=0;});                        // entrance not owned
// Unlevelled barracks: the source heights back, one levelled cell raised, a cliff next to the footprint, the source
// shading kept on the levelled ground.
const barracksCells=[...footprintCells(N,Buffer.concat(addedBuildings),levelOf,0)],hotspot=supply.barracks.y*N+supply.barracks.x;
const levelCell=barracksCells.find(k=>k!==hotspot),outside=barracksCells.map(k=>k+1).find(k=>!levelled.has(k));
mutated(13,d=>{get(source,13).copy(d);},/not level/);                                           // barracks on unlevelled ground
mutated(13,d=>{d[4*levelCell]+=1;},/not level/);                                                // one cell above the rest
mutated(13,d=>{d[4*outside]=supply.barracks.ground.height+ground.step+1;},/steeper/);           // cliff beside the footprint
mutated(13,d=>{for(let at=2;at<d.length;at+=4)d[at]=get(source,13)[at];},/Shading/);             // stale shading

// Dark placement with the native rule (masked flags == Dark bit 4, permitted terrain, no object),
// mask union plus halo (stricter than the native body check): all four buildings fit.
function reservation(c){const r=new Map();for(const [x,y]of c)for(let dy=-1;dy<=1;dy++)for(let dx=-1;dx<=1;dx++)r.set((y+dy)*N+x+dx,[x+dx,y+dy]);return [...r.values()];}
function accepts(c,x,y){return c.every(([dx,dy])=>{const a=x+dx,b=y+dy;if(a<1||b<1||a>=N-1||b>=N-1)return false;const at=4*(b*N+a),l=get(map,13),flags=objects[at+2]|((l[at+2]&64)?4:0),t=l[at+1];
  return (flags&0x5f)===4&&t!==21&&([16,64,144].includes(t&240)||t===48)&&!!(l[at+2]&64)&&t>=16&&t<96&&!objects[at];});}
const placement={};
for(const type of [49,50,51,80]) {
  const c=reservation(masks[type]);let n=0;
  for(let y=1;y<N-1;y++)for(let x=1;x<N-1;x++)if((land(x,y)&64)&&accepts(c,x,y))n++;
  placement[type]=n;assert(n>=150,'Too little open Dark land for building '+type);
}
// Script and texts.
assert(get(map,16).subarray(0,-1).equals(fs.readFileSync(path.join(root,'scenario/DarkTribe_IronKingdom.txt')))&&get(map,16).at(-1)===0);
for(const t of [11,14])assert(get(map,t).toString('latin1').startsWith('DARK TRIBE - IRON KINGDOM'));
// Every other chunk byte-identical to the source.
for(const c of map.chunks)if(![1,2,3,6,7,8,9,11,12,13,14,15,16].includes(c.type))assert(c.packed.equals(source.chunks.find(o=>o.type===c.type).packed),'Unexpected chunk change '+c.type);
fs.writeFileSync(path.join(root,'research/iron-kingdom-checks.json'),JSON.stringify({version:'0.2.15',game_started:false,live_process_read:false,
  players:2,darkBuildings:count(buildings,1),darkUnits:156,fireCleanStart:{farmLock:FarmLock,removedFarms},vikingBuildings:83,vikingUnits:count(units,2),vikingGardeners:gardeners.length,vikingGoods:goods.length,
  addedBarracks:supply.barracks,toolsmith:supply.toolsmith,authoredGround:{levelled:ground.mode,buildings:ground.buildings,step:ground.step},barracksHeightsChanged:landscape.heights,
  barracksShadingsChanged:landscape.shading,
  supplyPiles:supply.piles,rejectedSupplyMutations:rejected,darkPlacementAnchors:placement,runtimeVerified:false},null,2));
console.log(`PASS: Iron Kingdom has two players, merged Dark camps, manned Viking towers, owned Viking entrances, 24 field soldiers and 8 gardeners on free Viking cells, an added Viking barracks on level ground (footprint at height ${supply.barracks.ground.height}, ${landscape.heights} heights and ${landscape.shading} shadings changed, steps <= ${ground.step}) with weapon and shovel piles, 16 iron bars and 16 coal beside the toolsmith entrance (${supply.toolsmith.entrance}) (${rejected} broken supplies rejected), no Trojans, no Dark farm within FarmLock ${FarmLock} of a Temple fire centre (${removedFarms.length} removed with their flags, broken starts rejected) and room for all four Dark buildings`,JSON.stringify(placement));

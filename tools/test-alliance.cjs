// Offline checks of the generated Alliance map against its source; no game, process or input access.
const assert=require('assert'),fs=require('fs'),path=require('path');
const {execFileSync}=require('child_process');
const {readMap}=require('./map-format.cjs');
const {gameDir,python}=require('./project-paths.cjs');
const {CLEARED_FLAGS}=require('./scenario-terrain.cjs');
const {farmsInFire,assertNoFarmInFire,checkNative,buildingRows,FarmLock,FARM}=require('./citadel-fire.cjs');
const {buildingRecord}=require('./scenario-supply.cjs');
const {GOOD,TOOLSMITH,BARRACKS,TOOL_INPUTS,TOOL_TOTALS,TOOL_GOODS,FLAT_SPAN,FLAT_RADIUS,SHADE,shadeOf,footprintCells,pileByte7,authoredGround,levelSite,siteFor,checkPiles,checkSite,checkLevel,checkLandscape,
  checkCarriers}=require('./scenario-supply.cjs');
const root=path.resolve(__dirname,'..'),N=512;
const map=readMap(path.join(root,'work/generated/Map/User/DarkTribe_Alliance.map'));
const source=readMap(path.join(gameDir(),'Map/Campaign/dark07.map'));
const masks=JSON.parse(execFileSync(python(),[path.join(__dirname,'scenario-footprints.py')],{encoding:'utf8',maxBuffer:1<<26}));
const chunk=(m,t)=>m.chunks.find(c=>c.type===t),get=(m,t)=>chunk(m,t).data;
const rows=(m,t,step)=>{const d=get(m,t),r=[];for(let i=0;i<d.length;i+=step)r.push(d.subarray(i,i+step));assert(chunk(m,t).header.readUInt32LE(20)===r.length,'Record count header');return r;};
const OWNER={1:4,2:2,3:3,4:1},PLAYERS=[[3,303,423],[0,98,353],[1,367,116],[2,260,263]];
// Start supplies of the three peoples (build-alliance.cjs): weapons beside each barracks entrance, shovels at the start.
const WEAPONS={2:[GOOD.SWORD,GOOD.SWORD,GOOD.BOW,GOOD.BOW],3:[GOOD.SWORD,GOOD.SWORD,GOOD.BOW,GOOD.BATTLEAXE],4:[GOOD.SWORD,GOOD.SWORD,GOOD.BOW,GOOD.BLOWGUN]};
const SHOVELS=[GOOD.SHOVEL,GOOD.SHOVEL],EXTRA_CARRIERS=40;
// Toolsmith inputs of each people (r29): IRONBAR x16 and COAL x16 in piles of 8, one pile of 2 HAMMERs.
const TOOL_PILES=[...TOOL_INPUTS,GOOD.HAMMER];
const maskOf=(type,owner)=>masks._races[PLAYERS[owner-1][0]][type],startOf=owner=>PLAYERS[owner-1].slice(1);
const blockOf=(type,owner)=>masks._raceBlocks[PLAYERS[owner-1][0]][type];

// Four players as the plugin and launcher helper expect (tools/scenarios.py, native/scenario-model.hpp).
const players=get(map,2);assert(players.length===180&&chunk(map,2).header.readUInt32LE(20)===4);
PLAYERS.forEach(([race,x,y],i)=>assert.deepStrictEqual([0,4,8].map(o=>players.readUInt32LE(45*i+o)),[race,x,y]));
for(const [from,to] of Object.entries(OWNER))assert(players.subarray(45*(to-1),45*to).equals(get(source,2).subarray(45*(from-1),45*from)),'Player records moved unchanged');
assert(get(map,1).equals(get(source,1))&&get(map,1).readUInt32LE(4)===4&&get(map,1).readUInt32LE(12)===N,'Map info unchanged: four players, 512');
// Slot/team table: human Dark player 1 in team 1, three computer players in team 2.
assert(get(map,3).equals(Buffer.concat([Buffer.alloc(33),Buffer.from([0,1,1,2,1,2,1,2])]))&&chunk(map,3).header.readUInt32LE(20)===4);

// Entities: every source entity kept in place and type with its new owner; only garrisons grow.
for(const [t,step] of [[7,6],[8,20]]) {
  const before=rows(source,t,step),after=rows(map,t,step);
  const strip=(r,o)=>{const c=Buffer.from(r);c[5]=o;if(t===8)c.fill(0,6,20);return c.toString('hex');};
  assert.deepStrictEqual(after.slice(0,before.length).map(r=>strip(r,r[5])),before.map(r=>strip(r,OWNER[r[5]])),'Source entities kept in place, type and order');
}
const units=rows(map,7,6),buildings=rows(map,8,20),sourceBuildings=rows(source,8,20);
const count=(list,owner,types)=>list.filter(r=>r[5]===owner&&(!types||types.includes(r[4]))).length;
assert(count(buildings,1)===1&&count(buildings,1,[50])===1&&count(units,1)===109,'Dark Tribe: one Temple and the army');
// Clean start (r31, citadel-fire.cjs): no Dark farm within FarmLock 33 of the Temple fire centre (the source has none).
// Broken starts: a farm added beside the Temple (Euclid 20, or 32 on the axis) is rejected; one at Euclid 33 is allowed.
checkNative();assertNoFarmInFire(get(map,8),'The Alliance');assert(!farmsInFire(get(source,8)).length&&count(buildings,1,[FARM])===0,'No start farm');
{
  const temple=buildingRows(get(map,8)).find(r=>r.owner===1&&r.type===50),withFarm=(x,y)=>Buffer.concat([get(map,8),buildingRecord(x,y,FARM,1)]);
  assert.throws(()=>assertNoFarmInFire(withFarm(temple.x+1,temple.y+2-20)),/fire field/);
  assert.throws(()=>assertNoFarmInFire(withFarm(temple.x+1-32,temple.y+2)),/fire field/);
  assertNoFarmInFire(withFarm(temple.x+1-FarmLock,temple.y+2));
}
assert(count(buildings,2)===15&&count(buildings,3)===16&&count(buildings,4)===11,'Romans, Vikings, Mayas, each with an added barracks and toolsmith');
// Garrisons: at least 2 swordsmen + 1 bowman (small) or 3 + 3 (large); authored soldiers never removed.
buildings.slice(0,sourceBuildings.length).forEach((b,i)=>{
  const s=sourceBuildings[i];for(let k=7;k<20;k++)assert(b[k]>=s[k],'Garrison reduced');
  if([46,47].includes(b[4])&&b[5]!==1)assert(b[6]===1&&b[7]+b[8]+b[9]>=(b[4]===46?2:3)&&b[10]+b[11]+b[12]>=(b[4]===46?1:3),'Tower garrison');
  else assert(b.subarray(6).equals(s.subarray(6)),'Only towers change');
});
// Field armies: 12 soldiers per computer people appended after the source units, then 4 gardeners (type 50) each,
// then 40 carriers each (checked with the supplies below).
const base=rows(source,7,6).length,army=units.slice(base,base+36),gardeners=units.slice(base+36,base+48);
assert(units.length===base+48+3*EXTRA_CARRIERS,'Units after the field armies and gardeners');
assert(gardeners.length===12&&[2,3,4].every(o=>gardeners.filter(u=>u[5]===o&&u[4]===50).length===4),'Gardeners of the three peoples');
assert(army.length===36&&[2,3,4].every(o=>army.filter(u=>u[5]===o).length===12)&&army.every(u=>[29,30,32].includes(u[4])),'Field armies');

// Land: initial owners follow the players. Only the added barracks sites (footprint and one-cell halo) lose their
// objects and the flags tied to them (0x43), and only their levelled cells may change height: every authored
// non-mine building of dark07 stands with its blocked core level at its hotspot height (the rest of the footprint
// as the land falls) and steps of up to 11 next to the core, so each barracks core is levelled the same way. The
// slope shading (landscape byte 2 bits 0..3) follows the new heights on the cells above and below them, as it follows
// the heights on every cell of the source. Ground type, flag bits and byte 3 stay.
const objects=get(map,6),sourceObjects=get(source,6);
const addedBuildings=buildings.slice(sourceBuildings.length),prepared=footprintCells(N,Buffer.concat(addedBuildings),maskOf,1);
const ground=authoredGround(N,get(source,13),Buffer.concat(buildings.slice(0,sourceBuildings.length)),maskOf,blockOf);
assert(ground.mode==='core'&&ground.step===11&&ground.buildings===37,'Authored ground: level blocked cores, steps up to 11');
const levelOf=(t,o)=>ground.levelOf(maskOf(t,o),blockOf(t,o)),levelled=footprintCells(N,Buffer.concat(addedBuildings),levelOf,0);
const landscape=checkLandscape(N,get(map,13),get(source,13),levelled);
assert(landscape.heights>0&&landscape.shading>0,'Barracks ground levelled and shaded');
for(let at=0;at<objects.length;at+=4) {
  const was=sourceObjects[at+1];assert(objects[at+1]===(was?OWNER[was]:0),'Initial owner remap');
  assert(objects[at+3]===sourceObjects[at+3],'Resources unchanged');
  if(prepared.has(at/4))assert(objects[at]===0&&objects[at+2]===(sourceObjects[at+2]&~CLEARED_FLAGS),'Barracks site cleared only of objects and their flags');
  else for(const o of [0,2])assert(objects[at+o]===sourceObjects[at+o],'Objects and flags unchanged');
}
// Every computer entrance and unit on its own land (native economies need it), soldiers off footprints.
const footprint=new Set(),land=(x,y)=>get(map,13)[4*(y*N+x)+2];
for(const b of buildings) {
  const x=b.readUInt16LE(0),y=b.readUInt16LE(2),race=PLAYERS[b[5]-1][0];
  for(const [dx,dy] of masks._races[race][b[4]])footprint.add((y+dy)*N+x+dx);
  if(b[5]===1){assert(land(x,y)&64,'Dark building on Dark land');continue;}
  const [dx,dy]=masks._raceDoors[race][b[4]];assert(objects[4*((y+dy)*N+x+dx)+1]===b[5],'Entrance outside its land');
}
const cells=new Set();
for(const u of units) {
  const k=u.readUInt16LE(2)*N+u.readUInt16LE(0);assert(objects[4*k+1]===(u[5]===1?0:u[5]),'Unit outside its land');
  if(army.includes(u)||gardeners.includes(u))assert(!footprint.has(k)&&!cells.has(k)&&!objects[4*k]&&!(objects[4*k+2]&0x5b)&&!(land(u.readUInt16LE(0),u.readUInt16LE(2))&64),'Soldier or gardener on a blocked or Dark cell');
  cells.add(k);
}
// Goods stay on the land of the peoples they belong to; the source piles are the unchanged prefix.
const goods=rows(map,9,8),sourceGoods=get(source,9);
assert(goods.length===71+18+3*TOOL_PILES.length&&goods.every(g=>[2,3,4].includes(objects[4*(g.readUInt16LE(2)*N+g.readUInt16LE(0))+1])),'Goods of the three peoples');
assert(sourceGoods.length===71*8&&get(map,9).subarray(0,sourceGoods.length).equals(sourceGoods),'Source goods kept as the prefix');

// Start supplies for new armies: per people one barracks of its race appended after the source buildings (clear
// owned non-Dark grass, entrance reachable from the start, blocked core level at the authored hotspot height with
// no step above the authored ones next to it), weapon piles beside that entrance and two shovel piles
// at the start after the source goods, and 40 carriers after the gardeners. Mutations below must fail here.
function verifySupply(m) {
  const all=rows(m,8,20),added=all.slice(sourceBuildings.length);
  assert(added.length===6&&added.every((b,i)=>b[4]===(i<3?BARRACKS:TOOLSMITH)&&b[5]===i%3+2),'One added barracks and one added toolsmith per people');
  const result={barracks:{},toolsmiths:{},piles:null,carriers:null},land=get(m,13),objects=get(m,6);
  for(const b of added) {
    const race=PLAYERS[b[5]-1][0],type=b[4],into=type===BARRACKS?result.barracks:result.toolsmiths;
    assert(all.filter(r=>r[5]===b[5]&&r[4]===type).length===1,'Building '+type+' of player '+b[5]);
    // Apart from every other building, the other added ones included.
    const others=Buffer.concat(all.filter(r=>r!==b));
    into[b[5]]=checkSite({N,land,objects,record:b,mask:masks._races[race][type],door:masks._raceDoors[race][type],
      others,maskOf,units:get(m,7),goods:get(m,9),start:startOf(b[5])});
    into[b[5]].ground=checkLevel({N,land,sourceLand:get(source,13),record:b,level:levelOf(type,b[5]),maxStep:ground.step});
  }
  result.piles=checkPiles({N,land,objects,goods:get(m,9),from:71,byte7:pileByte7(sourceGoods),buildings:get(m,8),maskOf,units:get(m,7),
    starts:{2:startOf(2),3:startOf(3),4:startOf(4)},expected:Object.fromEntries([2,3,4].map(o=>[o,[...WEAPONS[o],...SHOVELS,...TOOL_PILES]])),
    totals:Object.fromEntries([2,3,4].map(o=>[o,{...TOOL_TOTALS,[GOOD.HAMMER]:2}]))});
  for(const o of [2,3,4])for(const p of result.piles[o]) {
    const [ax,ay]=p.good===GOOD.SHOVEL?startOf(o):TOOL_GOODS.has(p.good)?result.toolsmiths[o].entrance:result.barracks[o].entrance,dx=p.x-ax,dy=p.y-ay;
    assert(Math.max(Math.abs(dx),Math.abs(dy),Math.abs(dx-dy))<=16,'Supply pile away from its anchor');
  }
  result.carriers=checkCarriers({N,land,objects,units:get(m,7),from:base+48,owners:[2,3,4],count:EXTRA_CARRIERS,buildings:get(m,8),maskOf,goods:get(m,9),
    starts:{2:startOf(2),3:startOf(3),4:startOf(4)}});
  return result;
}
const supply=verifySupply(map);
let rejected=0;
function mutated(type,mutate,expected) {
  const data=Buffer.from(get(map,type));mutate(data);
  const check=()=>verifySupply({...map,chunks:map.chunks.map(c=>c.type===type?{...c,data}:c)});
  if(expected)assert.throws(check,expected);else assert.throws(check);
  rejected++;
}
const goodsData=get(map,9),addedPile=good=>{for(let i=8*71;i<goodsData.length;i+=8)if(goodsData[i+4]===good)return i;};
const darkStart=startOf(1);
mutated(8,d=>{d[d.length-80+4]=1;});                                                         // Maya barracks missing
mutated(8,d=>{d[d.length-100+5]=2;});                                                        // Viking barracks given to the Romans
mutated(8,d=>{d[d.length-20+4]=1;});                                                         // Maya toolsmith missing
mutated(8,d=>{d[d.length-40+5]=4;});                                                         // Viking toolsmith given to the Mayas
mutated(9,d=>{d[addedPile(GOOD.IRONBAR)+4]=GOOD.SWORD;});                                     // iron bars replaced
mutated(9,d=>{d[addedPile(GOOD.COAL)+5]=2;});                                                 // coal short
mutated(9,d=>{const i=addedPile(GOOD.HAMMER),j=addedPile(GOOD.SHOVEL);d.writeUInt16LE(goodsData.readUInt16LE(j)+1,i);d.writeUInt16LE(goodsData.readUInt16LE(j+2)+3,i+2);}); // hammers at the start
mutated(9,d=>{const i=addedPile(GOOD.SWORD);d.writeUInt16LE(darkStart[0],i);d.writeUInt16LE(darkStart[1],i+2);}); // pile on the Dark start
mutated(13,d=>{const i=addedPile(GOOD.SWORD);d[4*(goodsData.readUInt16LE(i+2)*N+goodsData.readUInt16LE(i))+2]|=64;}); // land under a pile turned Dark
mutated(9,d=>{const i=addedPile(GOOD.BLOWGUN),[x,y]=startOf(2);d.writeUInt16LE(x+4,i);d.writeUInt16LE(y+4,i+2);}); // Maya pile on Roman land
mutated(9,d=>{d[addedPile(GOOD.BATTLEAXE)+5]=9;});                                             // overfull pile
mutated(7,d=>{d[d.length-2]=29;});                                                           // a carrier replaced by a soldier
mutated(6,d=>{const [x,y]=supply.barracks[3].entrance;d[4*(y*N+x)+1]=0;});                     // entrance not owned
// Unlevelled barracks: the source heights back, one levelled Maya cell raised, a cliff next to the Roman core, the
// source shading kept on the levelled ground.
const coreOf=o=>{const b=supply.barracks[o];return [...footprintCells(N,addedBuildings.find(r=>r[5]===o),levelOf,0)].filter(k=>k!==b.y*N+b.x);};
const levelCell=coreOf(4)[0],outside=coreOf(2).map(k=>k+1).find(k=>!levelled.has(k));
mutated(13,d=>{get(source,13).copy(d);},/not level/);                                           // barracks on unlevelled ground
mutated(13,d=>{d[4*levelCell]+=1;},/not level/);                                                // one cell above the rest
mutated(13,d=>{d[4*outside]=supply.barracks[2].ground.height+ground.step+1;},/steeper/);        // cliff beside the core
mutated(13,d=>{for(let at=2;at<d.length;at+=4)d[at]=get(source,13)[at];},/Shading/);             // stale shading

// siteFor's ground rules on a synthetic 40x40 owned grass map (3x3 building, levelled whole, entrance at its hotspot,
// shading derived from the heights). A spike of FLAT_SPAN+2 inside the centre site makes its ground too steep: the
// first site whose cells and ring miss the spike (ring 2) wins. A spike only in the centre's ring (its cells flat)
// does the same at ring 1, and a step bound alone also keeps sites off it. With flat ground only beyond FLAT_RADIUS
// the steep centre stays. levelSite then levels and reshades the centre, which checkLevel accepts and stale
// shading fails.
{
  const n=40,flatLand=Buffer.alloc(4*n*n),owned=Buffer.alloc(4*n*n),mask=[];
  for(let k=0;k<n*n;k++){flatLand[4*k+1]=28;owned[4*k+1]=2;}
  for(let dy=-1;dy<=1;dy++)for(let dx=-1;dx<=1;dx++)mask.push([dx,dy]);
  const shaded=land=>{for(let k=0;k<n*n;k++)land[4*k+2]=(land[4*k+2]&~SHADE)|shadeOf(n,land,k);return land;};
  const spiked=(x,y,h)=>{const land=Buffer.from(flatLand);land[4*(y*n+x)]=h;return shaded(land);};
  shaded(flatLand);
  const seen=new Uint8Array(n*n).fill(1),base={N:n,objects:owned,owner:2,mask,door:[0,0],near:[20,20],blocked:new Set(),units:new Set(),piles:new Set(),seen};
  const inside=spiked(21,21,FLAT_SPAN+2),beside=spiked(22,20,FLAT_SPAN+2);
  const steep=siteFor({...base,land:inside,maxSpan:255});
  assert(steep.x===20&&steep.y===20&&steep.span===FLAT_SPAN+2&&steep.groundSpan===FLAT_SPAN+2&&steep.step===0,'siteFor without a span bound keeps the centre');
  const flat=siteFor({...base,land:inside});
  assert(flat.ring===2&&flat.groundSpan===0&&FLAT_RADIUS>=2,'siteFor prefers flat ground nearby');
  const cliff=siteFor({...base,land:beside,maxSpan:255});
  assert(cliff.ring===0&&cliff.span===0&&cliff.groundSpan===FLAT_SPAN+2&&cliff.step===FLAT_SPAN+2,'Centre flat inside, next to the spike');
  const ring=siteFor({...base,land:beside});
  assert(ring.ring===1&&ring.groundSpan===0,'siteFor counts the ring around the levelled cells');
  const gentle=siteFor({...base,land:beside,maxSpan:255,maxStep:FLAT_SPAN+1});
  assert(gentle.ring===1&&gentle.step===0,'siteFor keeps the step bound');
  const far=Buffer.from(flatLand);for(let y=0;y<n;y++)for(let x=0;x<n;x++)far[4*(y*n+x)]=y<30?(x+y)%2*(FLAT_SPAN+1):0;shaded(far);
  const fallback=siteFor({...base,land:far,radius:15});
  assert(fallback.ring===0&&fallback.groundSpan===FLAT_SPAN+1,'siteFor keeps a steep site when no flatter one lies within FLAT_RADIUS');
  assert(FLAT_RADIUS<12&&siteFor({...base,land:far,radius:15,flatRadius:12}).ring===12,'Flat ground first found at ring 12');
  const record=Buffer.alloc(20);record.writeUInt16LE(20,0);record.writeUInt16LE(20,2);
  const levelled=levelSite(n,inside,mask,20,20),check=land=>checkLevel({N:n,land,sourceLand:flatLand,record,level:mask,maxStep:0});
  assert(levelled.changed===1&&levelled.shaded===2&&check(inside).step===0&&inside.equals(flatLand),'levelSite levels and reshades to the hotspot height');
  const stale=Buffer.from(inside);stale[4*(20*n+21)+2]=spiked(21,21,FLAT_SPAN+2)[4*(20*n+21)+2];
  assert.throws(()=>check(stale),/Shading/);
}
// Dark placement with the native rule on mask union plus halo (stricter than the native body check).
function reservation(c){const r=new Map();for(const [x,y]of c)for(let dy=-1;dy<=1;dy++)for(let dx=-1;dx<=1;dx++)r.set((y+dy)*N+x+dx,[x+dx,y+dy]);return [...r.values()];}
function accepts(c,x,y){return c.every(([dx,dy])=>{const a=x+dx,b=y+dy;if(a<1||b<1||a>=N-1||b>=N-1)return false;const at=4*(b*N+a),l=get(map,13),flags=objects[at+2]|((l[at+2]&64)?4:0),t=l[at+1];
  return (flags&0x5f)===4&&t!==21&&([16,64,144].includes(t&240)||t===48)&&!!(l[at+2]&64)&&t>=16&&t<96&&!objects[at];});}
const placement={};
for(const type of [49,50,51,80]) {
  const c=reservation(masks[type]);let n=0;
  for(let y=1;y<N-1;y++)for(let x=1;x<N-1;x++)if((land(x,y)&64)&&accepts(c,x,y))n++;
  placement[type]=n;
}
assert(placement[49]>=2000&&placement[51]>=10&&placement[80]>=10,'Room for Farms, a Fortress and Halls');
// Script and texts.
assert(get(map,16).subarray(0,-1).equals(fs.readFileSync(path.join(root,'scenario/DarkTribe_Alliance.txt')))&&get(map,16).at(-1)===0);
for(const t of [11,14])assert(get(map,t).toString('latin1').startsWith('DARK TRIBE - THE ALLIANCE'));
// Every other chunk byte-identical to the source.
for(const c of map.chunks)if(![2,3,6,7,8,9,11,12,13,14,15,16].includes(c.type))assert(c.packed.equals(source.chunks.find(o=>o.type===c.type).packed),'Unexpected chunk change '+c.type);
fs.writeFileSync(path.join(root,'research/alliance-checks.json'),JSON.stringify({version:'0.2.15',game_started:false,live_process_read:false,
  players:4,mapSize:N,darkBuildings:1,fireCleanStart:{farmLock:FarmLock,removedFarms:[]},darkUnits:109,opponentBuildings:{roman:15,viking:16,maya:11},fieldArmy:36,gardeners:12,
  addedBarracks:supply.barracks,addedToolsmiths:supply.toolsmiths,authoredGround:{levelled:ground.mode,buildings:ground.buildings,step:ground.step},barracksHeightsChanged:landscape.heights,
  barracksShadingsChanged:landscape.shading,
  supplyPiles:supply.piles,extraCarriers:supply.carriers,rejectedSupplyMutations:rejected,darkPlacementAnchors:placement,runtimeVerified:false},null,2));
console.log(`PASS: The Alliance has four players on a 512 map, the Dark Tribe as human player 1, three allied computer peoples on their own land with manned towers, field armies, 4 gardeners, a barracks and a toolsmith on level ground (blocked cores at the hotspot height, ${landscape.heights} heights and ${landscape.shading} shadings changed, steps <= ${ground.step}) with weapon, shovel, iron bar, coal and hammer piles and 40 extra carriers each (${rejected} broken supplies rejected), and room for Farms, a Fortress and Halls`,JSON.stringify(placement));

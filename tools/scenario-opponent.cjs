// Authored scenario changes; masks and map contents come from the user's local files.
const assert=require('assert');
const {prepareGrassCell}=require('./scenario-terrain.cjs');
const {GOOD,TOOLSMITH,TOOL_INPUTS,TOOL_TOTALS,TOOL_GOODS,steps,reachable,footprintCells,recordCells,pileList,pileByte7,buildingRecord,edgeStep,placePiles,
  placeToolsmithPiles,describePile,checkPiles,checkCarriers}=require('./scenario-supply.cjs');
const START={x:75,y:401};
// Authored Viking gardeners (settler type 50): the native AI never recruits them (CAIAgentGardeners::Execute 0x31740 is empty).
const GARDENERS=6;
// Start supplies for new armies (r5): the barracks turns an idle carrier and a weapon into a soldier, the plugin
// turns a carrier and a shovel into a gardener. Weapons lie beside the barracks entrance, shovels at the start;
// the extra carriers follow the gardeners. Map start content only, nothing is spawned later.
const SOURCE_PILES=16,EXTRA_CARRIERS=30;
const WEAPONS=[GOOD.SWORD,GOOD.SWORD,GOOD.SWORD,GOOD.BOW,GOOD.BOW,GOOD.BATTLEAXE],SHOVELS=[GOOD.SHOVEL,GOOD.SHOVEL];
// Toolsmith (r29): the Vikings have none, so the plugin's shovel orders could never be made. One TOOLSMITH beside
// the barracks (searched from TOOLSMITH_NEAR) with IRONBAR x16, COAL x16 and HAMMER x2 beside its entrance; the
// map's idle SMITH or a carrier with a hammer works it. Placed after every unit and r28 pile, so their positions stay.
const TOOLSMITH_NEAR=[96,442],TOOL_PILES=[...TOOL_INPUTS,GOOD.HAMMER];
function prepareOpponent(map,changes,masks) {
  const get=t=>(changes.get(t)||map.chunks.find(c=>c.type===t)).data;
  const players=Buffer.from(get(2));
  assert(players.readUInt32LE(45)===1,'Expected Viking opponent');
  players.writeUInt32LE(START.x,49);players.writeUInt32LE(START.y,53);
  changes.set(2,{data:players});
  const land=Buffer.from(get(13)),objects=Buffer.from(get(6));
  const units=Buffer.from(get(7)),buildings=Buffer.from(get(8));
  // Original loader: byte 6 enables garrison; bytes 7..15 are nine military counts.
  // Retain the original small tower and its genuine native garrison.
  const towerRows=[];
  for(let i=0;i<buildings.length;i+=20)if(buildings[i+4]===46&&buildings[i+5]===2)towerRows.push(i);
  assert(towerRows.length===1,'Expected one original Viking starting tower');
  const ti=towerRows[0];
  assert(buildings.readUInt16LE(ti)===START.x&&buildings.readUInt16LE(ti+2)===START.y,'Tower/start mismatch');
  buildings[ti+6]=1;buildings[ti+7]=2;buildings[ti+10]=1;
  const occupied=new Set(),unitCells=new Set(),addedBuildings=[],addedUnits=[];
  const key=(x,y)=>y*576+x;
  const dark=(x,y)=>!!(land[4*key(x,y)+2]&64);
  function cells(type,x,y,halo=0) {
    assert(masks[type],'Missing original footprint');const result=new Set();
    for(const [dx,dy] of masks[type])for(let oy=-halo;oy<=halo;oy++)for(let ox=-halo;ox<=halo;ox++)
      result.add(key(x+dx+ox,y+dy+oy));
    return [...result];
  }
  for(let i=0;i<buildings.length;i+=20)for(const k of cells(buildings[i+4],buildings.readUInt16LE(i),buildings.readUInt16LE(i+2),2))occupied.add(k);
  // Retain local workers at their original locations; move the central patrol and remote detachment.
  const relocated=[];
  for(let i=0;i<units.length;i+=6) {
    const x=units.readUInt16LE(i),y=units.readUInt16LE(i+2);
    if(units[i+5]===2&&(x>=140||y<340||y>460))relocated.push(i);
    else unitCells.add(key(x,y));
  }
  function validBuilding(type,x,y) {
    return cells(type,x,y,2).every(k=>{
      const x=k%576,y=Math.floor(k/576);
      return x>=25&&x<135&&y>=345&&y<=455&&!dark(x,y)&&!occupied.has(k)&&!unitCells.has(k);
    });
  }
  // Towers and their intended initial territory are authored together below.
  for(const [type,px,py] of [[47,75,362],[47,47,416],[47,103,428],[2,75,375],[3,93,382],[4,49,433],[40,107,408],[24,97,443]]) {
    let chosen;
    for(let r=0;r<=25&&!chosen;r++)for(let y=py-r;y<=py+r&&!chosen;y++)for(let x=px-r;x<=px+r;x++) {
      if(Math.max(Math.abs(x-px),Math.abs(y-py))!==r||!validBuilding(type,x,y))continue;
      chosen={type,x,y};break;
    }
    assert(chosen,'No separated footprint for Viking building '+type);
    const {x,y}=chosen,record=Buffer.alloc(20);
    record.writeUInt16LE(x,0);record.writeUInt16LE(y,2);record[4]=type;record[5]=2;record[6]=1;record[7]=1;
    if(type===47){record[7]=3;record[10]=3;}
    addedBuildings.push(record);
    // Prepare only each new footprint/entrance halo, retaining surrounding trees, stone and ores.
    for(const k of cells(type,x,y,2)) {
      occupied.add(k);prepareGrassCell(land,objects,4*k,false);
    }
  }
  function unitSpot() {
    for(let r=0;r<55;r++)for(let y=390-r;y<=390+r;y++)for(let x=70-r;x<=70+r;x++) {
      if(Math.max(Math.abs(x-70),Math.abs(y-390))!==r||x<30||x>=135||y<345||y>455)continue;
      const k=key(x,y),at=4*k;
      if(occupied.has(k)||unitCells.has(k)||dark(x,y)||land[at+1]!==28||objects.readUInt32LE(at)!==0)continue;
      unitCells.add(k);return {x,y};
    }
    throw Error('No clear Viking unit position');
  }
  for(const i of relocated) {const {x,y}=unitSpot();units.writeUInt16LE(x,i);units.writeUInt16LE(y,i+2);}
  // Starting settlers only. All later births and resource production belong to original game AI.
  for(let i=0;i<24;i++) {
    const {x,y}=unitSpot(),u=Buffer.alloc(6);u.writeUInt16LE(x,0);u.writeUInt16LE(y,2);u[4]=1;u[5]=2;addedUnits.push(u);
  }
  // Gardeners after the carriers (keeps carrier positions unchanged), for the plugin's offence director.
  for(let i=0;i<GARDENERS;i++) {
    const {x,y}=unitSpot(),u=Buffer.alloc(6);u.writeUInt16LE(x,0);u.writeUInt16LE(y,2);u[4]=50;u[5]=2;addedUnits.push(u);
  }
  // Carriers for soldier and gardener recruitment after the gardeners (earlier positions stay), off every pile.
  const goods=get(9);
  for(const [x,y] of pileList(goods))unitCells.add(key(x,y));
  for(let i=0;i<EXTRA_CARRIERS;i++) {
    const {x,y}=unitSpot(),u=Buffer.alloc(6);u.writeUInt16LE(x,0);u.writeUInt16LE(y,2);u[4]=1;u[5]=2;addedUnits.push(u);
  }
  // Pinned landscape initializer seeds owner IDs 0..8 before rebuilding sectors.
  // The source Roman owner 1 becomes Viking player 2 after the race/entity swap.
  for(let at=1;at<objects.length;at+=4) {
    assert(objects[at]===0||objects[at]===1,'Unexpected original initial owner');
    if(objects[at]===1)objects[at]=2;
  }
  const allBuildings=Buffer.concat([buildings,...addedBuildings]);
  let territoryTiles=0;
  // Map-load buildings do not acquire territory from their garrison automatically.
  // Seed the intended settlement before native sectors/economies and buildings load.
  for(let i=0;i<allBuildings.length;i+=20)if(allBuildings[i+5]===2&&[46,47].includes(allBuildings[i+4])) {
    const x=allBuildings.readUInt16LE(i),y=allBuildings.readUInt16LE(i+2),radius=masks._radii[allBuildings[i+4]];
    for(let ny=y-radius;ny<=y+radius;ny++)for(let nx=x-radius;nx<=x+radius;nx++) {
      if(nx<2||ny<2||nx>=574||ny>=574||Math.max(Math.abs(nx-x),Math.abs(ny-y),Math.abs((nx-x)-(ny-y)))>radius)continue;
      const at=4*key(nx,ny);
      if(land[at+1]<=8||(land[at+2]&64))continue;
      if(objects[at+1]!==2)territoryTiles++;
      objects[at+1]=2;
    }
  }
  // Weapons beside the barracks entrance, shovels at the start: free piles of the Viking economy on its final land.
  assert(goods.length===8*SOURCE_PILES,'Unexpected source piles');
  const allUnits=Buffer.concat([units,...addedUnits]),piles=pileList(goods),byte7=pileByte7(goods);
  const site={N:576,land,objects,owner:2,byte7,piles,blocked:footprintCells(576,allBuildings,t=>masks[t],1),
              units:recordCells(576,allUnits,6),seen:reachable(576,land,objects,2,[START.x,START.y])};
  const barracks=addedBuildings.find(b=>b[4]===24),entrance=[barracks.readUInt16LE(0)+masks._doors[24][0],barracks.readUInt16LE(2)+masks._doors[24][1]];
  const weapons=placePiles({...site,anchor:entrance,goods:WEAPONS}),shovels=placePiles({...site,anchor:[START.x,START.y],goods:SHOVELS});
  // Toolsmith site as the other authored Viking buildings (validBuilding: footprint and two-cell halo apart from every
  // footprint, unit and source pile, prepared as grass at height 10 by prepareGrassCell), also off the new piles, on
  // owned land, entrance reachable from the start, and the step from the prepared ground to its surroundings no
  // steeper than next to the other prepared Viking buildings.
  const toolDoor=masks._doors[TOOLSMITH],newPiles=new Set(piles.map(([x,y])=>key(x,y)));
  const preparedStep=(type,x,y)=>edgeStep(576,land,new Set(cells(type,x,y,2)),10);
  const maxStep=Math.max(...addedBuildings.map(b=>preparedStep(b[4],b.readUInt16LE(0),b.readUInt16LE(2))));
  let toolsmith;
  for(let r=0;r<=25&&!toolsmith;r++)for(let y=TOOLSMITH_NEAR[1]-r;y<=TOOLSMITH_NEAR[1]+r&&!toolsmith;y++)for(let x=TOOLSMITH_NEAR[0]-r;x<=TOOLSMITH_NEAR[0]+r;x++) {
    if(Math.max(Math.abs(x-TOOLSMITH_NEAR[0]),Math.abs(y-TOOLSMITH_NEAR[1]))!==r||!validBuilding(TOOLSMITH,x,y))continue;
    if(cells(TOOLSMITH,x,y,2).some(k=>objects[4*k+1]!==2||newPiles.has(k))||!site.seen[key(x+toolDoor[0],y+toolDoor[1])])continue;
    const step=preparedStep(TOOLSMITH,x,y);if(step>maxStep)continue;
    toolsmith={x,y,entrance:[x+toolDoor[0],y+toolDoor[1]],step,maxStep};break;
  }
  assert(toolsmith,'No site for the Viking toolsmith');
  for(const k of cells(TOOLSMITH,toolsmith.x,toolsmith.y,2)){occupied.add(k);prepareGrassCell(land,objects,4*k,false);}
  const finalBuildings=Buffer.concat([allBuildings,buildingRecord(toolsmith.x,toolsmith.y,TOOLSMITH,2)]);
  const tools=placeToolsmithPiles({...site,blocked:footprintCells(576,finalBuildings,t=>masks[t],1),seen:reachable(576,land,objects,2,[START.x,START.y])},toolsmith.entrance);
  changes.set(7,{data:allUnits,count:allUnits.length/6});
  changes.set(8,{data:finalBuildings,count:finalBuildings.length/20});
  changes.set(9,{data:Buffer.concat([goods,...weapons,...shovels,...tools]),count:SOURCE_PILES+weapons.length+shovels.length+tools.length});
  changes.set(13,{data:land});changes.set(6,{data:objects});
  return {start:START,relocatedUnits:relocated.length,startingCarriersAdded:24,startingGardenersAdded:GARDENERS,extraCarriersAdded:EXTRA_CARRIERS,
          extraCarrierCells:addedUnits.slice(-EXTRA_CARRIERS).map(u=>[u.readUInt16LE(0),u.readUInt16LE(2)]),
          territoryTiles,addedBuildings:addedBuildings.map(b=>({type:b[4],x:b.readUInt16LE(0),y:b.readUInt16LE(2)})),
          barracksEntrance:entrance,weaponPiles:weapons.map(describePile),shovelPiles:shovels.map(describePile),
          toolsmith,toolsmithPiles:tools.map(describePile)};
}
function verifyOpponent(map,masks) {
  const get=t=>map.chunks.find(c=>c.type===t).data;
  assert(get(2).readUInt32LE(49)===START.x&&get(2).readUInt32LE(53)===START.y,'Wrong opponent start');
  const land=get(13);let units=0,buildings=0;
  for(const [type,step] of [[7,6],[8,20]]) {
    const c=map.chunks.find(c=>c.type===type);assert(c.data.length%step===0);
    assert(c.header.readUInt32LE(20)===c.data.length/step,'Wrong entity record count');
    for(let i=0;i<c.data.length;i+=step)if(c.data[i+5]===2) {
      const x=c.data.readUInt16LE(i),y=c.data.readUInt16LE(i+2);
      assert(x<140&&x>=25&&y>=340&&y<=460,'Enemy entity outside its own external start area');
      assert(!(land[4*(y*576+x)+2]&64),'Enemy starts on Dark ground');
      if(type===7){assert(get(6)[4*(y*576+x)+1]===2,'Viking unit starts outside owned settlement');units++;}else buildings++;
    }
  }
  assert(units===68+GARDENERS+EXTRA_CARRIERS&&buildings===11,'Incomplete Viking start economy');
  let gardeners=0;for(let i=0;i<get(7).length;i+=6)if(get(7)[i+5]===2&&get(7)[i+4]===50)gardeners++;
  assert(gardeners===GARDENERS,'Viking gardeners');
  const types=[];for(let i=0;i<get(8).length;i+=20)if(get(8)[i+5]===2)types.push(get(8)[i+4]);
  for(const t of [1,2,3,4,TOOLSMITH,24,40,46])assert(types.includes(t),'Missing Viking economy building');
  assert(types.filter(t=>t===TOOLSMITH).length===1,'One Viking toolsmith');
  assert(get(9).length===8*(SOURCE_PILES+WEAPONS.length+SHOVELS.length+TOOL_PILES.length)&&map.chunks.find(c=>c.type===9).header.readUInt32LE(20)===get(9).length/8,
    'Starting tools/construction goods changed unexpectedly');
  for(let i=0;i<get(9).length;i+=8) {
    const x=get(9).readUInt16LE(i),y=get(9).readUInt16LE(i+2);
    assert(get(6)[4*(y*576+x)+1]===2,'Starting materials not in Viking economy');
  }
  // Start supplies: the source piles stay the prefix (test-scenario.cjs compares them); weapon piles beside the
  // barracks entrance, shovel piles at the start, 30 carriers after the gardeners.
  const maskOf=t=>masks[t],starts={2:[START.x,START.y]};
  const piles=checkPiles({N:576,land,objects:get(6),goods:get(9),from:SOURCE_PILES,byte7:pileByte7(get(9).subarray(0,8*SOURCE_PILES)),
    buildings:get(8),maskOf,units:get(7),starts,expected:{2:[...WEAPONS,...SHOVELS,...TOOL_PILES]},totals:{2:{...TOOL_TOTALS,[GOOD.HAMMER]:2}}})[2];
  const door=type=>{for(let i=0;i<get(8).length;i+=20)if(get(8)[i+5]===2&&get(8)[i+4]===type)
    return [get(8).readUInt16LE(i)+masks._doors[type][0],get(8).readUInt16LE(i+2)+masks._doors[type][1]];};
  const entrance=door(24),toolEntrance=door(TOOLSMITH);
  for(const p of piles)assert(steps(...(p.good===GOOD.SHOVEL?[START.x,START.y]:TOOL_GOODS.has(p.good)?toolEntrance:entrance),p.x,p.y)<=16,'Supply pile away from its anchor');
  const carriers=checkCarriers({N:576,land,objects:get(6),units:get(7),from:get(7).length/6-EXTRA_CARRIERS,owners:[2],count:EXTRA_CARRIERS,
    buildings:get(8),maskOf,goods:get(9),starts});
  const b=get(8);let tower;
  for(let i=0;i<b.length;i+=20)if(b[i+4]===46&&b[i+5]===2)tower=b.subarray(i,i+20);
  assert(tower&&tower.readUInt16LE(0)===START.x&&tower.readUInt16LE(2)===START.y
    &&tower[6]===1&&tower[7]===2&&tower[10]===1,'Missing occupied Viking starting tower');
  const military=[];
  for(let i=0;i<b.length;i+=20)if(b[i+5]===2&&[46,47].includes(b[i+4]))
    military.push({x:b.readUInt16LE(i),y:b.readUInt16LE(i+2),radius:masks._radii[b[i+4]]});
  assert(military.length===4,'Missing territorial support towers');
  for(let i=0;i<b.length;i+=20)if(b[i+5]===2&&!([46,47].includes(b[i+4]))) {
    const x=b.readUInt16LE(i),y=b.readUInt16LE(i+2);
    assert(military.some(t=>Math.max(Math.abs(x-t.x),Math.abs(y-t.y),Math.abs((x-t.x)-(y-t.y)))<=t.radius),
      'Economic building outside initial native tower influence');
  }
  const footprintCells=new Set();
  for(let i=0;i<b.length;i+=20)if(b[i+5]===2) {
    const x=b.readUInt16LE(i),y=b.readUInt16LE(i+2);
    for(const [dx,dy]of masks[b[i+4]]) {
      const k=(y+dy)*576+x+dx;
      assert(!footprintCells.has(k),'Overlapping Viking building footprints');
      assert(x+dx<140&&!(land[4*k+2]&64),'Viking footprint enters Dark start');footprintCells.add(k);
    }
  }
  // Terrain-only topology test using original XML doors. Objects/slope and engine
  // pathfinding remain user acceptance work, even if this grass component connects.
  const queue=[[START.x,START.y+2]],seen=new Set([(START.y+2)*576+START.x]);
  for(let i=0;i<queue.length;i++) {
    const [x,y]=queue[i];
    for(const [dx,dy]of [[1,0],[-1,0],[0,1],[0,-1],[1,1],[-1,-1]]) {
      const nx=x+dx,ny=y+dy,k=ny*576+nx;
      if(nx<25||nx>=140||ny<340||ny>460||seen.has(k)||![16,17,28].includes(land[k*4+1])||(land[k*4+2]&64)||get(6)[k*4+1]!==2)continue;
      seen.add(k);queue.push([nx,ny]);
    }
  }
  for(let i=0;i<b.length;i+=20)if(b[i+5]===2) {
    const [dx,dy]=masks._doors[b[i+4]],x=b.readUInt16LE(i)+dx,y=b.readUInt16LE(i+2)+dy;
    assert(seen.has(y*576+x),'Viking entrance disconnected from external base grass');
    assert(get(6)[4*(y*576+x)+1]===2,'Viking entrance has no owned initial economy sector');
  }
  return {enemyUnits:units,enemyBuildings:buildings,enemyGardeners:gardeners,enemyExtraCarriers:carriers[2],
    weaponPiles:piles.filter(p=>p.good!==GOOD.SHOVEL&&!TOOL_GOODS.has(p.good)).length,shovelPiles:piles.filter(p=>p.good===GOOD.SHOVEL).length,
    toolsmithEntrance:toolEntrance,toolsmithPiles:piles.filter(p=>TOOL_GOODS.has(p.good)),
    supplyPilesOnFreeOwnedNonDarkGrass:true,allOutsideDarkStart:true,grassEntrancesConnected:true,allEntrancesInitiallyOwnedByViking:true,startingUnitsAndMaterialsOwnedByViking:true,ownedGrassEntrancesConnected:true};
}
module.exports={prepareOpponent,verifyOpponent,START,GARDENERS,SOURCE_PILES,EXTRA_CARRIERS,WEAPONS,SHOVELS,TOOL_PILES};

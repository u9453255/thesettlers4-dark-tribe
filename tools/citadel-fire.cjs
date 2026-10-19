// Dark citadel fire geometry for the scenario builders and map tests (r31). Same numbers and formulas as
// native/citadel-fire.hpp (dtfire) and native/blockade-model.hpp; checkNative() parses both headers and fails on any
// drift. A Dark Mushroom Farm (map building type 49) at (x,y) lies in the fire field of a Dark Temple (50) or
// Fortress (51) of the same owner at (bx,by) when fire2(x,y,bx+1,by+2) < FarmLock*FarmLock (Euclid 33 on the axial
// grid). The Manacopter Hall (80) has no fire. Start maps must hold no such farm: builders remove them with
// removeFarmsInFire(), tests assert it with farmsInFire()/assertNoFarmInFire(). The plugin's start-tick crush
// (FARM_FIRE_REMOVED) is only a safety net.
// Map building type bytes are the S4_BUILDING_ENUM ids (tools/scenario-footprints.py asserts 49/50/51/80) and equal
// the plugin's building kinds; a map record's x,y is the building position the plugin measures from (r30 survey).
const fs=require('fs'),path=require('path'),assert=require('assert');
const FARM=49,TEMPLE=50,FORTRESS=51,HALL=80;
const CentreDX=1,CentreDY=2,FireLine=27,FarmReach=4,FarmLock=FireLine+FarmReach+2;
// Flag bits (objects chunk 6 byte 2) the map editor bakes under every authored building: 0x08 on the whole
// footprint, 0x01 on the blocked core, 0x02 on the entrance cells. Builders that add buildings write none of them
// (the engine sets its own), so a removed farm takes exactly these bits with it.
const BAKED_FLAGS=0x0b;
const citadelKind=k=>k===TEMPLE||k===FORTRESS;
const fire2=(x,y,cx,cy)=>{const dx=x-cx,dy=y-cy;return dx*dx-dx*dy+dy*dy;};
const centreX=bx=>bx+CentreDX,centreY=by=>by+CentreDY;
const farmTooClose=(fx,fy,bx,by)=>fire2(fx,fy,centreX(bx),centreY(by))<FarmLock*FarmLock;
const euclid=(fx,fy,bx,by)=>Math.sqrt(fire2(fx,fy,centreX(bx),centreY(by)));
// Building records (chunk 8, 20 bytes: x u16, y u16, type, owner, ...) as rows; accepts a Buffer or an array of records.
function buildingRows(buildings) {
  const list=Buffer.isBuffer(buildings)?Array.from({length:buildings.length/20},(_,i)=>buildings.subarray(20*i,20*i+20)):buildings;
  return list.map((r,index)=>({index,x:r.readUInt16LE(0),y:r.readUInt16LE(2),type:r[4],owner:r[5]}));
}
// Every farm inside a fire field of a citadel of its own owner, with the nearest such citadel and its Euclid distance.
function farmsInFire(buildings) {
  const rows=buildingRows(buildings),citadels=rows.filter(r=>citadelKind(r.type));
  return rows.filter(r=>r.type===FARM).flatMap(f=>{
    const near=citadels.filter(c=>c.owner===f.owner&&farmTooClose(f.x,f.y,c.x,c.y)).sort((a,b)=>euclid(f.x,f.y,a.x,a.y)-euclid(f.x,f.y,b.x,b.y));
    return near.length?[{index:f.index,x:f.x,y:f.y,owner:f.owner,citadel:{type:near[0].type,x:near[0].x,y:near[0].y},euclid:+euclid(f.x,f.y,near[0].x,near[0].y).toFixed(2)}]:[];
  });
}
function assertNoFarmInFire(buildings,label='map') {
  const inside=farmsInFire(buildings);
  assert(!inside.length,`${label}: Dark Mushroom Farm inside a Temple/Fortress fire field (FarmLock ${FarmLock}): `+inside.map(f=>`${f.x},${f.y} (Euclid ${f.euclid} from ${f.citadel.type}@${f.citadel.x},${f.citadel.y})`).join(' '));
}
// Remove every farm record inside a fire field and clear the flags the editor baked under it (BAKED_FLAGS on its
// footprint cells that no kept building covers). Objects, owner and resource bytes, landscape, units and goods stay:
// the maps store no Mushroom Farmer or mushroom bound to a farm. `farmMask`/`farmCore` are the Dark farm footprint and
// blocked core offsets, `maskOf(type,owner)` the footprint of every other building. Returns the kept records and a
// report of the removed farms.
function removeFarmsInFire({N,buildings,objects,farmMask,farmCore,maskOf}) {
  const removed=farmsInFire(buildings),gone=new Set(removed.map(f=>f.index));
  const kept=buildings.filter((_,i)=>!gone.has(i));
  const covered=new Set();
  for(const r of kept){const m=maskOf(r[4],r[5]);assert(m,'Missing footprint '+r[4]);for(const [dx,dy] of m)covered.add((r.readUInt16LE(2)+dy)*N+r.readUInt16LE(0)+dx);}
  const core=new Set(farmCore.map(([dx,dy])=>dx+','+dy));
  for(const f of removed) {
    let cleared=0;
    for(const [dx,dy] of farmMask) {
      const k=(f.y+dy)*N+f.x+dx,at=4*k;
      // The authored pattern this removal relies on: footprint 0x08, blocked core 0x01, no object.
      assert(objects[at+2]&0x08&&!!(objects[at+2]&0x01)===core.has(dx+','+dy)&&!objects[at],`Unexpected authored flags under farm ${f.x},${f.y}`);
      if(covered.has(k))continue;
      objects[at+2]&=~BAKED_FLAGS;cleared++;
    }
    f.flagCellsCleared=cleared;
  }
  return {kept,removed};
}
// The flag cells of a removed farm must be free again (BAKED_FLAGS clear) unless a kept building covers them.
function assertFarmCleared({N,objects,farm:[x,y],farmMask,covered=new Set()}) {
  for(const [dx,dy] of farmMask){const k=(y+dy)*N+x+dx;if(!covered.has(k))assert(!(objects[4*k+2]&BAKED_FLAGS),`Flags left under the removed farm ${x},${y}`);}
}
// Cross-check with the native headers: constants, centre offset, the fire2 formula, the strict farm test and the kinds.
function checkNative(root=path.resolve(__dirname,'..')) {
  const header=fs.readFileSync(path.join(root,'native/citadel-fire.hpp'),'utf8'),values={};
  for(const m of header.matchAll(/constexpr int ([^;]+);/g))for(const part of m[1].split(',')) {
    const [name,expr]=part.split('=').map(s=>s.trim());
    const code=expr.replace(/[A-Za-z_]\w*/g,id=>{assert(id in values,'Unknown constant '+id);return String(values[id]);});
    assert(/^[\d+\-*() ]+$/.test(code),'Unsupported expression '+expr);values[name]=Function('return '+code)();
  }
  assert.deepStrictEqual(values,{CentreDX,CentreDY,FireReach:24,FireLine,FarmReach,FarmLock},'tools/citadel-fire.cjs drifted from native/citadel-fire.hpp');
  for(const text of ['int dx=x-cx,dy=y-cy;return dx*dx-dx*dy+dy*dy;','inline int centreX(int buildingX){return buildingX+CentreDX;}','inline int centreY(int buildingY){return buildingY+CentreDY;}',
    'inline bool farmTooClose(int fx,int fy,int bx,int by){return fire2(fx,fy,centreX(bx),centreY(by))<FarmLock*FarmLock;}'])
    assert(header.includes(text),'native/citadel-fire.hpp changed: '+text);
  const blockade=fs.readFileSync(path.join(root,'native/blockade-model.hpp'),'utf8');
  assert(blockade.includes('inline bool citadelKind(int kind){return kind==50||kind==51;}')&&blockade.includes('else if(kind==49)farms.push_back({x,y});'),'native/blockade-model.hpp kinds changed');
  // The vectors of native/construction-tests.cpp fireBlockade().
  const bx=300,by=300,cx=bx+1,cy=by+2;
  assert(farmTooClose(cx+32,cy,bx,by)&&!farmTooClose(cx+33,cy,bx,by)&&farmTooClose(cx,cy+32,bx,by)&&!farmTooClose(cx,cy+33,bx,by));
  assert(farmTooClose(cx+32,cy+32,bx,by)&&!farmTooClose(cx+33,cy+33,bx,by)&&fire2(cx+38,cy+19,cx,cy)===1083&&farmTooClose(cx+38,cy+19,bx,by)&&!farmTooClose(cx+40,cy+20,bx,by));
  let cells=0;for(let y=0;y<200;y++)for(let x=0;x<200;x++)if(farmTooClose(x,y,100,100))cells++;
  assert(cells===3943,'Fire field cell count');
  return values;
}
module.exports={FARM,TEMPLE,FORTRESS,HALL,CentreDX,CentreDY,FireLine,FarmReach,FarmLock,BAKED_FLAGS,citadelKind,fire2,farmTooClose,euclid,buildingRows,farmsInFire,
  assertNoFarmInFire,removeFarmsInFire,assertFarmCleared,checkNative};

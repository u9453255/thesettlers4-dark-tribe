// Disposable decoded maps; no game execution, saves, or installation.
const fs=require('fs'),path=require('path'),assert=require('assert');
const {execFileSync}=require('child_process');
const {readMap,writeMap}=require('./map-format.cjs');
const {verifyOpponent,SOURCE_PILES}=require('./scenario-opponent.cjs');
const {farmsInFire,assertNoFarmInFire,assertFarmCleared,checkNative,buildingRows,FarmLock,FARM,BAKED_FLAGS}=require('./citadel-fire.cjs');
const root=path.resolve(__dirname,'..');
const map=readMap(path.join(root,'work/generated/Map/User/DarkTribe_FirstDawn.map'));
const {gameDir,python}=require('./project-paths.cjs');
const source=readMap(path.join(gameDir(),'Map/Campaign/dark03.map'));
const masks=JSON.parse(execFileSync(python(),[path.join(__dirname,'scenario-footprints.py')],{encoding:'utf8'}));
verifyOpponent(map,masks);
function mutated(type,mutate) {
  const data=Buffer.from(map.chunks.find(c=>c.type===type).data);mutate(data);
  const disposable={...map,chunks:map.chunks.map(c=>c.type===type?{...c,data}:c)};
  assert.throws(()=>verifyOpponent(disposable,masks));
}
mutated(2,d=>d.writeUInt32LE(261,49));
mutated(7,d=>{for(let i=0;i<d.length;i+=6)if(d[i+5]===2){d.writeUInt16LE(261,i);d.writeUInt16LE(279,i+2);break;}});
mutated(7,d=>{for(let i=0;i<d.length;i+=6)if(d[i+5]===2&&d[i+4]===50){d[i+4]=1;break;}});
mutated(8,d=>{for(let i=0;i<d.length;i+=20)if(d[i+5]===2&&d[i+4]===46){d[i+5]=1;break;}});
mutated(8,d=>{for(let i=0;i<d.length;i+=20)if(d[i+5]===2&&d[i+4]===46){d[i+6]=0;break;}});
mutated(8,d=>{for(let i=0;i<d.length;i+=20)if(d[i+5]===2&&d[i+4]===47){d[i+4]=1;break;}});
// Start supplies: a missing barracks, weapon piles moved onto Dark land or owned land turned Dark under them,
// a pile under the barracks, an overfull pile and a missing extra carrier are rejected.
const piles=map.chunks.find(c=>c.type===9).data,addedPile=good=>{for(let i=8*SOURCE_PILES;i<piles.length;i+=8)if(piles[i+4]===good)return i;};
mutated(8,d=>{for(let i=0;i<d.length;i+=20)if(d[i+5]===2&&d[i+4]===24){d[i+4]=1;break;}});
mutated(9,d=>{const i=addedPile(34);d.writeUInt16LE(277,i);d.writeUInt16LE(214,i+2);});
mutated(13,d=>{const i=addedPile(34);d[4*(piles.readUInt16LE(i+2)*576+piles.readUInt16LE(i))+2]|=64;});
mutated(9,d=>{const i=addedPile(8),b=map.chunks.find(c=>c.type===8).data;
  for(let j=0;j<b.length;j+=20)if(b[j+5]===2&&b[j+4]===24){d.writeUInt16LE(b.readUInt16LE(j),i);d.writeUInt16LE(b.readUInt16LE(j+2),i+2);}});
mutated(9,d=>{d[addedPile(31)+5]=9;});
mutated(7,d=>{d[d.length-2]=29;});
// Toolsmith (r29): a missing toolsmith, an iron bar pile replaced by a sword, a coal pile moved onto Dark land, an
// iron bar pile moved next to the start (away from the toolsmith) and an emptied hammer pile are rejected.
mutated(8,d=>{for(let i=0;i<d.length;i+=20)if(d[i+5]===2&&d[i+4]===21){d[i+4]=1;break;}});
mutated(9,d=>{d[addedPile(20)+4]=34;});
mutated(13,d=>{const i=addedPile(10);d[4*(piles.readUInt16LE(i+2)*576+piles.readUInt16LE(i))+2]|=64;});
mutated(9,d=>{const i=addedPile(31),j=addedPile(20);d.writeUInt16LE(piles.readUInt16LE(i)+1,j);d.writeUInt16LE(piles.readUInt16LE(i+2)+3,j+2);});
mutated(9,d=>{d[addedPile(18)+5]=1;});
for(const type of [3,10])assert(map.chunks.find(c=>c.type===type).data.equals(source.chunks.find(c=>c.type===type).data),
  'Original teams and victory metadata must be retained');
const sourcePiles=source.chunks.find(c=>c.type===9).data;
assert(sourcePiles.length===8*SOURCE_PILES&&piles.subarray(0,sourcePiles.length).equals(sourcePiles),'Original starting tool/goods piles must stay the unchanged prefix');
// Preserve every original Dark unit record, apart from remapping its owner to player 1.
{
  const before=source.chunks.find(c=>c.type===7).data,after=map.chunks.find(c=>c.type===7).data;
  for(let i=0;i<before.length;i+=6)if(before[i+5]===2) {
    const expected=Buffer.from(before.subarray(i,i+6));expected[5]=1;
    assert(expected.equals(after.subarray(i,i+6)),'Dark starting entity changed');
  }
}
// Clean start (r31, citadel-fire.cjs): every original Dark building stays unchanged and in order (owner 1) except the
// farms inside the Temple's fire field (FarmLock 33), which are gone with the flags the editor baked under them.
checkNative();
const sourceBuildings=source.chunks.find(c=>c.type===8).data,removedFarms=farmsInFire(sourceBuildings).filter(f=>f.owner===2);
assert.deepStrictEqual(removedFarms.map(f=>[f.x,f.y]),[[337,282]],'First Dawn removes exactly the farm by the Temple');
{
  const gone=new Set(removedFarms.map(f=>f.index)),after=map.chunks.find(c=>c.type===8).data,expected=[],kept=[];
  for(let i=0;i<sourceBuildings.length;i+=20)if(sourceBuildings[i+5]===2&&!gone.has(i/20)){const r=Buffer.from(sourceBuildings.subarray(i,i+20));r[5]=1;expected.push(r.toString('hex'));}
  for(let i=0;i<after.length;i+=20)if(after[i+5]===1)kept.push(after.subarray(i,i+20).toString('hex'));
  assert.deepStrictEqual(kept,expected,'Dark starting buildings changed');
}
const fireOf=m=>m.chunks.find(c=>c.type===8).data;
assertNoFarmInFire(fireOf(map),'First Dawn');
assertFarmCleared({N:576,objects:map.chunks.find(c=>c.type===6).data,farm:[337,282],farmMask:masks[FARM]});
// Broken starts: a kept farm moved into the fire field (Euclid 32 on the axis from the centre) or the removed farm put
// back are rejected; Euclid 33 is allowed.
const temple=buildingRows(fireOf(map)).find(r=>r.owner===1&&r.type===50),aFarm=buildingRows(fireOf(map)).find(r=>r.owner===1&&r.type===FARM);
function farmAt(x,y){const d=Buffer.from(fireOf(map));d.writeUInt16LE(x,20*aFarm.index);d.writeUInt16LE(y,20*aFarm.index+2);return d;}
assert.throws(()=>assertNoFarmInFire(farmAt(temple.x+1+32,temple.y+2)),/fire field/);
assert.throws(()=>assertNoFarmInFire(farmAt(337,282)),/fire field/);
assertNoFarmInFire(farmAt(temple.x+1+FarmLock,temple.y+2));
const script=map.chunks.find(c=>c.type===16).data;
assert(script.subarray(0,-1).equals(fs.readFileSync(path.join(root,'scenario/DarkTribe_FirstDawn.txt'))));
assert(writeMap(map).equals(map.raw));
console.log('PASS: separated opponent, occupied tower/territory, native economy, weapon/shovel piles and extra carriers, a toolsmith with iron bar/coal/hammer piles, rejection of 17 broken starts (incl. a missing gardener, barracks or toolsmith, a weapon or coal pile on Dark land, wrong or misplaced toolsmith inputs), unchanged Dark entities/teams/source goods/victory metadata and map round trip');

// Reproduce the failed 0.2.4 barracks entrance label and wrong owner separately.
const rows=map.chunks.find(c=>c.type===8).data;
let entrance;
for(let i=0;i<rows.length;i+=20)if(rows[i+4]===24&&rows[i+5]===2) {
  const [dx,dy]=masks._doors[24];entrance=4*((rows.readUInt16LE(i+2)+dy)*576+rows.readUInt16LE(i)+dx)+1;
}
assert(entrance);mutated(6,d=>{d[entrance]=0;});mutated(6,d=>{d[entrance]=1;});
const oldObjects=source.chunks.find(c=>c.type===6).data,newObjects=map.chunks.find(c=>c.type===6).data;
const removedCells=new Set(masks[FARM].map(([dx,dy])=>(282+dy)*576+337+dx));
const oldLand=source.chunks.find(c=>c.type===13).data,newLand=map.chunks.find(c=>c.type===13).data;
for(let at=0;at<oldObjects.length;at+=4) {
  const prepared=!newLand.subarray(at,at+4).equals(oldLand.subarray(at,at+4));
  const farm=removedCells.has(at/4);
  assert(newObjects[at+2]===(prepared?(oldObjects[at+2]&~0x43):farm?(oldObjects[at+2]&~BAKED_FLAGS):oldObjects[at+2]),'Flags changed outside explicitly prepared grass and the removed farm');
  assert(newObjects[at+3]===oldObjects[at+3],'Native resources erased');
  if(oldObjects[at+1]===1)assert(newObjects[at+1]===2,'Original Viking territory not remapped');
  assert(newObjects[at+1]!==1,'Ordinary Viking land assigned to Dark player');
}
console.log('PASS: null/wrong-owner barracks entrance rejected; flags rebuilt only on prepared terrain and cleared under the removed farm, resources retained and ownership remapped');
console.log(`PASS: clean start, no Dark farm within FarmLock ${FarmLock} of the Temple fire centre (farm 337,282 removed with its flags; 2 broken starts rejected)`);

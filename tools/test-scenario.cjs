// Disposable decoded maps; no game execution, saves, or installation.
const fs=require('fs'),path=require('path'),assert=require('assert');
const {execFileSync}=require('child_process');
const {readMap,writeMap}=require('./map-format.cjs');
const {verifyOpponent}=require('./scenario-opponent.cjs');
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
mutated(8,d=>{for(let i=0;i<d.length;i+=20)if(d[i+5]===2&&d[i+4]===46){d[i+5]=1;break;}});
mutated(8,d=>{for(let i=0;i<d.length;i+=20)if(d[i+5]===2&&d[i+4]===46){d[i+6]=0;break;}});
mutated(8,d=>{for(let i=0;i<d.length;i+=20)if(d[i+5]===2&&d[i+4]===47){d[i+4]=1;break;}});
for(const type of [3,9,10])assert(map.chunks.find(c=>c.type===type).data.equals(source.chunks.find(c=>c.type===type).data),
  'Original teams, starting tool/goods piles and victory metadata must be retained');
// Preserve every original Dark building/unit record, apart from remapping its owner to player 1.
for(const [type,step]of [[7,6],[8,20]]) {
  const before=source.chunks.find(c=>c.type===type).data,after=map.chunks.find(c=>c.type===type).data;
  for(let i=0;i<before.length;i+=step)if(before[i+5]===2) {
    const expected=Buffer.from(before.subarray(i,i+step));expected[5]=1;
    assert(expected.equals(after.subarray(i,i+step)),'Dark starting entity changed');
  }
}
const script=map.chunks.find(c=>c.type===16).data;
assert(script.subarray(0,-1).equals(fs.readFileSync(path.join(root,'scenario/DarkTribe_FirstDawn.txt'))));
assert(writeMap(map).equals(map.raw));
console.log('PASS: separated opponent, occupied tower/territory, native economy, rejection of 5 broken starts, unchanged Dark entities/teams/goods/victory metadata and map round trip');

// Reproduce the failed 0.2.4 barracks entrance label and wrong owner separately.
const rows=map.chunks.find(c=>c.type===8).data;
let entrance;
for(let i=0;i<rows.length;i+=20)if(rows[i+4]===24&&rows[i+5]===2) {
  const [dx,dy]=masks._doors[24];entrance=4*((rows.readUInt16LE(i+2)+dy)*576+rows.readUInt16LE(i)+dx)+1;
}
assert(entrance);mutated(6,d=>{d[entrance]=0;});mutated(6,d=>{d[entrance]=1;});
const oldObjects=source.chunks.find(c=>c.type===6).data,newObjects=map.chunks.find(c=>c.type===6).data;
const oldLand=source.chunks.find(c=>c.type===13).data,newLand=map.chunks.find(c=>c.type===13).data;
for(let at=0;at<oldObjects.length;at+=4) {
  const prepared=!newLand.subarray(at,at+4).equals(oldLand.subarray(at,at+4));
  assert(newObjects[at+2]===(prepared?(oldObjects[at+2]&~0x43):oldObjects[at+2]),'Flags changed outside explicitly prepared grass');
  assert(newObjects[at+3]===oldObjects[at+3],'Native resources erased');
  if(oldObjects[at+1]===1)assert(newObjects[at+1]===2,'Original Viking territory not remapped');
  assert(newObjects[at+1]!==1,'Ordinary Viking land assigned to Dark player');
}
console.log('PASS: null/wrong-owner barracks entrance rejected; flags rebuilt only on prepared terrain, resources retained and ownership remapped');

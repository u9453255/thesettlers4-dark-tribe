// Actual local XML masks and decoded map; no game, process or input access.
const assert=require('assert'),fs=require('fs'),path=require('path');
const {execFileSync}=require('child_process');
const {readMap}=require('./map-format.cjs');
const {prepareGrassCell}=require('./scenario-terrain.cjs');
const root=path.resolve(__dirname,'..');
const map=readMap(path.join(root,'work/generated/Map/User/DarkTribe_FirstDawn.map'));
const {gameDir,python}=require('./project-paths.cjs');
const source=readMap(path.join(gameDir(),'Map/Campaign/dark03.map'));
const get=(m,t)=>m.chunks.find(c=>c.type===t).data;
const masks=JSON.parse(execFileSync(python(),[path.join(__dirname,'scenario-footprints.py')],{encoding:'utf8'}));
const land=get(map,13),objects=get(map,6),bad=Buffer.from(objects);
// Recreate 0.2.5 exactly in this clearing: flattened Dark grass, cleared objects,
// but the source terrain's blocking flags retained. Ownership stays corrected.
for(let y=284;y<=332;y++)for(let x=384;x<=432;x++) {
  const at=4*(y*576+x);bad[at+2]=get(source,6)[at+2];
}
function reservation(cells) {
  const result=new Map();for(const [x,y]of cells)for(let dy=-1;dy<=1;dy++)for(let dx=-1;dx<=1;dx++)result.set((y+dy)*576+x+dx,[x+dx,y+dy]);
  return [...result.values()];
}
// Pinned Dark outline validator: masked native flags must equal Dark bit 4,
// and terrain must belong to its permitted groups. Use full mask union + halo,
// which is stricter than its native body check and matches the mod reservation.
function accepts(cells,x,y,o,l=land) {
  return cells.every(([dx,dy])=>{
    const a=x+dx,b=y+dy;if(a<1||b<1||a>=575||b>=575)return false;
    const at=4*(b*576+a),flags=o[at+2]|((l[at+2]&64)?4:0),terrain=l[at+1];
    return (flags&0x5f)===4&&terrain!==21&&([16,64,144].includes(terrain&240)||terrain===48)
      &&!!(l[at+2]&64)&&terrain>=16&&terrain<96&&!o[at];
  });
}
const result=[];
for(const type of [49,50,51,80]) {
  const cells=reservation(masks[type]);let before=0,after=0;
  for(let y=300;y<=316;y++)for(let x=395;x<=417;x++) {before+=accepts(cells,x,y,bad);after+=accepts(cells,x,y,objects);}
  assert(after===391,'Prepared clearing cannot fit Dark building '+type);
  if(type===49)assert(before>0,'Failed fixture must still permit a Farm');else assert(before===0,'Failed fixture must reject larger building '+type);
  const occupied=Buffer.from(objects);occupied[4*(308*576+406)+2]|=2;
  assert(!accepts(cells,406,308,occupied),'Collision accepted for '+type);
  const nonDark=Buffer.from(land);nonDark[4*(308*576+406)+2]&=~64;
  assert(!accepts(cells,406,308,objects,nonDark),'Non-Dark cell accepted for '+type);
  assert(!accepts(cells,1,1,objects),'Map edge accepted');
  result.push({type,footprintCells:masks[type].length,reservationCells:cells.length,oldPreparedAnchorsAccepted:before,newPreparedAnchorsAccepted:after});
}
// Clear only flags tied to explicitly removed objects/old prepared terrain.
const fixture=Buffer.from([93,2,0xff,0x71]),ground=Buffer.alloc(4);
prepareGrassCell(ground,fixture,0,true);
assert.deepStrictEqual([...fixture],[0,2,0xbc,0x71]);
assert((fixture[2]&0x18)===0x18,'Unrelated collision/reservation flags erased');
fs.writeFileSync(path.join(root,'research/placement-offline-checks.json'),JSON.stringify({version:'0.2.15',game_started:false,live_process_read:false,actualEnginePlacementVerified:false,
  checks:'0.2.5 regression reproduced for all four real masks; prepared terrain repaired; occupancy, Dark bounds, map edges and unrelated metadata retained',results:result},null,2));
console.log('PASS: actual four Dark footprints reproduce 0.2.5 Farm-only placement; 391 repaired anchors per building; collision/Dark/map-edge and metadata safeguards retained');

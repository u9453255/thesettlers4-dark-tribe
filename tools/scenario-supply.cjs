// Authored START supplies of computer players: a barracks, free weapon and shovel piles, carriers.
// Native soldier production (CAIAgentRecruitment 0x32CD0) orders soldiers at every ready barracks of the
// computer's land region; a barracks turns an idle carrier plus a weapon from its input pile into a soldier,
// and carriers bring the weapons from free piles of the same economy. The plugin recruits gardeners natively
// from carriers and shovels. All of this is map start content: nothing is spawned or credited later.
// Records (work/native-analysis/ai/r5/supply_verify.py): chunk 9 pile 8 B = u16 x, u16 y, u8 good,
// u8 amount 1..8, byte6 0xFE free pile (CAnyWherePileRole), byte7 as the map's own records.
// Chunk 8 building 20 B = u16 x, u16 y, type, owner, bytes 6..19 garrison (zero for non-military).
// Chunk 13 landscape 4 B per cell: byte 0 height, byte 1 ground type, byte 2 bits 0..3 slope shading (shadeOf)
// and bits 4..7 flags (0x40 Dark land), byte 3 as authored.
const assert=require('assert');
const {clearObjectCell}=require('./scenario-terrain.cjs');
const GOOD={BATTLEAXE:5,BLOWGUN:6,BOW:8,COAL:10,HAMMER:18,IRONBAR:20,SHOVEL:31,SWORD:34};
const PILE_MAX=8,FREE_PILE=0xFE,TOOLSMITH=21,BARRACKS=24,CARRIER=1;
// Toolsmith start supply (r29). A TOOLSMITH (S4_BUILDING_TOOLSMITH 21, the map's building type byte) makes a tool
// only while one is ordered (per-economy order bytes eco+0x348, set by net 0x13A9 through 0xD6A90 and consumed by
// 0x14CE20); the computers never order shovels for gardeners, the plugin does. Its inputs are IRONBAR and COAL
// (buildingInfo.xml): 16 of each in piles of 8 beside the entrance, and one pile of 2 HAMMERs so an idle carrier
// can become the SMITH (settler 15) where no idle smith exists.
const TOOL_INPUTS=[GOOD.IRONBAR,GOOD.IRONBAR,GOOD.COAL,GOOD.COAL],TOOL_HAMMERS=2;
const TOOL_TOTALS={[GOOD.IRONBAR]:16,[GOOD.COAL]:16},TOOL_GOODS=new Set([GOOD.IRONBAR,GOOD.COAL,GOOD.HAMMER]);
const NB=[[1,0],[-1,0],[0,1],[0,-1],[1,1],[-1,-1]];
// Steps between two cells of the map's hexagonal grid (neighbours as in NB).
const steps=(ax,ay,bx,by)=>{const dx=bx-ax,dy=by-ay;return Math.max(Math.abs(dx),Math.abs(dy),Math.abs(dx-dy));};
// Grass as the unit helpers accept it: ground 16..31 except 21 (grass/swamp edge).
const grass=t=>t>=16&&t<32&&t!==21;
// Free owned grass inside the map: no object, no blocking/keep-clear flag (0x5B), not Dark.
function freeCell(N,land,objects,owner,k) {
  const x=k%N,y=(k/N)|0,at=4*k;
  return x>=2&&y>=2&&x<N-2&&y<N-2&&objects[at+1]===owner&&!(land[at+2]&64)&&!objects[at]&&!(objects[at+2]&0x5b)&&grass(land[at+1]);
}
// Owned cells a carrier walks to from the start: no blocking flag 0x01, ground 16..47, not Dark.
function reachable(N,land,objects,owner,[sx,sy]) {
  const walk=k=>{const at=4*k,t=land[at+1];return objects[at+1]===owner&&!(objects[at+2]&1)&&t>=16&&t<48&&!(land[at+2]&64);};
  const seen=new Uint8Array(N*N),queue=[];
  for(let r=0;r<6&&!queue.length;r++)for(let y=sy-r;y<=sy+r&&!queue.length;y++)for(let x=sx-r;x<=sx+r;x++) {
    const k=y*N+x;if(walk(k)){seen[k]=1;queue.push(k);break;}
  }
  assert(queue.length,'No walkable start cell of player '+owner);
  for(let i=0;i<queue.length;i++) {
    const k=queue[i],x=k%N,y=(k/N)|0;
    for(const [dx,dy] of NB) {
      const nx=x+dx,ny=y+dy,nk=ny*N+nx;
      if(nx<0||ny<0||nx>=N||ny>=N||seen[nk]||!walk(nk))continue;
      seen[nk]=1;queue.push(nk);
    }
  }
  return seen;
}
function area(N,mask,x,y,halo) {
  const cells=new Set();
  for(const [dx,dy] of mask)for(let oy=-halo;oy<=halo;oy++)for(let ox=-halo;ox<=halo;ox++)cells.add((y+dy+oy)*N+x+dx+ox);
  return cells;
}
// Level ground. Every authored building of the source maps except the mines (types 14..18, on their mountains)
// stands on level ground: all cells of its blocked core (blockPosLines; equal to digPosLines for every barracks)
// have the height of its hotspot, and in AO_trojan10 so has its whole footprint (block, building and dig lines).
// The map loader levels nothing, so an added building is levelled here the same way, and only where levelling
// keeps every step to the unchanged neighbours within the steepest step next to the map's own level cells
// (measured by authoredGround: dark07 11, AO_trojan10 12).
const MINES=new Set([14,15,16,17,18]);
// siteFor's flat-site preference (documented bound): a site whose ground (its levelled cells and their one-cell
// ring) spans more than FLAT_SPAN height units gives way to a flatter valid site within FLAT_RADIUS rings of the
// search centre. The ring counts because it bounds the step levelling leaves next to the building.
const FLAT_SPAN=4,FLAT_RADIUS=8;
// Landscape byte 2 bits 0..3 are the slope shading the map editor derives from the heights above and below a
// cell: 8 + floor((height(x,y-1) - height(x,y+1)) / 2), clamped to 0..15, a row outside the map counting as the
// cell's own height. Every cell of dark07 and AO_trojan10 matches it (authoredGround asserts it); bits 4..7 are
// flags (0x40 Dark land). levelSite recomputes the shading wherever a height changes.
const SHADE=0x0f;
function shadeOf(N,land,k) {
  const y=(k/N)|0,h=land[4*k],up=y>0?land[4*(k-N)]:h,down=y<N-1?land[4*(k+N)]:h;
  return Math.max(0,Math.min(15,8+Math.floor((up-down)/2)));
}
// Cells whose shading reads the height of a cell of `cells`: the cells above and below each of them.
function shadedBy(N,cells) {
  const out=new Set();for(const k of cells){if(k>=N)out.add(k-N);if(k<N*N-N)out.add(k+N);}return out;
}
// Steepest height step from a cell of `cells` to a neighbour outside them; with `height`, as if every cell of
// `cells` had that height (after levelling).
function edgeStep(N,land,cells,height) {
  let step=0;
  for(const k of cells) {
    const x=k%N,y=(k/N)|0,h=height===undefined?land[4*k]:height;
    for(const [dx,dy] of NB){const n=(y+dy)*N+x+dx;if(!cells.has(n))step=Math.max(step,Math.abs(h-land[4*n]));}
  }
  return step;
}
// Original height span of a building's levelled cells.
function heightSpan(N,land,cells) {
  let low=255,high=0;for(const k of cells){low=Math.min(low,land[4*k]);high=Math.max(high,land[4*k]);}
  return high-low;
}
// `cells` and their neighbours outside them (the one-cell ring).
function withRing(N,cells) {
  const out=new Set(cells);
  for(const k of cells){const x=k%N,y=(k/N)|0;for(const [dx,dy] of NB)out.add((y+dy)*N+x+dx);}
  return out;
}
// How a map's own (non-mine) buildings stand: mode 'footprint' when every whole footprint (maskOf) is level at its
// hotspot height, otherwise 'core' (every blocked core, blockOf, must be); `step` is the steepest step from those
// level cells to their outside neighbours over all of them. `levelOf` gives the cells an added building levels.
function authoredGround(N,land,buildings,maskOf,blockOf) {
  const sites=[];
  for(let i=0;i<buildings.length;i+=20) {
    const type=buildings[i+4],owner=buildings[i+5],x=buildings.readUInt16LE(i),y=buildings.readUInt16LE(i+2);
    if(MINES.has(type))continue;
    const whole=area(N,maskOf(type,owner),x,y,0),core=area(N,blockOf(type,owner),x,y,0),hotspot=y*N+x;
    assert(core.has(hotspot)&&[...core].every(k=>whole.has(k)),'Blocked core of building '+type);
    sites.push({whole,core,height:land[4*hotspot]});
  }
  assert(sites.length,'No authored buildings');
  const level=cells=>sites.every(s=>[...s[cells]].every(k=>land[4*k]===s.height));
  assert(level('core'),'Authored building off level ground');
  for(let k=0;k<N*N;k++)assert((land[4*k+2]&SHADE)===shadeOf(N,land,k),'Authored shading not derived from the heights');
  const mode=level('whole')?'footprint':'core',cells=mode==='footprint'?'whole':'core';
  return {mode,buildings:sites.length,step:Math.max(...sites.map(s=>edgeStep(N,land,s[cells]))),
    levelOf:(mask,block)=>mode==='footprint'?mask:block};
}
// Level an added building: every cell of `level` (offsets from x,y) takes the hotspot height and the shading of
// the cells above and below a changed height is recomputed (shadeOf), as the map editor keeps it. Ground type and
// the flag bits stay as authored. Returns the height, the original span, the changed heights and shadings.
function levelSite(N,land,level,x,y) {
  const height=land[4*(y*N+x)],cells=area(N,level,x,y,0),span=heightSpan(N,land,cells),changed=new Set();
  for(const k of cells)if(land[4*k]!==height){land[4*k]=height;changed.add(k);}
  let shaded=0;
  for(const k of shadedBy(N,changed)) {
    const value=(land[4*k+2]&~SHADE)|shadeOf(N,land,k);
    if(value!==land[4*k+2]){land[4*k+2]=value;shaded++;}
  }
  return {height,span,cells:cells.size,changed:changed.size,shaded};
}
// Cells under every building footprint (maskOf(type, owner)), grown by a square halo.
function footprintCells(N,buildings,maskOf,halo=0) {
  const cells=new Set();
  for(let i=0;i<buildings.length;i+=20) {
    const mask=maskOf(buildings[i+4],buildings[i+5]);assert(mask,'Missing footprint '+buildings[i+4]);
    for(const k of area(N,mask,buildings.readUInt16LE(i),buildings.readUInt16LE(i+2),halo))cells.add(k);
  }
  return cells;
}
// Map cells of unit (6 B) or pile (8 B) records.
function recordCells(N,data,step) {
  const cells=new Set();
  for(let i=0;i<data.length;i+=step)cells.add(data.readUInt16LE(i+2)*N+data.readUInt16LE(i));
  return cells;
}
function pileList(goods) {
  const list=[];for(let i=0;i<goods.length;i+=8)list.push([goods.readUInt16LE(i),goods.readUInt16LE(i+2)]);return list;
}
// byte7 shared by every pile record of the map (0xCD in the Dark campaign maps, 0 in AO_trojan10).
function pileByte7(goods) {
  const values=new Set();for(let i=7;i<goods.length;i+=8)values.add(goods[i]);
  assert(values.size===1,'Mixed byte7 in the map piles');return [...values][0];
}
function pileRecord(x,y,good,amount,byte7) {
  assert(amount>=1&&amount<=PILE_MAX,'Pile amount');
  const r=Buffer.alloc(8);r.writeUInt16LE(x,0);r.writeUInt16LE(y,2);r[4]=good;r[5]=amount;r[6]=FREE_PILE;r[7]=byte7;return r;
}
function buildingRecord(x,y,type,owner) {
  const r=Buffer.alloc(20);r.writeUInt16LE(x,0);r.writeUInt16LE(y,2);r[4]=type;r[5]=owner;return r;
}
function unitRecord(x,y,type,owner) {
  const r=Buffer.alloc(6);r.writeUInt16LE(x,0);r.writeUInt16LE(y,2);r[4]=type;r[5]=owner;return r;
}
// Site of an added building, searched ring by ring around `near`: the footprint and a one-cell halo inside the
// map on grass owned by the player, not Dark, without reservation flags (0x18), off every other footprint and
// its two-cell halo (`blocked`), off units and piles; the entrance reachable from the start (`seen`); and
// levelling its `level` cells (offsets, authoredGround().levelOf) to the hotspot height leaves no step above
// `maxStep` to an unchanged neighbour. Flat sites first: the first such site whose ground (original heights of
// the `level` cells and their one-cell ring) spans at most `maxSpan` (FLAT_SPAN) wins; when none lies within
// `flatRadius` (FLAT_RADIUS) rings, the flattest ground of those rings does (first in ring order among equals),
// so a steep site gives way only to a flatter one nearby. The ring keeps a site that is flat inside but stands
// next to a slope from beating one with gentler surroundings. Without any site there, the first ring beyond
// with one decides the same way. `span` is the original span of the `level` cells alone, `groundSpan` with the
// ring. Objects and their flags there are cleared afterwards by clearSite and the `level` cells levelled by
// levelSite; the ground type stays as authored.
function siteFor({N,land,objects,owner,mask,door,level=mask,near:[px,py],blocked,units,piles,seen,radius=30,maxStep=255,
  maxSpan=FLAT_SPAN,flatRadius=FLAT_RADIUS}) {
  const fits=(x,y)=>{
    for(const k of area(N,mask,x,y,1)) {
      const cx=k%N,cy=(k/N)|0,at=4*k;
      if(cx<2||cy<2||cx>=N-2||cy>=N-2||objects[at+1]!==owner||(land[at+2]&64)||(objects[at+2]&0x18)||!grass(land[at+1])
        ||blocked.has(k)||units.has(k)||piles.has(k))return false;
    }
    const k=(y+door[1])*N+x+door[0];
    return objects[4*k+1]===owner&&!!seen[k];
  };
  let flattest=null;
  for(let r=0;r<=radius;r++) {
    for(let y=py-r;y<=py+r;y++)for(let x=px-r;x<=px+r;x++) {
      if(Math.max(Math.abs(x-px),Math.abs(y-py))!==r||!fits(x,y))continue;
      const cells=area(N,level,x,y,0),step=edgeStep(N,land,cells,land[4*(y*N+x)]);
      if(step>maxStep)continue;
      const site={x,y,door:[x+door[0],y+door[1]],ring:r,span:heightSpan(N,land,cells),groundSpan:heightSpan(N,land,withRing(N,cells)),step};
      if(site.groundSpan<=maxSpan)return site;
      if(!flattest||site.groundSpan<flattest.groundSpan)flattest=site;
    }
    if(flattest&&r>=flatRadius)return flattest;
  }
  if(flattest)return flattest;
  throw Error('No site for a building of player '+owner+' near ('+px+','+py+')');
}
// Clear objects and their flags on the footprint and its one-cell halo (the landscape is left to levelSite).
function clearSite(N,objects,mask,x,y) {
  let objectsCleared=0;
  for(const k of area(N,mask,x,y,1)) {if(objects[4*k])objectsCleared++;clearObjectCell(objects,4*k);}
  return objectsCleared;
}
// Free piles, one per entry of `goods`, ring by ring (hex steps) around `anchor`: free owned grass the carriers
// reach from the start, off every footprint and its one-cell halo (`blocked`), off units, at least two steps
// from every other pile. `piles` lists every pile already on the map and grows with the new ones.
function placePiles({N,land,objects,owner,anchor:[ax,ay],goods,amount=PILE_MAX,byte7,blocked,units,piles,seen,radius=40}) {
  return goods.map(good=>{
    for(let r=1;r<radius;r++)for(let y=ay-r;y<=ay+r;y++)for(let x=ax-r;x<=ax+r;x++) {
      if(steps(ax,ay,x,y)!==r)continue;
      const k=y*N+x;
      if(!freeCell(N,land,objects,owner,k)||blocked.has(k)||units.has(k)||!seen[k]||piles.some(([px,py])=>steps(px,py,x,y)<2))continue;
      piles.push([x,y]);return pileRecord(x,y,good,amount,byte7);
    }
    throw Error('No pile cell for good '+good+' of player '+owner+' near ('+ax+','+ay+')');
  });
}
const describePile=r=>({good:r[4],amount:r[5],x:r.readUInt16LE(0),y:r.readUInt16LE(2)});
// Input piles of a toolsmith beside its entrance (placePiles rules): TOOL_INPUTS in piles of 8 and, with `hammers`,
// one pile of TOOL_HAMMERS hammers.
function placeToolsmithPiles(common,entrance,hammers=true) {
  const inputs=placePiles({...common,anchor:entrance,goods:TOOL_INPUTS});
  return hammers?[...inputs,...placePiles({...common,anchor:entrance,goods:[GOOD.HAMMER],amount:TOOL_HAMMERS})]:inputs;
}
// Goods per type summed over described piles: {good: amount}.
function pileTotals(list) {const t={};for(const p of list)t[p.good]=(t[p.good]||0)+p.amount;return t;}
const tally=list=>{const t={};for(const g of list)t[g]=(t[g]||0)+1;return JSON.stringify(Object.entries(t).sort((a,b)=>a[0]-b[0]));};

// Offline checks (tests); they read only the finished map data.
// Piles after the first `from` records: exactly the `expected` goods per owner (owner -> list of goods), free
// piles of 1..8 goods with the map's byte7, on free owned non-Dark grass reachable from that owner's start,
// off every footprint and its one-cell halo, off units, at least two steps from every other pile. `totals` (owner ->
// {good: amount}) also fixes the summed amount of the listed goods.
function checkPiles({N,land,objects,goods,from,byte7,buildings,maskOf,units,starts,expected,totals={}}) {
  assert(goods.length%8===0&&from*8<=goods.length,'Pile records');
  const blocked=footprintCells(N,buildings,maskOf,1),unitCells=recordCells(N,units,6),all=pileList(goods),found={},seen={};
  for(let i=from;i<all.length;i++) {
    const [x,y]=all[i],k=y*N+x,at=8*i,owner=objects[4*k+1];
    assert(x>=2&&y>=2&&x<N-2&&y<N-2,'Added pile outside the map');
    assert(!(land[4*k+2]&64),'Added pile on Dark land');
    assert(expected[owner],'Added pile outside the computer economies');
    assert(goods[at+5]>=1&&goods[at+5]<=PILE_MAX&&goods[at+6]===FREE_PILE&&goods[at+7]===byte7,'Added pile record');
    assert(freeCell(N,land,objects,owner,k),'Added pile not on free owned grass');
    assert(!blocked.has(k),'Added pile under a footprint or its halo');
    assert(!unitCells.has(k),'Added pile on a unit');
    seen[owner]=seen[owner]||reachable(N,land,objects,owner,starts[owner]);
    assert(seen[owner][k],'Added pile unreachable from the start');
    assert(all.every(([px,py],j)=>j===i||steps(px,py,x,y)>=2),'Piles closer than two steps');
    (found[owner]=found[owner]||[]).push(describePile(goods.subarray(at,at+8)));
  }
  for(const [owner,list] of Object.entries(expected))
    assert.strictEqual(tally((found[owner]||[]).map(p=>p.good)),tally(list),'Added goods of player '+owner);
  for(const [owner,want] of Object.entries(totals)) {
    const have=pileTotals(found[owner]||[]);
    for(const [good,amount] of Object.entries(want))assert.strictEqual(have[good]||0,amount,'Added amount of good '+good+' of player '+owner);
  }
  return found;
}
// An added building: footprint and halo inside the map on free owned non-Dark grass (objects cleared), apart
// from every other footprint and its two-cell halo, no unit on the footprint and no pile on footprint or halo,
// entrance owned and reachable from the start.
function checkSite({N,land,objects,record,mask,door,others,maskOf,units,goods,start}) {
  const x=record.readUInt16LE(0),y=record.readUInt16LE(2),owner=record[5];
  assert(record.subarray(6).every(v=>v===0),'Added building carries garrison bytes');
  const blocked=footprintCells(N,others,maskOf,2),unitCells=recordCells(N,units,6),pileCells=recordCells(N,goods,8);
  for(const k of area(N,mask,x,y,1)) {
    assert(!(land[4*k+2]&64),'Added building on Dark land');
    assert(freeCell(N,land,objects,owner,k),'Added building not on clear owned grass');
    assert(!blocked.has(k),'Added building next to another footprint');
    assert(!pileCells.has(k),'Pile on an added building');
  }
  for(const k of area(N,mask,x,y,0))assert(!unitCells.has(k),'Unit on an added building');
  const entrance=[x+door[0],y+door[1]],k=entrance[1]*N+entrance[0];
  assert(objects[4*k+1]===owner&&reachable(N,land,objects,owner,start)[k],'Added entrance not reachable from the start');
  return {x,y,entrance};
}
// An added building on level ground: every `level` cell at the authored (source) height of its hotspot, no
// step above `maxStep` (authoredGround().step) from them to a neighbour outside, and the shading of those cells
// and of the cells above and below them derived from the new heights (shadeOf).
function checkLevel({N,land,sourceLand,record,level,maxStep}) {
  const x=record.readUInt16LE(0),y=record.readUInt16LE(2),hotspot=y*N+x,height=sourceLand[4*hotspot],cells=area(N,level,x,y,0);
  assert(cells.has(hotspot),'Hotspot outside the levelled cells');
  for(const k of cells)assert(land[4*k]===height,'Added building not level at its authored hotspot height');
  const step=edgeStep(N,land,cells);
  assert(step<=maxStep,'Step next to an added building steeper than next to the authored ones');
  for(const k of new Set([...cells,...shadedBy(N,cells)]))
    assert((land[4*k+2]&SHADE)===shadeOf(N,land,k),'Shading next to an added building not derived from its levelled heights');
  return {height,cells:cells.size,step};
}
// Landscape against its source: ground type, byte 3 and the byte 2 flag bits unchanged everywhere; height (byte 0)
// changed only on `levelled` cells; shading (byte 2 bits 0..3) changed only on cells above or below a changed
// height and there derived from the new heights. Returns the numbers of changed heights and shadings.
function checkLandscape(N,land,sourceLand,levelled) {
  assert(land.length===sourceLand.length&&land.length===4*N*N,'Landscape size');
  const raised=new Set();
  for(let at=0;at<land.length;at+=4) {
    assert(land[at+1]===sourceLand[at+1]&&land[at+3]===sourceLand[at+3]&&(land[at+2]&~SHADE)===(sourceLand[at+2]&~SHADE),'Landscape ground or flags changed');
    if(land[at]!==sourceLand[at]){assert(levelled.has(at/4),'Height changed outside an added building');raised.add(at/4);}
  }
  const reshaded=shadedBy(N,raised);let shading=0;
  for(const k of reshaded)assert((land[4*k+2]&SHADE)===shadeOf(N,land,k),'Shading not derived from the levelled heights');
  for(let k=0;k<N*N;k++)if(land[4*k+2]!==sourceLand[4*k+2]){assert(reshaded.has(k),'Shading changed away from a levelled height');shading++;}
  return {heights:raised.size,shading};
}
// Units after the first `from` records: `count` carriers per listed owner, each on free owned non-Dark grass
// reachable from that owner's start, off every footprint, off piles and off every other unit.
function checkCarriers({N,land,objects,units,from,owners,count,buildings,maskOf,goods,starts}) {
  const footprint=footprintCells(N,buildings,maskOf,0),pileCells=recordCells(N,goods,8),cells=new Map(),found={},seen={};
  for(let i=0;i<units.length;i+=6){const k=units.readUInt16LE(i+2)*N+units.readUInt16LE(i);cells.set(k,(cells.get(k)||0)+1);}
  assert.strictEqual(units.length/6-from,owners.length*count,'Added carrier count');
  for(let i=6*from;i<units.length;i+=6) {
    const x=units.readUInt16LE(i),y=units.readUInt16LE(i+2),k=y*N+x,owner=units[i+5];
    assert(units[i+4]===CARRIER&&owners.includes(owner),'Added unit is not a carrier of a computer player');
    assert(!(land[4*k+2]&64),'Added carrier on Dark land');
    assert(freeCell(N,land,objects,owner,k),'Added carrier not on free owned grass');
    assert(!footprint.has(k)&&!pileCells.has(k)&&cells.get(k)===1,'Added carrier on a building, pile or unit');
    seen[owner]=seen[owner]||reachable(N,land,objects,owner,starts[owner]);
    assert(seen[owner][k],'Added carrier unreachable from the start');
    found[owner]=(found[owner]||0)+1;
  }
  for(const owner of owners)assert(found[owner]===count,'Carriers of player '+owner);
  return found;
}
module.exports={GOOD,PILE_MAX,FREE_PILE,TOOLSMITH,BARRACKS,CARRIER,TOOL_INPUTS,TOOL_HAMMERS,TOOL_TOTALS,TOOL_GOODS,placeToolsmithPiles,pileTotals,steps,freeCell,reachable,footprintCells,recordCells,pileList,pileByte7,
  pileRecord,buildingRecord,unitRecord,MINES,FLAT_SPAN,FLAT_RADIUS,SHADE,shadeOf,edgeStep,heightSpan,authoredGround,levelSite,siteFor,clearSite,placePiles,describePile,
  checkPiles,checkSite,checkLevel,checkLandscape,checkCarriers};

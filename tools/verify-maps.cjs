const fs=require('fs'),path=require('path'),assert=require('assert'),crypto=require('crypto');
const {readMap,writeMap,encode,decode}=require('./map-format.cjs');
const game=process.argv[2]||require('./project-paths.cjs').gameDir();
const results=[];
for(let i=1;i<=12;i++) {
  const file=path.join(game,'Map/Campaign','dark'+String(i).padStart(2,'0')+'.map');
  const m=readMap(file);
  assert(writeMap(m).equals(m.raw),'Byte identical preservation failed');
  for(const c of m.chunks.filter(c=>[1,2,3,7,8,9,10,11,12,14,15,16].includes(c.type))) assert(decode(encode(c.data),c.data.length).equals(c.data));
  const info=m.chunks.find(c=>c.type===1).data;
  const players=m.chunks.find(c=>c.type===2).data;
  const buildings=m.chunks.find(c=>c.type===8).data;
  const playerRows=[];
  for(let p=0;p<players.length;p+=45) playerRows.push({player:p/45+1,race:players.readUInt32LE(p),x:players.readUInt32LE(p+4),y:players.readUInt32LE(p+8)});
  const buildingRows=[];
  for(let p=0;p<buildings.length;p+=20) buildingRows.push({x:buildings.readUInt16LE(p),y:buildings.readUInt16LE(p+2),type:buildings[p+4],player:buildings[p+5]});
  results.push({file,sha256:crypto.createHash('sha256').update(m.raw).digest('hex'),size:info.readUInt32LE(12),chunks:m.chunks.length,players:playerRows,buildings:buildingRows,byteIdenticalRoundTrip:true,recompressionPassed:true});
}
const project=path.resolve(__dirname,'..');fs.mkdirSync(path.join(project,'research'),{recursive:true});
fs.writeFileSync(path.join(project,'research/map-validation.json'),JSON.stringify(results,null,2));
console.log('PASS: '+results.length+' maps, byte-identical reconstruction and metadata recompression');

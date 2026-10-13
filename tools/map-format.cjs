// Map/resource format: MIT Settlers.ts by Thomas Zeugner; see vendor license.
// Headers are resource obfuscation, unrelated to game ownership or DRM.
const fs = require('fs');
const {BinaryReader} = require('./vendor/settlers-ts/resources/file/binary-reader');
const {AraCrypt} = require('./vendor/settlers-ts/resources/file/ara-crypt');
const {ChecksumCalculator} = require('./vendor/settlers-ts/resources/file/checksum-calculator');
const {Decompress} = require('./vendor/settlers-ts/resources/file/decompress');
const {Compression} = require('./vendor/settlers-ts/resources/file/compress');
const {LogHandler} = require('./vendor/settlers-ts/utilities/log-handler');

function checksum(buf) {return ChecksumCalculator.calc(new BinaryReader(new Uint8Array(buf)));}
function crypt(buf) {
  const ara = new AraCrypt(); ara.reset(0x30313233, 0x34353637, 0x38393031);
  return Buffer.from(buf.map(b => b ^ ara.getNextKey()));
}
function decode(packed, length) {
  if(length > 32*1024*1024) throw Error('Chunk exceeds 32 MB limit');
  const logs=LogHandler.getLogManager().log;
  const start=logs.length;
  const result=Buffer.from(new Decompress().unpack(new BinaryReader(new Uint8Array(packed)),0,packed.length,length).getBuffer());
  if(result.length!==length || logs.slice(start).some(l=>l.type===0)) throw Error('Invalid compressed stream');
  return result;
}
function encode(data) {
  // Literal-only encoder avoids relying on the upstream LZ distance writer.
  // Large unchanged chunks keep their original compressed bytes.
  const packer=new Compression(); packer.find=()=>({matchLen:0,offset:0});
  const packed=Buffer.from(packer.pack(new Uint8Array(data),data.length).getBuffer());
  if(!decode(packed,data.length).equals(data)) throw Error('Compression round trip failed');
  return packed;
}
function readMap(file) {
  const raw=fs.readFileSync(file);
  if(raw.length<32 || raw.readUInt32LE(0)!==checksum(raw.subarray(8))) throw Error('Invalid file checksum');
  let pos=8;
  const chunks=[];
  while(pos+24<=raw.length) {
    const header=crypt(raw.subarray(pos,pos+24));
    const type=header.readUInt32LE(0),len=header.readUInt32LE(4);
    if(type===0) return {raw,version:raw.readUInt32LE(4),chunks,tail:raw.subarray(pos)};
    if(pos+24+len>raw.length) throw Error('Chunk outside file');
    const packed=raw.subarray(pos+24,pos+24+len);
    if(checksum(packed)!==header.readUInt32LE(12)) throw Error('Invalid chunk checksum');
    chunks.push({type,header,packed,data:decode(packed,header.readUInt32LE(8))});
    pos+=24+len;
  }
  throw Error('Missing end marker');
}
function writeMap(map, changes=new Map()) {
  const pieces=[];
  for(const c of map.chunks) {
    let header=Buffer.from(c.header),packed=c.packed;
    if(changes.has(c.type)) {
      const change=changes.get(c.type); packed=encode(change.data);
      header.writeUInt32LE(packed.length,4);header.writeUInt32LE(change.data.length,8);header.writeUInt32LE(checksum(packed),12);
      if(change.count!==undefined) header.writeUInt32LE(change.count,20);
    }
    pieces.push(crypt(header),packed);
  }
  pieces.push(map.tail);
  const body=Buffer.concat(pieces),head=Buffer.alloc(8);
  head.writeUInt32LE(checksum(body),0);head.writeUInt32LE(map.version,4);
  return Buffer.concat([head,body]);
}
module.exports={readMap,writeMap,decode,encode,checksum,crypt};

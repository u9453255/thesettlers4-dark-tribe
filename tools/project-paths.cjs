// Game folder and Python for the map tools; same rules as tools/project_paths.py:
// DARK_TRIBE_GAME_DIR, then "game_dir" in local-config.json, then C:/Projects/thesettlers4.
const fs=require('fs'),path=require('path');
const root=path.resolve(__dirname,'..');
function config() {
  try {return JSON.parse(fs.readFileSync(path.join(root,'local-config.json'),'utf8'));}
  catch(error) {if(error.code==='ENOENT') return {};throw error;}
}
function gameDir() {
  for(const [source,value] of [['DARK_TRIBE_GAME_DIR',process.env.DARK_TRIBE_GAME_DIR],['local-config.json',config().game_dir]]) {
    if(!value) continue;
    if(!fs.existsSync(path.join(value,'S4_Main.exe'))) throw new Error(source+' points to '+value+', which has no S4_Main.exe');
    return value;
  }
  const legacy='C:/Projects/thesettlers4';
  if(fs.existsSync(path.join(legacy,'S4_Main.exe'))) return legacy;
  throw new Error('The Settlers IV game folder is not configured. Run python tools/setup.py --game "<folder containing S4_Main.exe>" or set DARK_TRIBE_GAME_DIR.');
}
const python=()=>process.env.DARK_TRIBE_PYTHON||config().python||'python';
module.exports={root,gameDir,python};

"""Install or restore only this scenario's files, preserving a verified backup."""
import argparse, hashlib, json, shutil
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
def digest(p): return hashlib.sha256(p.read_bytes()).hexdigest()

def targets(game):
    """Installed files in manifest order: compatibility XML, First Dawn map and script copy, then the other scenario maps."""
    from scenarios import SCENARIOS
    first=SCENARIOS[0]
    return [(ROOT/'work/generated/GameData/buildingInfo.xml',game/'GameData/buildingInfo.xml'),
            (ROOT/first['generated'],game/first['installed']),(ROOT/first['script'],game/'Script/DarkTribe_FirstDawn.txt'),
            *[(ROOT/s['generated'],game/s['installed']) for s in SCENARIOS[1:]]]

def add_entry(entries,src,dst):
    """Back up whatever already exists at the destination, then record the file to install there."""
    backup=ROOT/'backups'/('original-'+str(len(entries))+'-'+dst.name)
    existed=dst.exists()
    if existed: shutil.copy2(dst,backup)
    entries.append(dict(source=str(src),destination=str(dst.resolve()),backup=str(backup),existed=existed,original_sha256=digest(backup) if existed else None,installed_sha256=digest(src)))

def upgrade(manifest,game):
    """Add files of scenarios released after this installation (existing entries stay untouched)."""
    known={str(Path(e['destination']).resolve()).lower() for e in manifest['entries']}
    added=[]
    for src,dst in targets(game):
        if str(dst.resolve()).lower() not in known:
            add_entry(manifest['entries'],src,dst);added.append(dst.name)
    return added

def install(game, profile):
    report=json.loads((ROOT/'research/building-compatibility.json').read_text())
    destination=game/'GameData/buildingInfo.xml'
    source=ROOT/'work/generated/GameData/buildingInfo.xml'
    if digest(source)!=report['output_sha256']: raise RuntimeError('Generated XML hash mismatch')
    manifest_path=ROOT/'backups/install-manifest.json'
    if manifest_path.exists():
        manifest=json.loads(manifest_path.read_text())
        if manifest['game']!=str(game.resolve()): raise RuntimeError('Backup belongs to another game folder')
        if upgrade(manifest,game): manifest_path.write_text(json.dumps(manifest,indent=2))
    else:
        if digest(destination)!=report['source_sha256']: raise RuntimeError('Unexpected game XML; regenerate against this installation')
        entries=[];(ROOT/'backups').mkdir(exist_ok=True)  # absent in a fresh checkout
        for src,dst in targets(game): add_entry(entries,src,dst)
        manifest=dict(game=str(game.resolve()),entries=entries)
        manifest_path.write_text(json.dumps(manifest,indent=2))
    (ROOT/'save-states').mkdir(exist_ok=True)
    (ROOT/'evidence').mkdir(exist_ok=True)
    # Check every destination before modifying any of them.
    for e in manifest['entries']:
        dst=Path(e['destination'])
        if e['existed'] and digest(Path(e['backup']))!=e['original_sha256']:raise RuntimeError('Original backup hash mismatch')
        if dst.exists() and digest(dst) not in (e['original_sha256'],e['installed_sha256'],*e.get('previous_sha256',[])): raise RuntimeError('Unrelated changes in '+str(dst))
        if digest(Path(e['source']))!=e['installed_sha256']: raise RuntimeError('Regenerated content differs; update manifest explicitly')
    for e in manifest['entries']:
        src=Path(e['source']);dst=Path(e['destination'])
        if digest(src)!=e['installed_sha256']: raise RuntimeError('Regenerated content differs; update manifest explicitly')
        dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst)
        assert digest(dst)==e['installed_sha256']
    (ROOT/'runtime').mkdir(exist_ok=True)
    (ROOT/'runtime/active.json').write_text(json.dumps(dict(version='0.2.15',game=str(game.resolve()))))
    print('Installed the scenarios and compatibility XML; originals preserved in',ROOT/'backups')

def restore():
    manifest=json.loads((ROOT/'backups/install-manifest.json').read_text())
    for e in manifest['entries']:
        dst=Path(e['destination'])
        if dst.exists() and digest(dst) not in (e['installed_sha256'],*e.get('previous_sha256',[])): raise RuntimeError('File changed after install: '+str(dst))
        if e['existed'] and digest(Path(e['backup']))!=e['original_sha256']: raise RuntimeError('Backup hash mismatch')
    for e in manifest['entries']:
        dst=Path(e['destination'])
        if e['existed']: shutil.copy2(e['backup'],dst)
        else: dst.unlink(missing_ok=True)
    (ROOT/'runtime/active.json').unlink(missing_ok=True)
    print('Restored original XML and removed only the scenarios of this mod')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('action',choices=['install','restore']);p.add_argument('--game',type=Path,default=None);p.add_argument('--profile',type=Path,default=Path.home()/'Documents/TheSettlers4');a=p.parse_args()
    if a.action=='install':
        from project_paths import game_dir
        install(a.game or game_dir(),a.profile)
    else:restore()

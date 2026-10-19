"""Prepare an upgrade manifest without changing the installed game or launching it."""
import hashlib,json
from pathlib import Path
from install import upgrade
ROOT=Path(__file__).resolve().parents[1]
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def stage():
    path=ROOT/'backups/install-manifest.json'
    if not path.exists():
        print('Nothing installed yet: Launch Dark Tribe.cmd installs this release on its first run. Game files unchanged.');return
    m=json.loads(path.read_text())
    added=upgrade(m,Path(m['game']))  # scenarios released after the first installation
    for e in m['entries']:
        dst=Path(e['destination'])
        accepted=[e['installed_sha256'],e['original_sha256'],*e.get('previous_sha256',[])]
        if dst.exists() and digest(dst) not in accepted:raise RuntimeError('Unrelated game changes: '+str(dst))
        if e['existed'] and digest(Path(e['backup']))!=e['original_sha256']:raise RuntimeError('Original backup changed')
    for e in m['entries']:
        new=digest(Path(e['source']))
        if new!=e['installed_sha256']:
            previous=e.setdefault('previous_sha256',[])
            if e['installed_sha256'] not in previous:previous.append(e['installed_sha256'])
            e['installed_sha256']=new
    m['version']='0.2.15';m['activation']='installed when the user runs Launch Dark Tribe.cmd'
    path.write_text(json.dumps(m,indent=2))
    print('Upgrade staged'+(' (new: '+', '.join(added)+')' if added else '')+'. Game files unchanged; activation is through the user launcher.')
if __name__=='__main__':stage()

"""Execute the actual Lua victory/start logic with mocked APIs; never the game."""
import importlib.util,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'work/python'))  # tools/setup.py installs lupa here when it is missing
from lupa import LuaRuntime
source=(ROOT/'scenario/DarkTribe_FirstDawn.txt').read_text()

def fixture():
    lua=LuaRuntime()
    lua.execute('''
      counts = {}; lost = {}; endChecks = 0; settings = {}; grants = 0
      Buildings = { DARKTEMPLE=50, GUARDTOWERSMALL=46, GUARDTOWERBIG=47, CASTLE=48, READY=1 }
      function Buildings.Amount(p,b,state) return counts[p*100+b] or 0 end
      Game = {}
      function Game.EnemyPlayersLost(p) lost[#lost+1]=p end
      function Game.DefaultGameEndCheck() endChecks=endChecks+1 end
      AI = {}
      function AI.SetPlayerVar(p,k,a,b,c) settings[k]={p,a,b,c} end
      Magic = {}; function Magic.IncreaseMana(p,n) grants=grants+1 end
      Map = {}; function Map.SetScreenPos(x,y) end
      dbg = {}; function dbg.stm(s) end
    ''')
    lua.execute(source)
    return lua,lua.globals()

def run():
    lua,g=fixture();g.DT_Victory();assert g.DT_VictoryArmed==0 and g.endChecks==0 and len(g.lost)==0
    g.counts[150]=1;g.DT_Victory();assert g.endChecks==0 and len(g.lost)==0
    g.counts[246]=1;g.DT_Victory();assert g.DT_VictoryArmed==1 and len(g.lost)==0 and g.endChecks==0
    g.counts[246]=0;g.counts[247]=3;g.DT_Victory();assert len(g.lost)==0 and g.endChecks==0
    g.counts[247]=0;g.DT_Victory();assert g.lost[1]==1 and g.endChecks==1
    lua,g=fixture();g.counts[150]=1;g.counts[246]=1;g.DT_Victory()
    g.counts[150]=0;g.counts[151]=1;g.DT_Victory();assert len(g.lost)==0 and g.endChecks==0
    g.counts[151]=0;g.DT_Victory();assert g.lost[1]==2 and g.endChecks==1
    lua,g=fixture();g.DT_Init()
    assert g.grants==1 and list(g.settings.AttackMode.values())==[2,2,2,2]
    g.DT_Resume();assert g.grants==1  # Loading does not inject starting resources or change AI orders.
    spec=importlib.util.spec_from_file_location('activate',ROOT/'tools/activate-human-slot.py')
    activate=importlib.util.module_from_spec(spec);spec.loader.exec_module(activate)
    report={'playerCount':2,'players':[{'0x18c':3,'0xfc':277,'0x120':214},{'0x18c':1,'0xfc':75,'0x120':401}]}
    assert activate.matches_layout(report)
    report['players'][1].update({'0xfc':261,'0x120':279});assert activate.matches_layout(report)
    for x,y in [(75,279),(261,401),(76,401),(340,300)]:
        report['players'][1].update({'0xfc':x,'0x120':y});assert not activate.matches_layout(report)
    report['playerCount']=3;assert not activate.matches_layout(report)
    report['players']=[];assert not activate.matches_layout(report)
    checks=['actual Lua: startup zero counts do not decide victory','both bases arm victory; last enemy tower and last Dark base decide correct winner',
            'Fortress and additional enemy towers prevent premature defeat','native AI settings and one initial grant; resume adds none',
            'exact new/legacy layouts accepted; mismatched coordinates/counts rejected']
    (ROOT/'research/opponent-offline-checks.json').write_text(json.dumps({'version':'0.2.15','opponentRevision':2,'checks':checks,'game_started':False,'runtimeVerified':False},indent=2))
    for check in checks:print('PASS:',check)

if __name__=='__main__':run()

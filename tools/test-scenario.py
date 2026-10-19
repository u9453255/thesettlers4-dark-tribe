"""Execute the actual Lua victory/start logic with mocked APIs; never the game."""
import importlib.util,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'work/python'))  # tools/setup.py installs lupa here when it is missing
from lupa import LuaRuntime
from scenarios import SCENARIOS, layouts
SOURCES={s['key']:(ROOT/s['script']).read_text() for s in SCENARIOS}
# Expected Viking AttackMode per scenario: First Dawn as the shipped default, Iron Kingdom at the aggressive end.
ATTACK={'first_dawn':[2,2,2,2],'iron_kingdom':[2,2,3,3],'alliance':[4,2,3,3]}

def fixture(source):
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

def victory(key):
    source=SOURCES[key]
    lua,g=fixture(source);g.DT_Victory();assert g.DT_VictoryArmed==0 and g.endChecks==0 and len(g.lost)==0
    g.counts[150]=1;g.DT_Victory();assert g.endChecks==0 and len(g.lost)==0
    g.counts[246]=1;g.DT_Victory();assert g.DT_VictoryArmed==1 and len(g.lost)==0 and g.endChecks==0
    g.counts[246]=0;g.counts[247]=3;g.DT_Victory();assert len(g.lost)==0 and g.endChecks==0
    g.counts[247]=0;g.DT_Victory();assert g.lost[1]==1 and g.endChecks==1
    lua,g=fixture(source);g.counts[150]=1;g.counts[246]=1;g.DT_Victory()
    g.counts[150]=0;g.counts[151]=1;g.DT_Victory();assert len(g.lost)==0 and g.endChecks==0
    g.counts[151]=0;g.DT_Victory();assert g.lost[1]==2 and g.endChecks==1
    if key=='alliance':  # three allied opponents: a tower of any of them keeps the game going
        lua,g=fixture(source);g.counts[150]=1;g.counts[347]=1;g.DT_Victory();assert g.DT_VictoryArmed==1
        for alive in (247,348,446,447):
            g.counts[347]=0;g.counts[alive]=1;g.DT_Victory();assert len(g.lost)==0 and g.endChecks==0;g.counts[alive]=0
        g.DT_Victory();assert g.lost[1]==1 and g.endChecks==1
    lua,g=fixture(source);g.DT_Init()
    assert g.grants==1 and list(g.settings.AttackMode.values())==ATTACK[key] and g.DT_IsNew==1
    g.DT_Resume();assert g.grants==1  # Loading does not inject starting resources or change AI orders.

def run():
    for key in SOURCES:victory(key)
    spec=importlib.util.spec_from_file_location('activate',ROOT/'tools/activate-human-slot.py')
    activate=importlib.util.module_from_spec(spec);spec.loader.exec_module(activate)
    def report(starts,size=None):
        r={'playerCount':len(starts),'players':[{'0x18c':race,'0xfc':x,'0x120':y} for race,x,y in starts]}
        if size:r['mapSize']=size
        return r
    for size,starts in layouts():  # every scenario and the legacy First Dawn layout, with and without the size
        assert activate.matches_layout(report(starts)) and activate.matches_layout(report(starts,size))
    assert activate.matches_layout(report([(3,277,214),(1,261,279)]))
    for x,y in [(75,279),(261,401),(76,401),(340,300),(475,353)]:  # First Dawn with a wrong opponent start
        assert not activate.matches_layout(report([(3,277,214),(1,x,y)]))
    alliance=[(3,303,423),(0,98,353),(1,367,116),(2,260,263)]
    # In the lobby the map size is still 0 or the previous map's; the slot must be assigned there anyway.
    for stale in (0,576,512):assert activate.matches_layout(report(alliance,stale)) and activate.matches_layout(report([(3,277,214),(1,75,401)],stale))
    assert not activate.matches_layout(report(alliance[:2])) and not activate.matches_layout(report(alliance[:3]))
    assert not activate.matches_layout(report([alliance[0],alliance[2],alliance[1],alliance[3]]))  # players swapped
    assert not activate.matches_layout(report([(3,378,134),(1,75,401)])) and not activate.matches_layout(report([(4,378,134),(1,475,353)]))
    r=report(alliance);r['playerCount']=3;assert not activate.matches_layout(r)
    assert not activate.matches_layout({'playerCount':0,'players':[]})
    checks=['actual Lua of every scenario: startup zero counts do not decide victory','both bases arm victory; last enemy tower and last Dark base decide correct winner',
            'Fortress and additional enemy towers prevent premature defeat','native AI settings and one initial grant; resume adds none',
            'exact layouts of every scenario (First Dawn new/legacy, Iron Kingdom, The Alliance) accepted in the lobby whatever the stale map size; mixed, swapped, coordinates and counts rejected']
    (ROOT/'research/opponent-offline-checks.json').write_text(json.dumps({'version':'0.2.15','opponentRevision':3,'checks':checks,'game_started':False,'runtimeVerified':False},indent=2))
    for check in checks:print('PASS:',check)

if __name__=='__main__':run()

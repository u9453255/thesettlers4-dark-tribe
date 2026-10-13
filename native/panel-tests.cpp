#include "panel.hpp"
#include <assert.h>
#include <limits.h>
#include <set>
#include <string>
#include <vector>
// Game folder from DARK_TRIBE_GAME_DIR (tools/build-native.py sets it); empty when not configured.
static std::wstring gameFolder(){wchar_t d[MAX_PATH];DWORD n=GetEnvironmentVariableW(L"DARK_TRIBE_GAME_DIR",d,MAX_PATH);if(!n||n>=MAX_PATH)return L"";std::wstring s(d);if(s.back()!=L'\\'&&s.back()!=L'/')s+=L'\\';return s;}
// Hit targets on the native layout, then rendering: in every mode the whole
// covered panel (x 0..199, y 210..599) and the mana section below it (y
// 600..767, drawn in every mode) must be opaque and never pure black (the HD
// colour key), with nothing drawn outside them. Rendering is checked
// with the drawn fallback and, when the game files are present, with the
// native sprites. Finally the layout check: every text stays inside its frame,
// whole, and clear of the sprites and texts around it (worst-case data).
static uint32_t*canvas=nullptr;static HDC canvasDc=nullptr;constexpr int W=200,H=780;
static void opaque(int y0,int y1,const dtui::Data&v,bool build){
 for(int y=y0;y<y1;y++)for(int x=dtui::BoxX;x<dtui::BoxX+dtui::BoxW;x++)if(!(canvas[y*W+x]&0xffffff)){printf("colour-key pixel at %d,%d (art=%d build=%d kind=%u site=%d aircraft=%d units=%d)\n",x,y,(int)dtui::art().ok,build,v.kind,v.site,v.isAircraft,v.unitPanel);fflush(stdout);assert(!"covered panel has a pure black pixel");}
}
static bool blank(int y){for(int x=0;x<W;x++)if(canvas[y*W+x]&0xffffff)return false;return true;}
// One Dark page plus the mana section, as the HD canvas and the classic frame draw them.
static void covered(const dtui::Data&v,bool build){
 for(int i=0;i<W*H;i++)canvas[i]=0;dtui::render(canvasDc,0,v,build);dtui::manaSection(canvasDc,0,v);GdiFlush();
 opaque(dtui::BoxY,dtui::BoxY+dtui::BoxH,v,build);opaque(dtui::ManaY,dtui::ManaY+dtui::ManaH,v,build);
 assert(dtui::BoxY+dtui::BoxH==dtui::ManaY&&blank(205)&&blank(dtui::ManaY+dtui::ManaH)&&blank(H-1));
}
// The mana section alone (a native menu shows above it), with and without data.
static void manaOnly(const dtui::Data&v){
 for(int i=0;i<W*H;i++)canvas[i]=0;dtui::manaSection(canvasDc,0,v);GdiFlush();
 opaque(dtui::ManaY,dtui::ManaY+dtui::ManaH,v,false);for(int y=0;y<dtui::ManaY;y++)assert(blank(y));assert(blank(dtui::ManaY+dtui::ManaH));
}
// Manacopter panel data: n picked aircraft (the first six shown) with varied
// health and landed state, `load` passengers each; `types` passenger types of `count` each.
static void fleet(dtui::Data&v,int n,int load,int types,uint16_t count){
 v.kind=0;v.isAircraft=true;v.group=(uint32_t)n;v.airShown=(uint8_t)std::min(n,6);v.groupPassengers=(uint32_t)(n*load);v.passengers=(uint32_t)load;v.groupUnload=load>0;v.landed=true;
 for(int i=0;i<6;i++){v.airHp[i]=(uint8_t)(i==1?1:100-i*13);v.airLoad[i]=(uint8_t)load;v.airLanded[i]=i%2==0;v.airId[i]=(uint16_t)(100+i);v.airIdentity[i]=5000u+i;}
 v.groupHp=v.aircraftHp=100;const uint16_t kinds[4]={29,32,52,54};v.riderTypes=(uint8_t)types;for(int i=0;i<4;i++){v.riderKind[i]=i<types?kinds[i]:0;v.riderCount[i]=i<types?count:0;}
}
static void renderAll(){
 dtui::Data r;r.ready=true;r.mana=48;r.recruitmentReady=true;r.queues[4]=12;r.queues[5]=100;strcpy(r.message,"A long message that wraps onto a second line of the panel and is clipped at the bottom edge of its box.");
 covered(r,true);r.mana=0;covered(r,true);r.mana=480;covered(r,true);
 r.buildLimit[1]=dtui::TileBuilt;r.buildLimit[2]=dtui::TileBuilding;covered(r,true);r.mana=0;covered(r,true);r.buildLimit[1]=dtui::TileBuilding;r.buildLimit[2]=dtui::TileBuilt;covered(r,true);r.buildLimit[1]=r.buildLimit[2]=0;r.mana=48;
 for(int k:{49,50,51,80}){r.kind=(uint16_t)k;covered(r,false);r.site=true;covered(r,false);r.site=false;}
 r.kind=50;for(int p=0;p<4;p++){r.recruitPick=(uint8_t)p;covered(r,false);}r.recruitPick=0;
 r.kind=51;r.recruitmentReady=false;covered(r,false);r.kind=80;r.aircraftCount=2;r.progress=150;covered(r,false);
 /* Manacopters: one (with and without a notice), a group of twelve, unload targeting, nobody aboard. */
 fleet(r,1,6,4,6);covered(r,false);r.message[0]=0;covered(r,false);fleet(r,12,6,4,18);covered(r,false);r.unloadTargeting=true;covered(r,false);
 fleet(r,3,0,0,0);r.unloadTargeting=false;covered(r,false);r.airLanded[0]=false;r.landed=false;covered(r,false);
 r.isAircraft=false;r.unitPanel=true;covered(r,false);
 dtui::Data idle;idle.ready=true;idle.mana=UINT32_MAX;manaOnly(idle);manaOnly(dtui::Data{});
 // The plugin stopped: failure panel and an empty mana section.
 for(int i=0;i<W*H;i++)canvas[i]=0;dtui::failure(canvasDc,0);dtui::manaSection(canvasDc,0,dtui::Data{});GdiFlush();opaque(dtui::BoxY,dtui::ManaY+dtui::ManaH,idle,false);assert(blank(205)&&blank(dtui::ManaY+dtui::ManaH));
}
// ---- Layout check ------------------------------------------------------------------
struct Seen {std::string text;dtui::Font font;RECT box,ink;bool wrap,message;SIZE extent;int drawn;};
static std::vector<Seen> labels;static std::vector<std::pair<RECT,int>> parts;static const char*messageText=nullptr;
static HDC maskDc=nullptr;static uint32_t*mask=nullptr;constexpr int MW=520,MOX=160,MOY=100,MH=H+2*MOY; // room around the whole canvas for ink beyond a box
static bool empty(const RECT&r){return r.right<=r.left||r.bottom<=r.top;}
static bool within(const RECT&a,const RECT&b){return a.left>=b.left&&a.top>=b.top&&a.right<=b.right&&a.bottom<=b.bottom;}
static bool overlaps(const RECT&a,const RECT&b){return !empty(a)&&!empty(b)&&a.left<b.right&&b.left<a.right&&a.top<b.bottom&&b.top<a.bottom;}
static RECT inset(RECT r,int d){return {r.left+d,r.top+d,r.right-d,r.bottom-d};}
// Pixel bounds of the text alone, drawn white on black, plus the 1-px shadow
// right and below; empty for an empty string. Single lines and fixed copy are
// drawn unclipped (what the glyphs need: anything beyond the box would be cut
// off); notices exactly as drawn (clipped, whole lines only), with the height
// DrawText reports.
static RECT inkOf(const dtui::LabelProbe&p,bool message,int&drawn){
 for(int i=0;i<MW*MH;i++)mask[i]=0;SelectObject(maskDc,dtui::font(p.font));SetTextColor(maskDc,RGB(255,255,255));SetBkMode(maskDc,TRANSPARENT);
 RECT r=p.box;OffsetRect(&r,MOX+dtui::leadIn(maskDc,p.text,p.format),MOY);drawn=DrawTextA(maskDc,p.text,-1,&r,message?p.format:p.format|DT_NOCLIP);GdiFlush();
 RECT b{INT_MAX,INT_MAX,INT_MIN,INT_MIN};
 for(int y=0;y<MH;y++)for(int x=0;x<MW;x++){uint32_t c=mask[y*MW+x];int v=std::max({(int)(c>>16)&255,(int)(c>>8)&255,(int)c&255});if(v<0x40)continue;b.left=std::min<LONG>(b.left,x);b.top=std::min<LONG>(b.top,y);b.right=std::max<LONG>(b.right,x+2);b.bottom=std::max<LONG>(b.bottom,y+2);}
 if(b.left==INT_MAX)return {0,0,0,0};OffsetRect(&b,-MOX,-MOY);return b;
}
static void onLabel(HDC,const dtui::LabelProbe&p){bool message=p.text==messageText;int drawn=0;RECT ink=inkOf(p,message,drawn);labels.push_back({p.text,p.font,p.box,ink,p.wrap,message,p.extent,drawn});}
// Drawn parts: sprites by their opaque pixels (alpha >= 50%), drawn stand-ins by their rectangle; backgrounds are skipped.
static void onPart(const RECT&r,int what){
 if(what==dtgfx::Panel||what==dtgfx::ResourcePanel||what==dtgfx::HouseTabOn)return;
 RECT b=r;if(what>=0){const auto&m=dtui::art()[(dtgfx::Art)what];b={INT_MAX,INT_MAX,INT_MIN,INT_MIN};
  for(int y=0;y<m.h;y++)for(int x=0;x<m.w;x++)if((m.at(x,y)>>24)>=0x80){b.left=std::min<LONG>(b.left,r.left+x);b.top=std::min<LONG>(b.top,r.top+y);b.right=std::max<LONG>(b.right,r.left+x+1);b.bottom=std::max<LONG>(b.bottom,r.top+y+1);}
  if(b.left==INT_MAX)return;}
 parts.push_back({b,what});}
// The construction page has no sub-tab strip: it draws the content panel #589
// (with its title) like every page, and no tab, strip or page sprite.
static std::vector<int> drawnParts;static void onAnyPart(const RECT&,int what){drawnParts.push_back(what);}
static void noSubTabs(){
 dtui::Data b;b.ready=true;b.mana=500;drawnParts.clear();dtui::partProbe=onAnyPart;for(int i=0;i<W*H;i++)canvas[i]=0;dtui::render(canvasDc,0,b,true);GdiFlush();dtui::partProbe=nullptr;
 auto has=[](int a){return std::find(drawnParts.begin(),drawnParts.end(),a)!=drawnParts.end();};
 for(int a:{(int)dtgfx::Strip,(int)dtgfx::Page,(int)dtgfx::TabBuildings,(int)dtgfx::TabBuildingsOn,(int)dtgfx::TabAircraft,(int)dtgfx::TabAircraftOn,(int)dtui::PartTab})assert(!has(a));
 assert(has(dtgfx::Panel)==dtui::art().ok);
}
static const char*fontName(dtui::Font f){static const char*n[]={"F1","F3","F4","F9","F11","F14"};return n[f];}
static std::string show(const RECT&r){char s[64];snprintf(s,sizeof(s),"[%ld,%ld..%ld,%ld)",r.left,r.top,r.right,r.bottom);return s;}
static int failures=0,notes=0;static bool verbose=false;static std::set<std::string> noted;
// Temple/Fortress recruitment rows (below the separator, plus the Clear button)
// belong to a separate redesign: reported as notes until strictRecruitment is set.
constexpr bool strictRecruitment=true;
static bool recruitment(const dtui::Data&v,bool build,const RECT&b){
 return !build&&!v.unitPanel&&!v.isAircraft&&!v.site&&(v.kind==50||v.kind==51)&&(b.top>=dtui::SeparatorY||(b.left>=dtui::SecondX&&b.top>=dtui::SecondY&&b.top<dtui::SeparatorY));
}
static void layout(const char*name,const dtui::Data&v,bool build){
 labels.clear();parts.clear();messageText=v.message;dtui::labelProbe=onLabel;dtui::partProbe=onPart;
 for(int i=0;i<W*H;i++)canvas[i]=0;dtui::render(canvasDc,0,v,build);dtui::manaSection(canvasDc,0,v);GdiFlush();dtui::labelProbe=nullptr;dtui::partProbe=nullptr;
 const RECT title{dtui::InnerX0,dtui::TitleY0,dtui::InnerX1,dtui::TitleY1},content{dtui::InnerX0,dtui::InnerY0,dtui::InnerX1,dtui::InnerY1},mana{dtui::InnerX0,dtui::ManaInnerY0,dtui::InnerX1,dtui::ManaInnerY1};
 auto report=[&](const Seen&l,const char*what,const std::string&detail){
  bool other=!strictRecruitment&&recruitment(v,build,l.box);if(other&&!noted.insert(what+l.text).second){notes++;return;} /* each note once */
  printf("%s [%s/%s] %s: \"%s\" %s box %s ink %s: %s\n",other?"NOTE (recruitment block)":"FAIL",dtui::art().ok?"art":"drawn",name,what,l.text.c_str(),fontName(l.font),show(l.box).c_str(),show(l.ink).c_str(),detail.c_str());
  if(other)notes++;else failures++;};
 for(size_t i=0;i<labels.size();i++){const Seen&l=labels[i];
  int button=-1;for(size_t k=0;k<parts.size();k++)if(parts[k].second==dtui::PartButton&&within(l.box,parts[k].first))button=(int)k;
  RECT area=button>=0?inset(parts[button].first,2):l.box.top>=dtui::ManaY?mana:l.box.top<dtui::InnerY0-4?title:content;
  if(verbose)printf("  [%s/%s] \"%s\" %s box %s ink %s extent %ldx%ld\n",dtui::art().ok?"art":"drawn",name,l.text.c_str(),fontName(l.font),show(l.box).c_str(),show(l.ink).c_str(),l.extent.cx,l.extent.cy);
  if(button<0&&!within(l.box,area))report(l,"box crosses the frame",show(area));
  // Glyphs keep TextPad px clear of the inset; their shadow (right and below) may reach it.
  RECT clear{area.left+dtui::TextPad,area.top+dtui::TextPad,area.right,area.bottom};
  if(!empty(l.ink)&&!within(l.ink,clear))report(l,"text touches the frame",show(clear));
  if(!empty(l.ink)&&!within(l.ink,RECT{l.box.left-1,l.box.top,l.box.right+1,l.box.bottom+1}))report(l,"glyphs cut by the box",""); /* one antialiased column may pass the left edge (Arial "W") */
  if(l.message){if(l.drawn>l.box.bottom-l.box.top)report(l,"notice shows a partial line",std::to_string(l.drawn)+" px drawn");}
  else if(!l.wrap){if(l.extent.cx>l.box.right-l.box.left)report(l,"truncated",std::to_string(l.extent.cx)+" px wide");}
  else if(l.extent.cy>l.box.bottom-l.box.top||l.extent.cx>l.box.right-l.box.left)report(l,"wrapped text does not fit",std::to_string(l.extent.cx)+"x"+std::to_string(l.extent.cy));
  for(size_t k=0;k<parts.size();k++)if((int)k!=button&&overlaps(l.ink,parts[k].first))report(l,"text overlaps a part",show(parts[k].first)+" #"+std::to_string(parts[k].second));
  for(size_t j=i+1;j<labels.size();j++)if(overlaps(l.ink,labels[j].ink))report(l,"text overlaps text",'"'+labels[j].text+"\" "+show(labels[j].ink));
 }
 // Every part inside the content box or the mana section stays off its gold frame (native button slots reach the dark inset).
 for(auto&p:parts){const RECT gold=inset(p.first.top>=dtui::ManaY?mana:content,-2);if(p.first.top>=dtui::InnerY0-4&&!within(p.first,gold)){bool other=!strictRecruitment&&recruitment(v,build,p.first);if(other&&!noted.insert("part"+show(p.first)).second){notes++;continue;}
  printf("%s [%s/%s] part #%d %s crosses the frame %s\n",other?"NOTE (recruitment block)":"FAIL",dtui::art().ok?"art":"drawn",name,p.second,show(p.first).c_str(),show(gold).c_str());if(other)notes++;else failures++;}}
}
static bool drew(const char*t){for(auto&l:labels)if(l.text==t)return true;return false;}
// A disabled (limited) tile shows the same greyed picture as an unaffordable
// one; an affordable tile differs from it.
static void limitedPicture(){
 auto snap=[&](const dtui::Data&d){for(int i=0;i<W*H;i++)canvas[i]=0;dtui::render(canvasDc,0,d,true);GdiFlush();return std::vector<uint32_t>(canvas,canvas+W*H);};
 dtui::Data rich;rich.mana=100000;dtui::Data poor;dtui::Data limited=rich;limited.buildLimit[1]=dtui::TileBuilt;limited.buildLimit[2]=dtui::TileBuilding;
 auto a=snap(rich),p=snap(poor),l=snap(limited);
 for(int c=0;c<4;c++){int x0=dtui::CellX[c%2]+(dtui::CellW-dtui::IconW)/2,y0=dtui::CellY[c/2]+2,same=0,differ=0;
  for(int y=y0;y<y0+dtui::IconH;y++)for(int x=x0;x<x0+dtui::IconW;x++){same+=l[y*W+x]==p[y*W+x];differ+=l[y*W+x]!=a[y*W+x];}
  if(c==1||c==2)assert(same==dtui::IconW*dtui::IconH&&differ>300);else assert(!differ);}
}
static void layoutAll(){
 dtui::Data b;b.ready=true;b.recruitmentReady=true;
 // The longest notice: 159 characters of real words (v.message holds 160 bytes).
 std::string s;while(s.size()<159)s+="Only Warriors, Throwers, Gardeners and Shamans can board a landed Manacopter. ";s.resize(159);strcpy(b.message,s.c_str());
 b.mana=0;layout("construction, no mana",b,true);b.mana=UINT32_MAX;layout("construction, max mana",b,true);b.mana=99999;layout("construction, 99999 mana",b,true);
 // One Temple and one Fortress: the state replaces the cost on their tiles.
 {dtui::Data v=b;v.buildLimit[1]=dtui::TileBuilt;v.buildLimit[2]=dtui::TileBuilding;v.mana=UINT32_MAX;layout("construction, Temple built, Fortress building",v,true);
  assert(drew("Built")&&drew("Building")&&!drew("100")&&!drew("150")&&drew("25")&&drew("120"));
  v.buildLimit[1]=dtui::TileBuilding;v.buildLimit[2]=dtui::TileBuilt;v.mana=0;layout("construction, Temple building, Fortress built, no mana",v,true);
  assert(drew("Built")&&drew("Building")&&!drew("100")&&!drew("150"));}
 b.queues[2]=99;b.queues[3]=100;b.queues[4]=99;b.queues[5]=100;
 for(int k:{49,50,51,80})for(int site=0;site<2;site++){dtui::Data v=b;v.kind=(uint16_t)k;v.site=site;v.aircraftCount=128;v.aircraftSeconds=3840;v.progress=100;char n[48];snprintf(n,sizeof(n),"%s%s",dtui::buildingName(v.kind),site?" site":"");layout(n,v,false);}
 {dtui::Data v=b;v.kind=80;v.aircraftCount=0;layout("Hall, idle",v,false);}
 // The info column fits any mana value (exact below 100000, then k/M) beside both picture positions.
 for(uint32_t m:{9999u,99999u,100000u,99999999u,UINT32_MAX})for(int k:{49,80}){dtui::Data v=b;v.kind=(uint16_t)k;v.mana=m;v.message[0]=0;layout(k==80?"Hall, mana":"Farm, mana",v,false);}
 for(int k:{50,51}){dtui::Data v=b;v.kind=(uint16_t)k;v.recruitmentReady=false;layout("recruitment unavailable",v,false);}
 // Manacopters: worst-case counts (four passenger types of 999, 99 aboard each (the tick caps both)), every state, with and without the longest notice.
 for(int notice=0;notice<2;notice++){dtui::Data v=b;if(!notice)v.message[0]=0;auto check=[&](const char*what){layout((std::string(what)+(notice?" + notice":"")).c_str(),v,false);};
  fleet(v,1,99,4,999);check("Manacopter, landed");v.landed=false;v.airLanded[0]=false;check("Manacopter, flying");v.unloadTargeting=true;check("Manacopter, unload");
  fleet(v,1,0,0,0);v.unloadTargeting=false;check("Manacopter, nobody aboard");fleet(v,1,6,3,999);check("Manacopter, three types");fleet(v,3,6,4,999);check("Manacopters (3)");
  fleet(v,12,99,4,999);check("Manacopters (12)");v.unloadTargeting=true;check("Manacopters (12), unload");v.unloadTargeting=false;fleet(v,65535,99,4,999);check("Manacopters (65535)");}
 {dtui::Data v=b;v.unitPanel=true;v.unitTypes=6;v.unitTotal=5994;v.unitMilitary=true;const uint16_t k[6]={29,32,52,54,53,55};
  for(int i=0;i<6;i++){v.unitKind[i]=k[i];v.unitCount[i]=999;v.unitHealth[i]=(uint8_t)(100-i*19);}
  layout("units, 6 types",v,false);v.unitTypes=8;layout("units, 8 types",v,false);v.unitTypes=255;v.unitTotal=65535;layout("units, 255 types",v,false);v.unitMilitary=false;v.unitTypes=1;v.unitTotal=1;v.unitCount[0]=1;layout("units, one",v,false);}
}
int main(int argc,char**){
 verbose=argc>1;setvbuf(stdout,nullptr,_IONBF,0);
 using dtui::hit;dtui::Data v;v.mana=150;
 // Construction page: 2x2 large building tiles, affordable only; no sub-tabs (the title box is inert).
 {dtui::Data b;const int types[]={49,50,51,80};const uint32_t costs[]={25,100,150,120};
  for(int c=0;c<4;c++){int x=dtui::CellX[c%2]+dtui::IconW/2,y=dtui::CellY[c/2]+dtui::IconH/2;
   b.mana=costs[c]-1;assert(hit(b,true,x,y).type==0);b.mana=costs[c];auto a=hit(b,true,x,y);assert(a.type==1&&a.arg==types[c]);
   assert(hit(b,true,x,dtui::CellY[c/2]+dtui::CellH-1).arg==types[c]);} // names below the icon belong to the cell
  b.mana=1000;assert(hit(b,true,20,dtui::CellY[0]+5).arg==49&&hit(b,true,190,dtui::CellY[1]+5).arg==80);
  assert(hit(b,true,60,dtui::CellY[0]-2).type==0&&hit(b,true,60,dtui::CellY[1]+dtui::CellH).type==0&&hit(b,true,60,dtui::CellY[0]+dtui::CellH+3).type==0);
  for(int x=0;x<dtui::BoxW;x+=3)for(int y=dtui::BoxY;y<dtui::CellY[0];y+=3)assert(hit(b,true,x,y).type==0); /* the former sub-tabs (x 9..80, y 214..247) and the title box */
  assert(hit(b,true,60,dtui::BoxY-1).type==0&&hit(b,true,dtui::BoxW,300).type==0);}
 // Mana section: the native resource panel area (menu 1), in every mode; no commands (the callers swallow its clicks).
 {assert(dtui::manaArea(0,dtui::ManaY)&&dtui::manaArea(dtui::PanelW-1,dtui::ManaY+dtui::ManaH-1)&&dtui::manaArea(100,700));
  assert(!dtui::manaArea(100,dtui::ManaY-1)&&!dtui::manaArea(100,dtui::ManaY+dtui::ManaH)&&!dtui::manaArea(dtui::PanelW,700)&&!dtui::manaArea(-1,700));
  dtui::Data m;m.mana=1000;m.recruitmentReady=true;
  for(int x=0;x<dtui::PanelW;x+=7)for(int y=dtui::ManaY;y<dtui::ManaY+dtui::ManaH;y+=7){assert(hit(m,true,x,y).type==0);for(int k:{49,50,51,80}){m.kind=(uint16_t)k;assert(hit(m,false,x,y).type==0);}
   m.kind=0;m.isAircraft=true;assert(hit(m,false,x,y).type==0);m.isAircraft=false;m.unitPanel=true;assert(hit(m,false,x,y).type==0);m.unitPanel=false;}}
 // One Temple and one Fortress: while one is built or being built its tile is
 // disabled like an unaffordable one (the whole tile is inert); the others stay available.
 {dtui::Data b;b.mana=UINT32_MAX;
  for(uint8_t s:{dtui::TileBuilt,dtui::TileBuilding})for(int c:{1,2}){dtui::Data l=b;l.buildLimit[c]=s;
   for(int k=0;k<4;k++){auto a=hit(l,true,dtui::CellX[k%2]+dtui::IconW/2,dtui::CellY[k/2]+dtui::IconH/2);assert(k==c?a.type==0:a.type==1&&a.arg==dtui::BuildTypes[k]);}
   for(int y=dtui::CellY[c/2];y<dtui::CellY[c/2]+dtui::CellH;y+=3)for(int x=dtui::CellX[c%2];x<dtui::CellX[c%2]+dtui::CellW;x+=3)assert(hit(l,true,x,y).type==0);
   assert(!dtui::buildable(l,c)&&dtui::buildable(l,3-c));}
  dtui::Data l=b;l.buildLimit[1]=dtui::TileBuilt;l.buildLimit[2]=dtui::TileBuilt;l.mana=24;for(int k=0;k<4;k++)assert(hit(l,true,dtui::CellX[k%2]+5,dtui::CellY[k/2]+5).type==0);
  l.mana=25;assert(hit(l,true,dtui::CellX[0]+5,dtui::CellY[0]+5).arg==49&&hit(l,true,dtui::CellX[1]+5,dtui::CellY[0]+5).type==0);}
 // Temple/Fortress recruitment (native barracks controls in the building panel):
 // unit cells select a unit (UI only); the native cluster orders for the selected unit.
 assert(dtui::ClusterAmount[0][0]==100&&dtui::ClusterAmount[0][1]==5&&dtui::ClusterAmount[0][2]==1&&dtui::ClusterAmount[1][0]==0&&dtui::ClusterAmount[1][1]==-5&&dtui::ClusterAmount[1][2]==-1);
 {// Geometry: inside the content area below the separator, nothing overlapping, native 26x28 controls 1 px apart.
  auto overlap=[](int ax,int ay,int aw,int ah,int bx,int by,int bw,int bh){return ax<bx+bw&&bx<ax+aw&&ay<by+bh&&by<ay+ah;};
  int boxes[10][4];for(int r=0;r<4;r++){int*b=boxes[r];b[0]=dtui::RecruitX[r%2];b[1]=dtui::RecruitY[r/2];b[2]=dtui::RecruitW;b[3]=dtui::RecruitH;}
  for(int c=0;c<6;c++){int*b=boxes[4+c];b[0]=dtui::ClusterX[c%3];b[1]=dtui::ClusterY[c/3];b[2]=dtui::ClusterW;b[3]=dtui::ClusterH;}
  for(int i=0;i<10;i++){assert(boxes[i][0]>=12&&boxes[i][0]+boxes[i][2]<=198&&boxes[i][1]>=dtui::SeparatorY+5&&boxes[i][1]+boxes[i][3]<=546);for(int j=0;j<i;j++)assert(!overlap(boxes[i][0],boxes[i][1],boxes[i][2],boxes[i][3],boxes[j][0],boxes[j][1],boxes[j][2],boxes[j][3]));}
  assert(dtui::ClusterY[1]-dtui::ClusterY[0]==dtui::ClusterH+1&&dtui::ClusterX[1]-dtui::ClusterX[0]==53&&dtui::ClusterX[2]-dtui::ClusterX[1]==54&&dtui::ClusterW==26&&dtui::ClusterH==28);}
 for(int k:{50,51}){dtui::Data t;t.kind=(uint16_t)k;t.recruitmentReady=true;t.mana=150;
  for(int r=0;r<4;r++){int x=dtui::RecruitX[r%2],y=dtui::RecruitY[r/2];const int px[3]={x,x+60,x+dtui::RecruitW-1},py[3]={y,y+30,y+dtui::RecruitH-1};
   for(int i=0;i<3;i++){auto a=hit(t,false,px[i],py[i]);assert(a.type==dtui::RecruitSelect&&a.arg==r);}}
  for(int pick=0;pick<4;pick++){t.recruitPick=(uint8_t)pick;for(int i:{2,3,4,5})t.queues[i]=10;
   for(int c=0;c<6;c++){auto a=hit(t,false,dtui::ClusterX[c%3]+5,dtui::ClusterY[c/3]+5);assert(a.type==dtui::RecruitOrder&&a.arg==dtui::RecruitUnits[pick]&&a.amount==dtui::ClusterAmount[c/3][c%3]);
    assert(hit(t,false,dtui::ClusterX[c%3]+dtui::ClusterW-1,dtui::ClusterY[c/3]+dtui::ClusterH-1).amount==a.amount);}}
  // Disabled controls do nothing: empty queue (clear, -5, -1), full queue (+5, +1), infinite queue (infinite, +5, +1).
  auto orders=[&](uint32_t n){t.recruitPick=1;t.queues[5]=n;int mask=0;for(int c=0;c<6;c++){auto a=hit(t,false,dtui::ClusterX[c%3]+5,dtui::ClusterY[c/3]+5);if(a.type){assert(a.type==dtui::RecruitOrder&&a.arg==32);mask|=1<<c;}}return mask;};
  assert(orders(0)==0x07&&orders(1)==0x3F&&orders(98)==0x3F&&orders(99)==0x39&&orders(100)==0x38&&orders(5)==0x3F);
  t.recruitPick=7;assert(hit(t,false,dtui::ClusterX[2]+5,dtui::ClusterY[0]+5).arg==29); // out of range: Warrior
  // Gaps and the area below the cluster are inert.
  assert(hit(t,false,dtui::ClusterX[0]+dtui::ClusterW+2,dtui::ClusterY[0]+5).type==0&&hit(t,false,dtui::ClusterX[0]-3,dtui::ClusterY[1]+5).type==0&&hit(t,false,100,560).type==0&&hit(t,false,100,dtui::SeparatorY+2).type==0);
  // Clear all manual orders: only when some exist (the automatic farm-worker queue does not count); demolish.
  for(int i=0;i<6;i++)t.queues[i]=0;t.queues[1]=5;assert(hit(t,false,dtui::SecondX+5,dtui::SecondY+5).type==0);
  t.queues[3]=1;assert(hit(t,false,dtui::SecondX+5,dtui::SecondY+5).type==8&&hit(t,false,dtui::RightX+5,dtui::RightY+5).type==5);
  t.recruitmentReady=false;assert(hit(t,false,dtui::RecruitX[0]+5,dtui::RecruitY[0]+5).type==0&&hit(t,false,dtui::ClusterX[2]+5,dtui::ClusterY[0]+5).type==0&&hit(t,false,dtui::SecondX+5,dtui::SecondY+5).type==0&&hit(t,false,dtui::RightX+5,dtui::RightY+5).type==5);
  t.recruitmentReady=true;t.site=true;assert(hit(t,false,dtui::RecruitX[0]+5,dtui::RecruitY[0]+5).type==0&&hit(t,false,dtui::ClusterX[2]+5,dtui::ClusterY[0]+5).type==0&&hit(t,false,dtui::RightX+5,dtui::RightY+5).type==0);
  t.site=false;t.kind=49;assert(hit(t,false,dtui::ClusterX[2]+5,dtui::ClusterY[0]+5).type==0&&hit(t,false,dtui::RecruitX[0]+5,dtui::RecruitY[0]+5).type==0);}
 // Hall: queue, cancel last, cancel all, select ready aircraft; demolish.
 // Hall: rows like the Manacopter panel; queue needs 300 mana, cancel needs a queue; no top-right buttons.
 {dtui::Data h;h.kind=80;h.mana=300;h.aircraftCount=2;auto row=[](int r){return dtui::AirActionY+r*dtui::AirActionStep+10;};
  for(int r=0;r<dtui::HallActions;r++)assert(hit(h,false,40,row(r)).type==dtui::HallRows[r]);
  assert(hit(h,false,40,row(dtui::HallActions)).type==0&&hit(h,false,dtui::RightX+5,dtui::RightY+5).type==0);
  h.mana=299;assert(hit(h,false,40,row(0)).type==0);h.aircraftCount=0;assert(hit(h,false,40,row(1)).type==0&&hit(h,false,40,row(2)).type==0&&hit(h,false,40,row(3)).type==11&&hit(h,false,40,row(4)).type==5);
  h.site=true;assert(hit(h,false,40,row(4)).type==0);}
 // Farm: demolish only.
 {v.kind=49;assert(hit(v,false,dtui::RightX+5,dtui::RightY+5).type==5&&hit(v,false,40,400).type==0&&hit(v,false,40,dtui::AirActionY+5).type==0);}
 // Manacopter panel: the unit panel's grid, summary and action rows (same metrics).
 static_assert(dtui::AirX0==dtui::UnitX0&&dtui::AirDX==dtui::UnitDX&&dtui::AirRowY[0]==dtui::UnitRowY[0]&&dtui::AirRowY[1]==dtui::UnitRowY[1]&&dtui::AirCellH==dtui::UnitCellH,"aircraft grid = unit grid");
 static_assert(dtui::AirSummaryY==dtui::UnitSummaryY&&dtui::AirActionY==dtui::UnitActionY&&dtui::AirActionStep==dtui::UnitActionStep&&dtui::AirActionY+dtui::AirActions*dtui::AirActionStep<=dtui::InnerY1,"aircraft rows = unit rows, inside the frame");
 static_assert(dtui::AircraftKeep!=dtui::RecruitSelect&&dtui::AircraftKeep!=dtui::RecruitOrder&&(dtui::AircraftKeep<40||dtui::AircraftKeep>=50)&&dtui::AircraftKeep>36,"free command number");
 // Rows: unload (only with passengers aboard), load nearby, return to base, next Manacopter, deselect.
 {dtui::Data a;a.isAircraft=true;a.landed=true;a.group=1;a.airShown=1;auto row=[](int r){return dtui::AirActionY+r*dtui::AirActionStep+10;};
  assert(hit(a,false,40,row(0)).type==0);a.passengers=2;assert(hit(a,false,40,row(0)).type==16);a.passengers=0;a.groupPassengers=3;assert(hit(a,false,40,row(0)).type==16);
  a.groupPassengers=0;a.group=3;a.groupUnload=1;assert(hit(a,false,40,row(0)).type==16);a.groupUnload=0;a.group=1;
  const int rows[5]={16,36,12,11,13};for(int r=1;r<5;r++)assert(hit(a,false,40,row(r)).type==rows[r]&&hit(a,false,dtui::ActionX+dtui::ActionW-1,row(r)-10).type==rows[r]&&hit(a,false,dtui::ActionX,row(r)+18).type==rows[r]);
  assert(hit(a,false,40,row(5)).type==0&&hit(a,false,dtui::ActionX-1,row(1)).type==0&&hit(a,false,40,dtui::AirSummaryY+5).type==0&&hit(a,false,60,dtui::AirRuleY[1]+2).type==0);
  // One aircraft: its cell does nothing (like a single unit type); the old top-right buttons are gone.
  {int x,y;dtui::airCell(0,x,y);assert(hit(a,false,x+10,y+20).type==0&&hit(a,false,dtui::RightX+5,dtui::RightY+5).type==0&&hit(a,false,dtui::SecondX+5,dtui::SecondY+5).type==0);}
  // A group: each shown cell (picture, bar, count) keeps that aircraft; unused cells and gaps are inert.
  a.group=12;for(int n:{2,3,4,6}){a.airShown=(uint8_t)n;for(int i=0;i<6;i++){int x,y;dtui::airCell(i,x,y);const int px[3]={x+dtui::AirPictureDX,x+44,x+dtui::AirPictureDX+dtui::AirCellW-1},py[3]={y,y+30,y+dtui::AirCellH-1};
   for(int k=0;k<3;k++){auto h=hit(a,false,px[k],py[k]);assert(i<n?h.type==dtui::AircraftKeep&&h.arg==i:h.type==0);}
   assert(hit(a,false,x+dtui::AirPictureDX-2,y+20).type==0&&hit(a,false,x+20,y+dtui::AirCellH+1).type==0);}}
  // A notice replaces the second row: its cells are inert, the first row still keeps.
  strcpy(a.message,"Some Manacopters could not fly there.");{int x,y;dtui::airCell(4,x,y);assert(hit(a,false,x+10,y+20).type==0);dtui::airCell(2,x,y);auto h=hit(a,false,x+10,y+20);assert(h.type==dtui::AircraftKeep&&h.arg==2);}a.message[0]=0;
  // Unload targeting: the unload row stays active (clicking again re-arms); the construction page has priority.
  a.unloadTargeting=true;a.passengers=1;assert(hit(a,false,40,row(0)).type==16&&hit(a,false,40,row(3)).type==11);
  {int x,y;dtui::airCell(0,x,y);assert(hit(a,true,x+10,y+20).type!=dtui::AircraftKeep&&hit(a,true,40,row(4)).type!=13);}}
 puts("PASS: native-layout hit targets");
 BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=W;bi.bmiHeader.biHeight=-H;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
 canvasDc=CreateCompatibleDC(nullptr);HBITMAP bm=CreateDIBSection(canvasDc,&bi,DIB_RGB_COLORS,(void**)&canvas,nullptr,0);assert(bm);SelectObject(canvasDc,bm);
 // Recruitment text fits its boxes without ellipsis: unit names, the cluster caption, counts and costs.
 {auto fits=[&](const char*t,dtui::Font f,int w){return dtui::textWidth(canvasDc,t,f)<=w;};
  for(int r=0;r<4;r++){assert(fits(dtui::RecruitNames[r],dtui::F11,dtui::RecruitTextW[r%2]-1));char c[40];snprintf(c,sizeof(c),"%ss to train",dtui::RecruitNames[r]);assert(fits(c,dtui::F14,180));}
  assert(fits("99",dtui::F1,dtui::RecruitTextW[1]-1)&&fits("15",dtui::F9,24));}
 puts("PASS: recruitment labels fit");
 BITMAPINFO mi=bi;mi.bmiHeader.biWidth=MW;mi.bmiHeader.biHeight=-MH;maskDc=CreateCompatibleDC(nullptr);HBITMAP mb=CreateDIBSection(maskDc,&mi,DIB_RGB_COLORS,(void**)&mask,nullptr,0);assert(mb);SelectObject(maskDc,mb);
 dtui::art().unload();renderAll();noSubTabs();limitedPicture();puts("PASS: drawn fallback covers the panel and the mana section in every mode");
 layoutAll();
 if(!gameFolder().empty()&&dtui::art().load(gameFolder()+L"Gfx\\")){
  renderAll();noSubTabs();limitedPicture();
  // The native panel sprites are drawn 1:1 at their native positions: #589 on every page (construction too) and #587 under it.
  for(int build=0;build<2;build++){dtui::Data f;f.kind=49;f.ready=true;for(int i=0;i<W*H;i++)canvas[i]=0;dtui::render(canvasDc,0,f,build);dtui::manaSection(canvasDc,0,f);GdiFlush();
   const auto&p=dtui::art()[dtgfx::Panel];const auto&m=dtui::art()[dtgfx::ResourcePanel];assert(m.w==dtui::PanelW&&m.h==dtui::ManaH);
   for(int y=0;y<30;y++)for(int x=0;x<20;x++){assert((canvas[(dtui::PanelY+y)*W+x]&0xffffff)==(p.at(x,y)&0xffffff));assert((canvas[(dtui::ManaY+y)*W+x]&0xffffff)==(m.at(x,y)&0xffffff));}}
  // The native bracket marks only the selected recruitment cell and follows recruitPick;
  // the infinite control shows its pressed sprite while the selected unit recruits continuously.
  {dtui::Data t;t.kind=50;t.recruitmentReady=true;t.mana=100;
   auto snap=[&](uint8_t pick){t.recruitPick=pick;for(int i=0;i<W*H;i++)canvas[i]=0;dtui::render(canvasDc,0,t,false);GdiFlush();return std::vector<uint32_t>(canvas,canvas+W*H);};
   auto px=[&](const std::vector<uint32_t>&c,int x,int y){return c[y*W+x]&0xffffff;};
   const auto&tl=dtui::art()[dtgfx::BracketTL];assert(tl.at(0,0)>>24==255);
   for(int pick=0;pick<4;pick++){auto c=snap((uint8_t)pick);
    for(int r=0;r<4;r++){int x=dtui::RecruitX[r%2]+2,y=dtui::RecruitY[r/2]+2;assert((px(c,x,y)==(tl.at(0,0)&0xffffff))==(r==pick));}}
   t.recruitPick=1;t.queues[5]=10;auto finite=snap(1);t.queues[5]=100;auto endless=snap(1);int changed=0;
   for(int y=dtui::ClusterY[0];y<dtui::ClusterY[0]+dtui::ClusterH;y++)for(int x=dtui::ClusterX[0];x<dtui::ClusterX[0]+dtui::ClusterW;x++)changed+=px(finite,x,y)!=px(endless,x,y);assert(changed>20);}
  puts("PASS: native sprites cover the panel and the mana section in every mode (#589/#587 1:1, no construction sub-tabs); recruitment selection bracket and infinite state");
  layoutAll();
 }else printf("SKIP: native sprites (%s)\n",dtui::art().why);
 printf("layout: %d failures, %d notes (recruitment block)\n",failures,notes);assert(!failures);puts("PASS: every text inside its frame, whole, and clear of parts and other texts (fallback and native art, worst-case data)");
 // Health bars: colour thresholds and fill extent.
 {for(int i=0;i<W*H;i++)canvas[i]=0;dtui::healthBar(canvasDc,10,10,52,6,50);dtui::healthBar(canvasDc,10,30,6,42,100);GdiFlush();
  auto px=[&](int x,int y){uint32_t c=canvas[y*W+x];return RGB((c>>16)&255,(c>>8)&255,c&255);};
  assert(px(30,13)==dtui::healthColor(50)&&px(55,13)!=dtui::healthColor(50));assert(px(13,40)==dtui::healthColor(100));}
 DeleteDC(maskDc);DeleteObject(mb);DeleteDC(canvasDc);DeleteObject(bm);puts("PASS: panel hit targets, opaque panel in every mode (fallback and native art), health bars");return 0;}

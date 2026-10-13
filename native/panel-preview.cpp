#include "panel.hpp"
#include <string>
// Offline preview of every Dark panel mode (illustrative data, not a game
// screenshot). Each panel sits on a stand-in "native menu" stripe pattern, so
// any pixel the panel fails to cover would show as red/blue stripes: the
// content panel (y 210..599) and, below it, the native resource panel
// (y 600..767), which the mana section covers in every mode.
// Usage: panel-preview output.bmp [fortress|worst] [gfx-folder]
// "worst" shows the worst-case data of the layout test (longest values and
// notices). Without a gfx folder the drawn fallback is shown (safe to
// publish); with the game's Gfx folder the native sprites are used (keep that
// output private).
constexpr int Top=160,Bottom=dtui::ManaY+dtui::ManaH,Cell=Bottom-Top+40; // shown rows of the GUI column, plus the caption
static void backdrop(HDC dc,bool build){
 dtui::box(dc,0,168,210,Bottom+4-168,RGB(58,44,22));dtui::box(dc,2,170,206,Bottom-170,RGB(92,72,38));
 for(int y=dtui::BoxY;y<Bottom;y+=6)dtui::box(dc,0,y,dtui::BoxW,3,(y/6)%2?RGB(170,40,40):RGB(40,60,170));
 dtui::box(dc,9,176,43,34,RGB(120,40,40));dtui::box(dc,55,175,48,35,RGB(40,60,140)); // native tabs (stand-ins)
 if(build)dtui::houseTab(dc,0);
}
// page=false: a native menu shows above (no Dark page), only the mana section is drawn.
static void place(HDC dc,int column,int row,const char*caption,const dtui::Data&v,bool build,bool page=true){
 POINT old;SetViewportOrgEx(dc,20+column*222,60+row*Cell-Top,&old);backdrop(dc,build&&page);if(page)dtui::render(dc,0,v,build);dtui::manaSection(dc,0,v);
 SetBkMode(dc,TRANSPARENT);dtui::label(dc,4,Bottom+8,206,16,caption,dtui::F11,RGB(214,206,180));SetViewportOrgEx(dc,old.x,old.y,nullptr);
}
// Aircraft panel sample: n picked aircraft (the first six shown) with their
// health/passengers/landed state and the pick's passengers by type.
static void fleet(dtui::Data&v,int n,const int*hp,const int*load,const bool*landed,int types,const uint16_t*kind,const uint16_t*count){
 v.isAircraft=true;v.kind=0;v.group=(uint32_t)n;v.airShown=(uint8_t)std::min(n,6);uint32_t sum=0,total=0;
 for(int i=0;i<n;i++){int k=i%6;sum+=hp[k];total+=load[k];if(i<6){v.airHp[i]=(uint8_t)hp[k];v.airLoad[i]=(uint8_t)load[k];v.airLanded[i]=landed[k];v.airId[i]=(uint16_t)(40+i);v.airIdentity[i]=1000u+i;}}
 v.groupHp=(uint8_t)((sum+n/2)/n);v.groupPassengers=total;v.passengers=(uint32_t)load[0];v.aircraftHp=(uint8_t)hp[0];v.landed=landed[0];v.groupUnload=landed[0]&&total;
 v.riderTypes=(uint8_t)types;for(int i=0;i<types&&i<4;i++){v.riderKind[i]=kind[i];v.riderCount[i]=count[i];}
}
static void worst(HDC dc){
 dtui::Data b;b.ready=true;b.recruitmentReady=true;b.mana=99999;b.queues[2]=99;b.queues[3]=100;b.queues[4]=99;b.queues[5]=100;
 std::string m;while(m.size()<159)m+="Only Warriors, Throwers, Gardeners and Shamans can board a landed Manacopter. ";m.resize(159);strcpy(b.message,m.c_str());
 dtui::Data v=b;v.mana=0;place(dc,0,0,"Construction (0 mana)",v,true);v.mana=99999;place(dc,1,0,"Construction (99999)",v,true);
 v=b;v.kind=49;place(dc,2,0,"Farm (99999 mana)",v,false);v.kind=51;place(dc,3,0,"Fortress (99/ALL)",v,false);
 v=b;v.kind=80;v.aircraftCount=128;v.aircraftSeconds=3840;v.progress=60;place(dc,4,0,"Hall (128 queued)",v,false);v=b;v.kind=50;v.site=true;place(dc,5,0,"Temple site",v,false);
 {const int hp[6]={100,1,50,100,66,33},load[6]={6,6,6,6,6,6};const bool landed[6]={true,false,true,false,true,false};const uint16_t kind[4]={29,32,52,54},count[4]={999,999,999,999};
  dtui::Data a=b;a.message[0]=0;fleet(a,1,hp,load,landed,4,kind,count);place(dc,0,1,"Manacopter (4 types)",a,false);
  a=b;fleet(a,12,hp,load,landed,4,kind,count);a.unloadTargeting=true;place(dc,1,1,"12 Manacopters, unload",a,false);}
 dtui::Data u=b;u.unitPanel=true;u.unitTypes=6;u.unitTotal=5994;u.unitMilitary=true;const uint16_t k[6]={29,32,52,54,53,55};
 for(int i=0;i<6;i++){u.unitKind[i]=k[i];u.unitCount[i]=999;u.unitHealth[i]=(uint8_t)(100-i*19);}
 place(dc,2,1,"Units (6 x 999)",u,false);u.unitTypes=8;place(dc,3,1,"Units (8 types)",u,false);u.unitTypes=255;u.unitTotal=65535;place(dc,4,1,"Units (255 types)",u,false);
 v=b;v.kind=80;v.mana=4000000000u;v.aircraftCount=0;place(dc,5,1,"Hall (4G mana, idle)",v,false);
}
int main(int argc,char**argv){
 if(argc<2||argc>4)return 2;const char*mode=argc>=3&&(!strcmp(argv[2],"fortress")||!strcmp(argv[2],"worst"))?argv[2]:"";bool fortress=!strcmp(mode,"fortress");
 if(argc>=3&&strcmp(argv[argc-1],mode)){wchar_t dir[MAX_PATH];MultiByteToWideChar(CP_ACP,0,argv[argc-1],-1,dir,MAX_PATH);if(!dtui::art().load(dir))return 5;}const int w=fortress?20+5*222:20+6*222,h=fortress?60+Cell:60+2*Cell;
 BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=w;bi.bmiHeader.biHeight=-h;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
 void*pixels=nullptr;HDC dc=CreateCompatibleDC(nullptr);HBITMAP bm=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&pixels,nullptr,0);if(!bm)return 3;auto old=SelectObject(dc,bm);
 dtui::box(dc,0,0,w,h,RGB(22,20,16));SetBkMode(dc,TRANSPARENT);dtui::label(dc,20,14,w-40,24,"OFFLINE PANEL PREVIEW - illustrative data, not a game screenshot",dtui::F4,RGB(214,186,120));
 dtui::Data base;base.ready=true;base.recruitmentReady=true;base.mana=48;base.queues[4]=12;base.queues[5]=3;base.queues[2]=0;base.queues[3]=2;strcpy(base.message,"Orders wait for mana.");
 /* Manacopters: one flying with three aboard; a group of five (two rows); a landed group of three waiting for the unload click. */
 const int airHp[6]={72,100,45,90,20,100},airLoad[6]={3,6,0,2,6,1};const bool airLanded[6]={false,true,true,false,true,true};
 dtui::Data air=base;air.selected=42;air.flightActive=true;air.message[0]=0;{const uint16_t k[3]={29,32,54},n[3]={1,1,1};fleet(air,1,airHp,airLoad,airLanded,3,k,n);}
 dtui::Data five=base;five.message[0]=0;{const uint16_t k[4]={29,54,52,32},n[4]={9,4,3,2};fleet(five,5,airHp,airLoad,airLanded,4,k,n);}
 dtui::Data land=base;land.message[0]=0;{const int hp[3]={100,64,90},load[3]={6,1,0};const bool on[3]={true,true,true};const uint16_t k[3]={29,52,54},n[3]={4,2,1};fleet(land,3,hp,load,on,3,k,n);}land.unloadTargeting=true;
 if(!strcmp(mode,"worst"))worst(dc);
 else if(fortress){
  dtui::Data v=base;v.kind=51;v.recruitPick=3;strcpy(v.message,"Shaman orders: 2.");place(dc,0,0,"Fortress recruitment",v,false);
  v.site=true;place(dc,1,0,"Fortress foundation",v,false);v.site=false;v.recruitmentReady=false;place(dc,2,0,"Recruitment unavailable",v,false);
  v.mana=150;v.buildLimit[1]=dtui::TileBuilt;strcpy(v.message,"Select a building to place.");place(dc,3,0,"Construction (Temple built)",v,true);
  v=base;v.mana=1250;place(dc,4,0,"Native menu above (mana only)",v,false,false);
 }else{
  dtui::Data v=base;strcpy(v.message,"Select a building to place.");place(dc,0,0,"Construction (48 mana)",v,true);v.mana=420;v.buildLimit[1]=dtui::TileBuilt;v.buildLimit[2]=dtui::TileBuilding;place(dc,1,0,"Temple built, Fortress building",v,true);
  v=base;v.kind=50;strcpy(v.message,"Warrior orders: 12.");place(dc,2,0,"Temple recruitment",v,false);
  v.kind=51;v.queues[2]=100;v.recruitPick=2;strcpy(v.message,"Gardener: continuous recruitment while mana lasts.");place(dc,3,0,"Fortress (continuous)",v,false);
  v=base;v.kind=50;v.recruitmentReady=false;place(dc,4,0,"Recruitment unavailable",v,false);v=base;v.kind=50;v.site=true;strcpy(v.message,"Dark Temple placed.");place(dc,5,0,"Foundation (Temple)",v,false);
  v=base;v.kind=49;strcpy(v.message,"Native farm workers deliver mushrooms.");place(dc,0,1,"Mushroom Farm",v,false);
  v=base;v.kind=80;v.mana=340;v.aircraftCount=3;v.aircraftSeconds=17;v.progress=43;strcpy(v.message,"Aircraft queued.");place(dc,1,1,"Manacopter Hall",v,false);
  place(dc,2,1,"Manacopter (flying)",air,false);place(dc,3,1,"Manacopter group (5)",five,false);place(dc,4,1,"Unload targeting (3)",land,false);
  dtui::Data p=base;p.unitPanel=true;p.unitMilitary=true;p.unitTypes=4;p.unitTotal=9;const uint16_t uk[]={29,32,52,54},uc[]={4,2,2,1};const uint8_t uh[]={100,64,90,28};for(int i=0;i<4;i++){p.unitKind[i]=uk[i];p.unitCount[i]=uc[i];p.unitHealth[i]=uh[i];}strcpy(p.message,"");place(dc,5,1,"Dark units (mixed)",p,false);
 }
 GdiFlush();BITMAPFILEHEADER fh{};fh.bfType=0x4d42;fh.bfOffBits=sizeof(fh)+sizeof(BITMAPINFOHEADER);fh.bfSize=fh.bfOffBits+w*h*4;FILE*f=fopen(argv[1],"wb");if(!f)return 4;
 fwrite(&fh,1,sizeof(fh),f);fwrite(&bi.bmiHeader,1,sizeof(BITMAPINFOHEADER),f);fwrite(pixels,1,w*h*4,f);fclose(f);SelectObject(dc,old);DeleteObject(bm);DeleteDC(dc);return 0;
}

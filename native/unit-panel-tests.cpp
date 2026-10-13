#include "panel.hpp"
#include "aircraft-model.hpp"
#include <assert.h>
#include <set>
#include <string>
using dtunits::Member;
static std::vector<std::pair<std::string,int>> texts; // label text and box top, via dtui::labelProbe
static void collect(HDC,const dtui::LabelProbe&p){texts.push_back({p.text,(int)p.box.top});}
static dtui::Data view(const dtunits::Summary&s){dtui::Data v;v.unitPanel=true;v.unitTypes=s.types;v.unitTotal=s.total;v.unitMilitary=s.military;memcpy(v.unitKind,s.kind,sizeof(v.unitKind));memcpy(v.unitCount,s.count,sizeof(v.unitCount));memcpy(v.unitHealth,s.health,sizeof(v.unitHealth));return v;}
int main(){
 // Health percent follows the native bracket (hp/max, max 0 = no bar).
 assert(dtunits::healthPercent(50,50)==100&&dtunits::healthPercent(25,50)==50&&dtunits::healthPercent(1,200)==1&&dtunits::healthPercent(0,50)==0);
 assert(dtunits::healthPercent(10,0)==100&&dtunits::healthPercent(80,50)==100);
 // Same boardable set as the aircraft model.
 for(uint16_t k=0;k<80;k++)assert(dtunits::boardable(k)==dt::boardable(k));
 assert(dtunits::military(29)&&dtunits::military(34)&&!dtunits::military(52)&&!dtunits::military(35));
 assert(dtunits::mobile(54)&&!dtunits::mobile(53)&&dtunits::portraitKind(31)==29&&dtunits::portraitKind(33)==32);
 printf("PASS: unit helpers\n");
 // Grouping: most numerous first, ties by type, rounded average health.
 {Member m[]={{52,100},{29,100},{29,50},{54,80},{29,75},{52,51}};auto s=dtunits::summarize(m,6);
  assert(s.types==3&&s.total==6&&s.military);assert(s.kind[0]==29&&s.count[0]==3&&s.health[0]==75);
  assert(s.kind[1]==52&&s.count[1]==2&&s.health[1]==76);assert(s.kind[2]==54&&s.count[2]==1&&s.health[2]==80&&s.kind[3]==0);
  assert(dtunits::overallHealth(s.count,s.health,3)==(3*75+2*76+80+3)/6);}
 {Member m[]={{52,100},{54,100}};auto s=dtunits::summarize(m,2);assert(!s.military&&s.kind[0]==52&&s.kind[1]==54);}
 {auto s=dtunits::summarize(nullptr,0);assert(s.types==0&&s.total==0&&dtunits::overallHealth(s.count,s.health,0)==100);}
 // More than six types: the six largest are shown, the rest only counted.
 {std::vector<Member> m;uint16_t kinds[]={29,32,52,53,54,55,56,66};for(int i=0;i<8;i++)for(int j=0;j<=i;j++)m.push_back({kinds[i],100});
  auto s=dtunits::summarize(m.data(),m.size());assert(s.types==8&&s.total==36&&s.kind[0]==66&&s.count[0]==8&&s.kind[5]==52&&s.count[5]==3&&s.kind[4]==53);}
 printf("PASS: unit grouping\n");
 // Spread: distinct free tiles, nearest ring first, bounded.
 // Hit regions: portraits filter by type; action rows follow what is selected.
 auto row=[](int r){return dtui::UnitActionY+r*dtui::UnitActionStep+10;};
 {Member m[]={{29,100},{29,100},{52,100},{54,40}};auto v=view(dtunits::summarize(m,4));
  for(int i=0;i<3;i++){int x,y;dtui::unitCell(i,x,y);auto a=dtui::hit(v,false,x+15,y+20);assert(a.type==dtui::UnitKeepType&&a.arg==v.unitKind[i]);}
  {int x,y;dtui::unitCell(3,x,y);assert(dtui::hit(v,false,x+15,y+20).type==0);}
  // Only "Board Manacopter" remains below the summary.
  assert(dtui::hit(v,false,100,row(0)).type==dtui::UnitBoard);for(int r=1;r<6;r++)assert(dtui::hit(v,false,100,row(r)).type==0);
  assert(dtui::hit(v,false,100,dtui::UnitSummaryY+4).type==0&&dtui::hit(v,false,5,row(0)).type==0);
  // The build overlay and an aircraft pick keep priority over the unit panel.
  assert(dtui::hit(v,true,100,row(0)).type!=dtui::UnitBoard);v.isAircraft=true;assert(dtui::hit(v,false,100,row(0)).type!=dtui::UnitBoard);}
 {Member m[]={{29,100},{32,100}};auto v=view(dtunits::summarize(m,2));
  assert(dtui::hit(v,false,100,row(0)).type==dtui::UnitBoard);}
 {Member m[]={{53,100},{55,100}};auto v=view(dtunits::summarize(m,2));
  assert(dtui::hit(v,false,100,row(0)).type==0);} // workers that cannot board
 {Member m[]={{54,100},{54,60}};auto v=view(dtunits::summarize(m,2));int x,y;dtui::unitCell(0,x,y); // One type: no filter click.
  assert(dtui::hit(v,false,x+15,y+20).type==0&&dtui::hit(v,false,100,row(0)).type==dtui::UnitBoard);}
 printf("PASS: unit hit regions\n");
 // Rendering: no pure black (HD colour key) anywhere in the panel.
 {const int w=200,h=600;BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=w;bi.bmiHeader.biHeight=-h;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;void*bits=nullptr;
  HDC dc=CreateCompatibleDC(nullptr);HBITMAP bmp=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&bits,nullptr,0);assert(bmp&&bits);auto old=SelectObject(dc,bmp);
  Member m[]={{29,100},{29,30},{32,90},{52,100},{54,60},{53,100},{55,100},{66,10}};auto v=view(dtunits::summarize(m,8));snprintf(v.message,sizeof(v.message),"Kept 2 Warriors.");
  auto*px=(uint32_t*)bits;for(int i=0;i<w*h;i++)px[i]=0x00ffffff;dtui::render(dc,0,v,false);GdiFlush();
  unsigned black=0;for(int y=dtui::BoxY;y<dtui::BoxY+dtui::BoxH;y++)for(int x=dtui::BoxX;x<dtui::BoxX+dtui::BoxW;x++){uint32_t c=px[y*w+x]&0xffffff;if(!c)black++;}
  assert(!black);for(int y=0;y<dtui::BoxY;y++)for(int x=0;x<w;x++)assert((px[y*w+x]&0xffffff)==0xffffff);
  // Summary row: realistic selections show every part; only extreme ones drop the word "Health",
  // then the hidden-types note (never an ellipsis: panel-tests.cpp checks the fit).
  auto summary=[&](uint16_t total,uint8_t types){v.unitTotal=total;v.unitTypes=types;texts.clear();dtui::labelProbe=collect;dtui::render(dc,0,v,false);dtui::labelProbe=nullptr;
   std::string row;for(auto&t:texts)if(t.second==dtui::UnitSummaryY)row+=(row.empty()?"":"|")+t.first;return row;};
  assert(summary(9999,15)=="9999 units, +9 types|Health|"+std::to_string(dtunits::overallHealth(v.unitCount,v.unitHealth,6))+"%");
  assert(summary(1,1).rfind("1 unit|Health|",0)==0&&summary(65535,255).rfind("65535 units, +249 types|",0)==0&&summary(65535,255).find("Health")==std::string::npos);
  SelectObject(dc,old);DeleteObject(bmp);DeleteDC(dc);}
 printf("PASS: unit panel render, summary row\n");
 return 0;
}

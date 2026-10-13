#include "hd-overlay.hpp"
#include <stdio.h>
// Offline HD preview: paints the owned-window canvas for several states, scales
// it 1.25x (as at 120 DPI), crops to the tab row, the content box and the mana
// section (down to y 768), and composes it with the colour key RGB(0,0,0) over a
// stand-in native GUI (red/blue stripes where the native content panel and the
// resource panel are), exactly as ULW_COLORKEY would. Any stripe visible inside
// the content box or the mana section would be a leak.
int main(int argc,char**argv){
 if(argc<2||argc>3)return 2;if(argc==3){wchar_t dir[MAX_PATH];MultiByteToWideChar(CP_ACP,0,argv[2],-1,dir,MAX_PATH);if(!dtui::art().load(dir))return 5;} /* optional: game Gfx folder (private output) */
 const int panels=7,top=160,bottom=dthd::CanvasHeight,cw=250,ch=(bottom-top)*5/4,w=20+panels*(cw+10),h=60+ch+30;
 BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=-h;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
 void*pixels=nullptr;HDC dc=CreateCompatibleDC(nullptr);HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);if(!bitmap)return 3;auto old=SelectObject(dc,bitmap);dtui::box(dc,0,0,w,h,RGB(22,20,16));
 HDC source=CreateCompatibleDC(nullptr);info.bmiHeader.biWidth=dthd::CanvasWidth;info.bmiHeader.biHeight=-dthd::CanvasHeight;void*sourcePixels=nullptr;HBITMAP small=CreateDIBSection(source,&info,DIB_RGB_COLORS,&sourcePixels,nullptr,0);auto oldSmall=SelectObject(source,small);
 uint32_t*out=(uint32_t*)pixels;const uint32_t*in=(const uint32_t*)sourcePixels;
 dthd::Snapshot view;view.active=true;view.build=true;view.data.ready=true;view.data.mana=48;strcpy(view.data.message,"Native farm workers deliver mushrooms.");
 const char*captions[panels]={"Construction","Fortress recruitment","Manacopter Hall","Manacopter (landed)","Group of 4, unload, notice","Dark units (default body)","Native menu (mana only)"};
 for(int i=0;i<panels;i++){
  if(i==1){view.build=false;view.data.kind=51;view.data.recruitmentReady=true;view.data.queues[4]=12;view.data.queues[3]=2;}
  if(i==2){view.data.kind=80;view.data.aircraftCount=2;view.data.aircraftSeconds=9;view.data.progress=70;}
  if(i==3){auto&d=view.data;d.kind=0;d.isAircraft=true;d.selected=42;d.landed=true;d.passengers=2;d.aircraftHp=55;d.message[0]=0;
   d.group=1;d.airShown=1;d.airHp[0]=55;d.airLoad[0]=2;d.airLanded[0]=true;d.groupHp=55;d.groupPassengers=2;d.riderTypes=2;d.riderKind[0]=29;d.riderKind[1]=54;d.riderCount[0]=1;d.riderCount[1]=1;}
  if(i==4){auto&d=view.data;d.group=4;d.airShown=4;const int hp[3]={100,80,30},load[3]={6,3,0};for(int k=1;k<4;k++){d.airHp[k]=(uint8_t)hp[k-1];d.airLoad[k]=(uint8_t)load[k-1];d.airLanded[k]=k!=2;}
   d.groupHp=66;d.groupPassengers=11;d.groupUnload=1;d.unloadTargeting=true;strcpy(d.message,"Some Manacopters could not fly there.");}
  if(i==5){view.data.isAircraft=false;view.data.unitPanel=true;view.data.unitTypes=2;view.data.unitTotal=5;view.data.unitKind[0]=52;view.data.unitCount[0]=3;view.data.unitHealth[0]=100;view.data.unitKind[1]=54;view.data.unitCount[1]=2;view.data.unitHealth[1]=45;view.data.message[0]=0;}
  if(i==6){view.data.unitPanel=false;view.data.mana=1250;} /* nothing Dark selected, a native tab shows */
  dthd::paint(source,view);GdiFlush();
  int ox=20+i*(cw+10),oy=60;
  for(int y=0;y<ch;y++)for(int x=0;x<cw;x++){
   int sx=x*dthd::CanvasWidth/cw,sy=top+y*(bottom-top)/ch;uint32_t c=in[sy*dthd::CanvasWidth+sx]&0xffffff;
   // Stand-in native GUI beneath the transparent (colour-keyed) pixels.
   uint32_t under=sx<208&&sy>=170?0x5C4826u:0x16140Fu;if(sy>=dtui::BoxY&&sx<dtui::BoxW)under=(sy/6)%2?0xAA2828u:0x283CAAu;
   out[(oy+y)*w+ox+x]=c?c:under;
  }
  SetBkMode(dc,TRANSPARENT);dtui::label(dc,ox+4,oy+ch+6,cw,16,captions[i],dtui::F11,RGB(240,232,200));
 }
 SetBkMode(dc,TRANSPARENT);dtui::label(dc,20,14,w-40,24,"OFFLINE HD PANEL PREVIEW (colour key composed, 120 DPI) - not a game screenshot",dtui::F4,RGB(214,186,120));GdiFlush();
 info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=-h;BITMAPFILEHEADER header{};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER);header.bfSize=header.bfOffBits+w*h*4;FILE*f=fopen(argv[1],"wb");if(!f)return 4;fwrite(&header,1,sizeof(header),f);fwrite(&info.bmiHeader,1,sizeof(info.bmiHeader),f);fwrite(pixels,1,w*h*4,f);fclose(f);
 SelectObject(source,oldSmall);DeleteObject(small);DeleteDC(source);SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);return 0;
}

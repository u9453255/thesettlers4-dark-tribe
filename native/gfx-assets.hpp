#pragma once
// Native GUI art for the Dark menus, read from the installed game's Gfx files.
// Only the byte ranges pinned by tools/gfx_pins.py are read; every range and
// palette file is SHA-256 checked before decoding. Nothing is cached on disk
// or embedded in the DLL. If anything differs, ok stays false and the panel
// keeps its drawn style (panel.hpp).
#include "gfx-reader.hpp"
#include <windows.h>
#include <bcrypt.h>
#include <string.h>
#include <stdio.h>
#include <string>
#include "gfx-signatures.hpp"
namespace dtgfx {
enum Art {
 // Native sprites, by pin name (same order as Names below).
 Panel,Page,Strip,Separator,Bar,HealthFrame,Demolish,DemolishOn,Next,NextOn,Cancel,CancelOn,Back,BackOn,Down,DownOn,Up,UpOn,Unload,UnloadOn,HouseTab,HouseTabOn,Sword,Stop,
 Infinite,InfiniteOn,Zero,Up5,Down5,BowOn,
 ResourcePanel, // the native resource panel's empty background (menu 1), under the Dark mana section
 FarmRaw,TempleRaw,FortressRaw,HallRaw,WarriorRaw,ThrowerRaw,GardenerRaw,ShamanRaw,FarmerRaw,SlaveRaw,PilotRaw,ManacopterRaw,CrystalRaw,
 // Derived: construction tile pictures (88x92) and greyed variants, panel
 // pictures (83x74), unit portraits (30x47), sub-tabs (37x33), small icons.
 FarmIcon,TempleIcon,FortressIcon,HallIcon,FarmIconGrey,TempleIconGrey,FortressIconGrey,HallIconGrey,
 FarmPicture,TemplePicture,FortressPicture,HallPicture,ManacopterPicture,SitePictureFarm,SitePictureTemple,SitePictureFortress,SitePictureHall,
 WarriorPortrait,ThrowerPortrait,GardenerPortrait,ShamanPortrait,FarmerPortrait,SlavePortrait,PilotPortrait,
 ManacopterIcon,ManacopterIconGrey,GardenerIcon,GardenerIconGrey,TabBuildings,TabBuildingsOn,TabAircraft,TabAircraftOn,Crystal,CrystalLarge,
 // Native selection bracket: the four gold corners (10x10) of a pressed tab.
 BracketTL,BracketTR,BracketBL,BracketBR,
 // The mana section's crystal (dtui::ManaCrystalW x ManaCrystalH).
 ManaCrystal,
 // Manacopter cells of the aircraft panel grid (beside a native 6x41 health bar): landed, and raised over a soft shadow when flying.
 ManacopterPortrait,ManacopterFlying,
 ArtCount};
constexpr int RawCount=CrystalRaw+1;
inline const char*rawName(int a){static const char*n[RawCount]={"panel","page","strip","separator","bar","healthFrame","demolish","demolishOn","next","nextOn","cancel","cancelOn","back","backOn","down","downOn","up","upOn","unload","unloadOn","houseTab","houseTabOn","sword","stop",
 "infinite","infiniteOn","zero","up5","down5","bowOn",
 "resourcePanel",
 "farm","temple","fortress","hall","warrior","thrower","gardener","shaman","farmer","slave","pilot","manacopter","crystal"};return a>=0&&a<RawCount?n[a]:"";}
inline bool sha256(const void*data,size_t n,uint8_t out[32]){
 BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE h=nullptr;bool ok=BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0&&BCryptCreateHash(alg,&h,nullptr,0,nullptr,0,0)>=0&&BCryptHashData(h,(PUCHAR)data,(ULONG)n,0)>=0&&BCryptFinishHash(h,out,32,0)>=0;
 if(h)BCryptDestroyHash(h);if(alg)BCryptCloseAlgorithmProvider(alg,0);return ok;
}
inline bool readRange(const std::wstring&path,uint64_t offset,uint32_t length,std::vector<uint8_t>&out){
 HANDLE f=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);if(f==INVALID_HANDLE_VALUE)return false;
 LARGE_INTEGER at;at.QuadPart=(LONGLONG)offset;out.assign(length,0);DWORD got=0;bool ok=SetFilePointerEx(f,at,nullptr,FILE_BEGIN)&&(!length||(ReadFile(f,out.data(),length,&got,nullptr)&&got==length));CloseHandle(f);return ok;
}
inline uint64_t fileSize(const std::wstring&path){WIN32_FILE_ATTRIBUTE_DATA d{};if(!GetFileAttributesExW(path.c_str(),GetFileExInfoStandard,&d))return 0;return ((uint64_t)d.nFileSizeHigh<<32)|d.nFileSizeLow;}
class Library {
 HBITMAP bitmaps[ArtCount]{};
public:
 bool ok=false;Image img[ArtCount];char why[160]="not loaded";
 const Image&operator[](Art a)const{return img[a];}
 void unload(){ok=false;for(auto&b:bitmaps){if(b)DeleteObject(b);b=nullptr;}for(auto&i:img)i=Image{};snprintf(why,sizeof(why),"disabled");}
 // dir: the game's Gfx folder with a trailing backslash.
 bool load(const std::wstring&dir){
  unload();std::vector<std::vector<uint8_t>> palettes(dtgfxpin::paletteCount);
  for(int i=0;i<dtgfxpin::paletteCount;i++){const auto&p=dtgfxpin::palettes[i];std::wstring path=dir+p.name;uint8_t sha[32];
   if(fileSize(path)!=p.size||!readRange(path,0,p.size,palettes[i])||!sha256(palettes[i].data(),p.size,sha)||memcmp(sha,p.sha,32)){snprintf(why,sizeof(why),"palette %ls differs from the pinned game files",p.name);return false;}}
  for(int a=0;a<RawCount;a++){
   const dtgfxpin::Sprite*s=nullptr;for(int i=0;i<dtgfxpin::spriteCount;i++)if(!strcmp(dtgfxpin::sprites[i].name,rawName(a)))s=&dtgfxpin::sprites[i];
   if(!s){snprintf(why,sizeof(why),"sprite %s is not pinned",rawName(a));return false;}
   wchar_t name[32];swprintf(name,32,L"%u.gfx",(unsigned)s->file);std::vector<uint8_t> bytes;uint8_t sha[32];
   if(!readRange(dir+name,s->offset,s->length,bytes)||!sha256(bytes.data(),bytes.size(),sha)||memcmp(sha,s->sha,32)){snprintf(why,sizeof(why),"sprite %s (%u.gfx #%u) differs from the pinned game files",s->name,(unsigned)s->file,(unsigned)s->index);return false;}
   const auto&pal=palettes[s->palette];size_t entries=(pal.size()-20)/2;
   if(s->paletteOffset+256>entries){snprintf(why,sizeof(why),"palette range of %s",s->name);return false;}
   std::vector<uint16_t> colors(256);memcpy(colors.data(),pal.data()+20+2*(size_t)s->paletteOffset,512);
   bool icon=a>=FarmRaw&&a<=ManacopterRaw; // world sprites: no ground shadow in menus
   if(!decode(bytes.data(),bytes.size(),colors.data(),colors.size(),img[a],!icon)||img[a].w!=s->w||img[a].h!=s->h){snprintf(why,sizeof(why),"sprite %s did not decode",s->name);return false;}
  }
  derive();ok=true;snprintf(why,sizeof(why),"%d native sprites verified",RawCount);return true;
 }
 void derive(){
  const Art raws[4]={FarmRaw,TempleRaw,FortressRaw,HallRaw};
  for(int k=0;k<4;k++){Image t=trim(img[raws[k]]);img[FarmIcon+k]=fit(t,88,92,true); /* construction tile picture (dtui::IconW x IconH) */img[FarmIconGrey+k]=greyed(img[FarmIcon+k]);img[FarmPicture+k]=fit(t,83,74,true);img[SitePictureFarm+k]=darker(greyed(img[FarmPicture+k]),85);}
  Image heli=trim(img[ManacopterRaw]);img[ManacopterPicture]=fit(heli,83,74);img[ManacopterIcon]=fit(heli,40,30);img[ManacopterIconGrey]=greyed(img[ManacopterIcon]);
  // Aircraft panel cells, 44x46 (dtui::AirPictureW x AirPictureH): the picture (44x36) on the cell floor when
  // landed; when flying at the top, over a soft elliptical shadow (translucent, never opaque black).
  {const int w=44,h=46,ph=36;Image body=fit(heli,w,ph,true),land,fly;land.w=fly.w=w;land.h=fly.h=h;land.px.assign((size_t)w*h,0);fly.px=land.px;
   for(int y=0;y<ph;y++)for(int x=0;x<w;x++){land.px[(size_t)(y+h-ph)*w+x]=body.at(x,y);fly.px[(size_t)y*w+x]=body.at(x,y);}
   for(int y=h-6;y<h;y++)for(int x=0;x<w;x++){double dx=(x+0.5-w/2.0)/16.0,dy=(y+0.5-(h-3))/3.2,d=dx*dx+dy*dy;uint32_t&c=fly.px[(size_t)y*w+x];if(d<1&&!(c>>24))c=(uint32_t)(0xB0*(1-d*d))<<24;}
   img[ManacopterPortrait]=land;img[ManacopterFlying]=fly;}
  // Portraits fill the native 30x47 cell like native portraits (world sprites are smaller, so they may be enlarged).
  const Art units[7]={WarriorRaw,ThrowerRaw,GardenerRaw,ShamanRaw,FarmerRaw,SlaveRaw,PilotRaw};
  for(int k=0;k<7;k++)img[WarriorPortrait+k]=fit(trim(img[units[k]]),30,47,true,2.0);
  img[GardenerIcon]=fit(trim(img[GardenerRaw]),40,30,false,2.0);img[GardenerIconGrey]=greyed(img[GardenerIcon]);
  Image fort=fit(trim(img[FortressRaw]),37,33,true);img[TabBuildings]=silver(fort,false);img[TabBuildingsOn]=silver(fort,true);
  Image tab=fit(heli,34,34);img[TabAircraft]=silver(tab,false);img[TabAircraftOn]=silver(tab,true);
  Image gem=teal(trim(img[CrystalRaw]));img[Crystal]=fit(gem,12,16);img[CrystalLarge]=fit(gem,24,34);
  img[ManaCrystal]=fit(gem,40,58); /* nearly the native size of the gem */
  // The pressed tab #487 is the bow tab #486 plus gold corner brackets: arms 10 px
  // long and 3 px thick along the 32x32 edges, where #486 is empty. Only those
  // arms are kept, so the corners can frame any cell (a 30x47 portrait).
  auto corner=[&](int x0,int y0){const Image&t=img[BowOn];Image c;c.w=c.h=10;c.px.assign(100,0);
   for(int y=0;y<10;y++)for(int x=0;x<10;x++){int sx=x0+x,sy=y0+y;if(sx<3||sx>=29||sy<3||sy>=29)c.px[(size_t)y*10+x]=t.at(sx,sy);}return c;};
  img[BracketTL]=corner(0,0);img[BracketTR]=corner(22,0);img[BracketBL]=corner(0,22);img[BracketBR]=corner(22,22);
 }
 // Alpha-blends art `a` with its top-left at (x,y). A cached DIB per image.
 bool draw(HDC dc,int x,int y,Art a,BYTE alpha=255){
  const Image&m=img[a];if(!ok||m.empty())return false;
  if(!bitmaps[a]){BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=m.w;bi.bmiHeader.biHeight=-m.h;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;void*bits=nullptr;
   bitmaps[a]=CreateDIBSection(nullptr,&bi,DIB_RGB_COLORS,&bits,nullptr,0);if(!bitmaps[a]||!bits)return false;memcpy(bits,m.px.data(),m.px.size()*4);}
  HDC mem=CreateCompatibleDC(dc);if(!mem)return false;HGDIOBJ old=SelectObject(mem,bitmaps[a]);BLENDFUNCTION blend{AC_SRC_OVER,0,alpha,AC_SRC_ALPHA};
  BOOL done=AlphaBlend(dc,x,y,m.w,m.h,mem,0,0,m.w,m.h,blend);SelectObject(mem,old);DeleteDC(mem);return done!=FALSE;
 }
};
inline Library&library(){static Library l;return l;}
inline Library&art(){return library();}
}

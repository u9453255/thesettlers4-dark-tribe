#include "gfx-assets.hpp"
#include <assert.h>
#include <stdio.h>
// Game folder from DARK_TRIBE_GAME_DIR (tools/build-native.py sets it); empty when not configured.
static std::wstring gameFolder(){wchar_t d[MAX_PATH];DWORD n=GetEnvironmentVariableW(L"DARK_TRIBE_GAME_DIR",d,MAX_PATH);if(!n||n>=MAX_PATH)return L"";std::wstring s(d);if(s.back()!=L'\\'&&s.back()!=L'/')s+=L'\\';return s;}
// Sprite decoding on synthetic data, then (when the pinned game files are
// present on this machine) loading every pinned sprite read-only.
int main(){
 std::vector<uint16_t> pal(256,0);pal[2]=0xF800;pal[3]=0x07E0;pal[4]=0x001F;pal[5]=0x0000;
 // 12-byte header, run-length: 2 transparent, red, 1 shadow, green, then a trailing over-long transparent run.
 {const uint8_t d[]={3,0,2,0,0,0,0,0,1,0,0,0, 0,2,2,1,1,3,0,9};dtgfx::Image m;assert(dtgfx::decode(d,sizeof(d),pal.data(),256,m));
  assert(m.w==3&&m.h==2&&m.px[0]==0&&m.px[1]==0&&m.px[2]==0xFFFF0000u&&m.px[3]==dtgfx::ShadowAlpha<<24&&m.px[4]==0xFF00FF00u&&m.px[5]==0);
  assert(dtgfx::decode(d,sizeof(d),pal.data(),256,m,false)&&m.px[3]==0);
  assert(!dtgfx::decode(d,15,pal.data(),256,m));}
 // Raw palette indices (type 32); pure black becomes near-black (HD colour key).
 {const uint8_t d[]={2,0,1,0,0,0,0,0,32,0,0,0, 4,5};dtgfx::Image m;assert(dtgfx::decode(d,sizeof(d),pal.data(),256,m));assert(m.px[0]==0xFF0000FFu&&m.px[1]==0xFF080808u);}
 // Byte header when the first word exceeds 860.
 {const uint8_t d[]={2,4,0,0,0,0,0,0, 0,3,3,0,4};dtgfx::Image m;assert(dtgfx::decode(d,sizeof(d),pal.data(),256,m)&&m.w==2&&m.h==4);assert(m.px[3]==0xFF00FF00u&&m.px[4]==0);}
 // Bad sizes are refused.
 {const uint8_t d[]={0,0,1,0,0,0,0,0,1,0,0,0};dtgfx::Image m;assert(!dtgfx::decode(d,sizeof(d),pal.data(),256,m));assert(!dtgfx::decode(d,sizeof(d),pal.data(),255,m));}
 puts("PASS: sprite decoding (runs, shadow, raw, byte header, colour key, bounds)");
 // Trim, fit (aspect kept, bottom aligned), resample keeps opaque averages.
 {dtgfx::Image s;s.w=8;s.h=4;s.px.assign(32,0);for(int y=1;y<3;y++)for(int x=2;x<6;x++)s.px[y*8+x]=0xFF204060u;
  auto t=dtgfx::trim(s);assert(t.w==4&&t.h==2);
  {dtgfx::Image m;m.w=12;m.h=3;m.px.assign(36,0);for(int x=0;x<6;x++)m.px[12+x]=0xFF505050u;m.px[12+11]=0xFF505050u;auto u=dtgfx::trim(m);assert(u.w==6&&u.h==1);} // detached marks ignored
  {dtgfx::Image m;m.w=3;m.h=12;m.px.assign(36,0);for(int y=0;y<6;y++)m.px[y*3+1]=0xFF505050u;m.px[11*3+1]=0xFF505050u;auto u=dtgfx::trim(m);assert(u.h==6&&u.w==1);}auto f=dtgfx::fit(t,10,10,true,10.0);assert(f.w==10&&f.h==10);
  assert(f.at(5,9)==0xFF204060u&&f.at(5,4)==0&&f.at(0,9)==0xFF204060u);auto r=dtgfx::resample(t,2,1);assert(r.at(0,0)==0xFF204060u&&r.at(1,0)==0xFF204060u);
  auto g=dtgfx::greyed(r);uint32_t c=g.at(0,0);assert(((c>>16)&255)==((c>>8)&255)&&((c>>8)&255)==(c&255));
  auto sv=dtgfx::silver(r,false),sp=dtgfx::silver(r,true);assert((sp.at(0,0)&255)<(sv.at(0,0)&255));}
 puts("PASS: trim, fit, resample and tones");
 // The real files (offline, read-only). Skipped where the game is not installed.
 const std::wstring game=gameFolder(),gfx=game+L"Gfx\\";
 if(!game.empty()&&GetFileAttributesW((gfx+L"9.gfx").c_str())!=INVALID_FILE_ATTRIBUTES){
  auto&lib=dtgfx::library();bool ok=lib.load(gfx);if(!ok)printf("load failed: %s\n",lib.why);assert(ok);
  assert(lib[dtgfx::Panel].w==210&&lib[dtgfx::Panel].h==390&&lib[dtgfx::Page].h==350&&lib[dtgfx::Strip].h==40);
  // The resource panel's background (menu 1, under the mana section) is opaque; the mana section's crystal.
  {const auto&m=lib[dtgfx::ResourcePanel];assert(m.w==210&&m.h==168);for(uint32_t c:m.px)assert((c>>24)==255);assert(lib[dtgfx::ManaCrystal].w==40&&lib[dtgfx::ManaCrystal].h==58);}
  for(int a=0;a<dtgfx::ArtCount;a++){assert(!lib[(dtgfx::Art)a].empty());for(uint32_t c:lib[(dtgfx::Art)a].px)assert(!((c>>24)==255&&!(c&0xFFFFFF)));}
  assert(lib[dtgfx::FarmIcon].w==88&&lib[dtgfx::FarmIcon].h==92&&lib[dtgfx::TemplePicture].w==83&&lib[dtgfx::WarriorPortrait].h==47);
  // Aircraft panel cells (44x46): landed on the floor; flying at the top over a translucent shadow.
  {const auto&land=lib[dtgfx::ManacopterPortrait];const auto&fly=lib[dtgfx::ManacopterFlying];assert(land.w==44&&land.h==46&&fly.w==44&&fly.h==46);
   auto lowest=[](const dtgfx::Image&m){int b=-1;for(int y=0;y<m.h;y++)for(int x=0;x<m.w;x++)if((m.at(x,y)>>24)==255)b=y;return b;};
   assert(lowest(land)>=land.h-3&&lowest(fly)>=0&&lowest(fly)+8<=lowest(land));uint32_t c=fly.at(22,fly.h-3);assert((c>>24)>0x40&&(c>>24)<255&&!(c&0xFFFFFF));}
  // Barracks recruitment controls (26x28, as in the native cluster) and the selection bracket corners: only the gold arms are kept.
  for(auto a:{dtgfx::Infinite,dtgfx::InfiniteOn,dtgfx::Zero,dtgfx::Up5,dtgfx::Down5,dtgfx::Up,dtgfx::Down})assert(lib[a].w==26&&lib[a].h==28);
  assert(lib[dtgfx::BowOn].w==32&&lib[dtgfx::BowOn].h==32);
  for(int a=dtgfx::BracketTL;a<=dtgfx::BracketBR;a++){const auto&m=lib[(dtgfx::Art)a];assert(m.w==10&&m.h==10);bool right=a==dtgfx::BracketTR||a==dtgfx::BracketBR,bottom=a>=dtgfx::BracketBL;int opaque=0;
   for(int y=0;y<10;y++)for(int x=0;x<10;x++)if(m.at(x,y)>>24){assert((right?x>=7:x<3)||(bottom?y>=7:y<3));opaque++;}assert(opaque>=40);}
  // A wrong folder fails closed.
  assert(!lib.load(game+L"Missing\\")&&!lib.ok);
  puts("PASS: pinned native sprites load and verify from the local game files");
 }else puts("SKIP: game files not present");
 return 0;
}

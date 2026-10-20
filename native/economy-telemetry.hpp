// Dark Tribe 0.2.15 round r44: computer economy telemetry, pure part. The bridge (offence-native.hpp ledger()) fills
// one Ledger per computer every EcoEvery ticks; csvRow formats one line of evidence/v02-economy.csv (first line
// csvHeader), which tools/economy-report.py reads. Design and sources: research/ECONOMY_TELEMETRY_0215.md.
#pragma once
#include <cstdint>
#include <cstdio>
namespace dtledger {
constexpr uint32_t EcoEvery=500; /* ticks between two rows of one computer */
constexpr int SettlerTypes=67,BuildingTypes=83,GoodCount=22;
// Free stock columns of the home economy, S4ModApi good ids: LOG BOARD STONE IRONORE COAL IRONBAR SWORD BOW BATTLEAXE
// BLOWGUN BACKPACKCATAPULT SHOVEL HAMMER AXE PICKAXE SAW BREAD FISH MEAT GRAIN FLOUR WATER.
constexpr int Goods[GoodCount]={22,7,32,21,10,20,34,8,5,6,38,31,18,4,25,28,9,11,24,16,12,36};
struct Ledger {uint8_t player=0;bool eco=false;int population=0,carriers=0,busy=0,reserve=0,keep=0,housing=0;int goods[GoodCount]={};int settlers[SettlerTypes]={};int ready[BuildingTypes]={};int sites[BuildingTypes]={};};
inline const char*csvHeader(){return "ms,pid,tick,player,eco,population,carriers,busy,idle,reserve,keep,housing,houses_s,houses_m,houses_b,buildings,sites,barracks,toolsmiths,weaponsmiths,ironmines,coalmines,ironsmelters,soldiers,leaders,builders,diggers,gardeners,log,board,stone,ironore,coal,ironbar,sword,bow,battleaxe,blowgun,catapult,shovel,hammer,axe,pickaxe,saw,bread,fish,meat,grain,flour,water,settlers,ready,sites_by_type";}
inline int sum(const int*a,int n){int s=0;for(int i=0;i<n;i++)s+=a[i];return s;}
// Soldiers of every race: settler types 29..44 and 61..63 (as the census counts them).
inline int soldiers(const Ledger&l){int s=0;for(int t=29;t<=44;t++)s+=l.settlers[t];for(int t=61;t<=63;t++)s+=l.settlers[t];return s;}
// "type:count" pairs separated by spaces for every type with a count above 0; truncated on a full buffer.
inline void list(char*out,size_t n,const int*a,int count){size_t at=0;if(!n)return;out[0]=0;for(int t=0;t<count;t++){if(a[t]<=0)continue;int w=snprintf(out+at,n-at,"%s%d:%d",at?" ":"",t,a[t]);if(w<0)break;at+=(size_t)w;if(at>=n){out[n-1]=0;break;}}}
// One CSV row in the header's column order; returns the length snprintf would write (n or more means truncated;
// out stays terminated). The three list columns come last so new fixed columns can only be appended before them.
inline int csvRow(const Ledger&l,uint32_t ms,uint32_t pid,uint32_t tick,char*out,size_t n){
 char s[700],r[900],b[400];list(s,sizeof(s),l.settlers,SettlerTypes);list(r,sizeof(r),l.ready,BuildingTypes);list(b,sizeof(b),l.sites,BuildingTypes);
 int w=snprintf(out,n,"%u,%u,%u,%u,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",ms,pid,tick,(unsigned)l.player,l.eco?1:0,l.population,l.carriers,l.busy,l.carriers-l.busy,l.reserve,l.keep,l.housing,l.ready[40],l.ready[41],l.ready[42],sum(l.ready,BuildingTypes),sum(l.sites,BuildingTypes),l.ready[24],l.ready[21],l.ready[22],l.ready[15],l.ready[17],l.ready[20],soldiers(l),l.settlers[44],l.settlers[3],l.settlers[2],l.settlers[50]);
 for(int g=0;g<GoodCount&&w>=0&&(size_t)w<n;g++)w+=snprintf(out+w,n-w,",%d",l.goods[g]);
 if(w>=0&&(size_t)w<n)w+=snprintf(out+w,n-w,",%s,%s,%s",s,r,b);
 return w;}
}

#pragma once
#include <vector>
#include <algorithm>
namespace dt {
struct WorldCell {int x,y;bool operator<(const WorldCell&o)const{return y==o.y?x<o.x:y<o.y;}bool operator==(const WorldCell&o)const{return x==o.x&&y==o.y;}};
template<class C> std::vector<WorldCell> reservation(const C*cells,int count,int x,int y){std::vector<WorldCell> out;for(int i=0;i<count;i++)for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++)out.push_back({x+cells[i].x+dx,y+cells[i].y+dy});std::sort(out.begin(),out.end());out.erase(std::unique(out.begin(),out.end()),out.end());return out;}
inline bool contains(const std::vector<WorldCell>&cells,int x,int y){return std::binary_search(cells.begin(),cells.end(),WorldCell{x,y});}
// Match the pinned Dark outline validator, rather than the walking-sector map.
inline bool darkBuildTerrain(unsigned terrain){unsigned group=terrain&0xf0;return terrain!=0x15&&(group==0x10||group==0x40||group==0x90||terrain==0x30);}
inline bool darkBuildFlags(unsigned flags){return (flags&0x5f)==4;}
inline bool darkBuildCell(bool dark,unsigned terrain,unsigned flags,unsigned entity){return dark&&darkBuildTerrain(terrain)&&darkBuildFlags(flags)&&!entity;}
inline bool darkHaloCell(bool dark,unsigned terrain,unsigned flags,unsigned entity){return dark&&darkBuildTerrain(terrain)&&(flags&0x1f)==4&&!entity;}
template<class P> bool clearReservation(const std::vector<WorldCell>&cells,P blocked){for(auto&c:cells)if(c.x<1||c.y<1||c.x>=575||c.y>=575||blocked(c.x,c.y))return false;return !cells.empty();}
template<class C,class P> bool clearBuilding(const C*mask,int count,int x,int y,P clear){
 std::vector<WorldCell> body;for(int i=0;i<count;i++)body.push_back({x+mask[i].x,y+mask[i].y});std::sort(body.begin(),body.end());
 auto halo=reservation(mask,count,x,y);return clearReservation(halo,[&](int a,int b){return !clear(a,b,contains(body,a,b));});
}
}

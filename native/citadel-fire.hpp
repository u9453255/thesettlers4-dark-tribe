#pragma once
#include <stdint.h>
// Dark citadel fire defence geometry (r30), shared by the offence director and
// Dark construction. Pure: no engine access.
//
// Native facts (file-only, research/AI_OFFENCE_0215.md r30): CDarkTempleRole
// (Temple 50 and Fortress 51 only; the Manacopter Hall has no fire) runs
// 0x1458C0 every 31 ticks per building. While the owner's ready-farm counter
// [0x10EF7AC + p*0x298] is above 0 it kills hostile units with a selection
// group on walkable Dark cells (flags&5==4) of the door's land sector within
// radius 16+2n (n=1..3) or 24 (n>=4) around the building position + (1,2).
// The radius is Euclidean: the native spiral 0xD69968 holds exactly the cells
// with dx*dx - dx*dy + dy*dy <= R*R (2,083 cells for R=24, up to 27 hex steps).
namespace dtfire {
constexpr int CentreDX=1,CentreDY=2;     // fire centre = building position + (1,2)
constexpr int FireReach=24;              // largest native kill radius (4+ ready farms)
constexpr int FireLine=27;               // no computer unit may stand within this, and no gardener target cell
constexpr int FarmReach=4;               // a Mushroom Farm ring lies within 4 of the farm position
constexpr int FarmLock=FireLine+FarmReach+2; // 33: a farm position must be at least this far from every fire centre
// Squared Euclidean distance on the axial hex grid (X = x - y/2, Y = 0.866*y), times 1.
inline int fire2(int x,int y,int cx,int cy){int dx=x-cx,dy=y-cy;return dx*dx-dx*dy+dy*dy;}
inline int centreX(int buildingX){return buildingX+CentreDX;}
inline int centreY(int buildingY){return buildingY+CentreDY;}
// True when a farm at (fx,fy) would lie too close to a citadel (Temple/Fortress) at building position (bx,by).
inline bool farmTooClose(int fx,int fy,int bx,int by){return fire2(fx,fy,centreX(bx),centreY(by))<FarmLock*FarmLock;}
}

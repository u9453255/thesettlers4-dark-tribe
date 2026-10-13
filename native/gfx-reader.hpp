#pragma once
// Settlers IV sprite decoding and image helpers (pure logic, gfx-tests.cpp).
// Format notes: tools/gfx_pins.py. Images are premultiplied 0xAARRGGBB, the
// layout of a 32-bit top-down DIB, ready for AlphaBlend.
#include <stdint.h>
#include <stddef.h>
#include <vector>
#include <algorithm>
namespace dtgfx {
struct Image {int w=0,h=0;std::vector<uint32_t> px;bool empty()const{return w<=0||h<=0;}
 uint32_t at(int x,int y)const{return px[(size_t)y*w+x];}};
constexpr uint32_t ShadowAlpha=0x50;
inline uint32_t rgb565(uint16_t v){uint32_t r=(v>>11)&31,g=(v>>5)&63,b=v&31;return 0xFF000000u|((r*255/31)<<16)|((g*255/63)<<8)|(b*255/31);}
// Pure black is the HD overlay's colour key: opaque black becomes near-black.
inline uint32_t noKey(uint32_t c){return (c>>24)==255&&!(c&0xFFFFFF)?0xFF080808u:c;}
// Decodes one encoded sprite (header + data, `n` bytes). `palette` holds the
// palette entries starting at this sprite's palette offset (at least 256).
// keepShadow=false drops shadow runs (building icons are shown without them).
inline bool decode(const uint8_t*p,size_t n,const uint16_t*palette,size_t entries,Image&out,bool keepShadow=true){
 out=Image{};if(n<8||entries<256)return false;
 int w,h,type;size_t pos;
 uint16_t first=(uint16_t)(p[0]|p[1]<<8);
 if(first>860){w=p[0];h=p[1];type=0;pos=8;}
 else{if(n<12)return false;w=first;h=p[2]|p[3]<<8;type=p[8];pos=12;}
 if(w<=0||h<=0||w>2048||h>2048)return false;
 out.w=w;out.h=h;out.px.assign((size_t)w*h,0);size_t total=(size_t)w*h,j=0;
 if(type==32){if(pos+total>n)return false;for(;j<total;j++)out.px[j]=noKey(rgb565(palette[p[pos+j]]));return true;}
 while(j<total){
  if(pos>=n)return false;uint8_t v=p[pos++];
  if(v<=1){if(pos>=n)return false;size_t run=p[pos++];uint32_t c=v==1&&keepShadow?ShadowAlpha<<24:0;for(size_t k=0;k<run&&j<total;k++)out.px[j++]=c;}
  else out.px[j++]=noKey(rgb565(palette[v]));
 }
 return true;
}
// The densest band of non-empty lines (gaps up to `gap` lines bridged):
// detached marks, such as the few ground-marker pixels below every settler
// frame, are left out of the box.
inline void band(const std::vector<int>&count,int gap,int&a,int&b){
 a=0;b=-1;int best=-1;
 for(int i=0;i<(int)count.size();){
  if(!count[i]){i++;continue;}int start=i,end=i,sum=0,empty=0;
  for(;i<(int)count.size();i++){if(count[i]){end=i;sum+=count[i];empty=0;}else if(++empty>gap)break;}
  if(sum>best){best=sum;a=start;b=end;}
 }
}
// Bounding box of the main body of pixels with alpha >= minAlpha; the whole
// image when none.
inline Image trim(const Image&s,uint32_t minAlpha=0x80){
 auto on=[&](int x,int y){return (s.at(x,y)>>24)>=minAlpha;};
 std::vector<int> rows(s.h,0);for(int y=0;y<s.h;y++)for(int x=0;x<s.w;x++)rows[y]+=on(x,y);
 int y0,y1;band(rows,2,y0,y1);if(y1<0)return s;
 std::vector<int> cols(s.w,0);for(int y=y0;y<=y1;y++)for(int x=0;x<s.w;x++)cols[x]+=on(x,y);
 int x0,x1;band(cols,2,x0,x1);if(x1<0)return s;Image o;o.w=x1-x0+1;o.h=y1-y0+1;o.px.resize((size_t)o.w*o.h);
 for(int y=0;y<o.h;y++)for(int x=0;x<o.w;x++)o.px[(size_t)y*o.w+x]=s.at(x0+x,y0+y);return o;
}
// Area-average resample of premultiplied pixels to exactly w x h.
inline Image resample(const Image&s,int w,int h){
 Image o;o.w=w;o.h=h;o.px.assign((size_t)w*h,0);if(s.empty()||w<=0||h<=0)return o;
 for(int y=0;y<h;y++){double sy0=(double)y*s.h/h,sy1=(double)(y+1)*s.h/h;
  for(int x=0;x<w;x++){double sx0=(double)x*s.w/w,sx1=(double)(x+1)*s.w/w,acc[4]{},area=0;
   for(int yy=(int)sy0;yy<s.h&&yy<sy1;yy++){double fy=std::min<double>(yy+1,sy1)-std::max<double>(yy,sy0);if(fy<=0)continue;
    for(int xx=(int)sx0;xx<s.w&&xx<sx1;xx++){double fx=std::min<double>(xx+1,sx1)-std::max<double>(xx,sx0);if(fx<=0)continue;double f=fx*fy;uint32_t c=s.at(xx,yy);
     acc[0]+=f*(c>>24);acc[1]+=f*((c>>16)&255);acc[2]+=f*((c>>8)&255);acc[3]+=f*(c&255);area+=f;}}
   if(area<=0)continue;auto ch=[&](int i){return (uint32_t)std::min(255.0,acc[i]/area+0.5);};
   uint32_t a=ch(0),r=std::min(ch(1),a),g=std::min(ch(2),a),b=std::min(ch(3),a);o.px[(size_t)y*w+x]=noKey((a<<24)|(r<<16)|(g<<8)|b);}}
 return o;
}
// Scales to fit inside w x h keeping the aspect ratio (never enlarging beyond
// `maxScale`), placed on a transparent w x h canvas: centred horizontally,
// bottom-aligned when `bottom`, otherwise centred vertically.
inline Image fit(const Image&s,int w,int h,bool bottom=false,double maxScale=1.0){
 Image o;o.w=w;o.h=h;o.px.assign((size_t)w*h,0);if(s.empty())return o;
 double k=std::min({(double)w/s.w,(double)h/s.h,maxScale});int tw=std::max(1,(int)(s.w*k+0.5)),th=std::max(1,(int)(s.h*k+0.5));
 Image r=resample(s,tw,th);int ox=(w-tw)/2,oy=bottom?h-th:(h-th)/2;
 for(int y=0;y<th;y++)for(int x=0;x<tw;x++)if(ox+x>=0&&oy+y>=0&&ox+x<w&&oy+y<h)o.px[(size_t)(oy+y)*w+ox+x]=r.at(x,y);return o;
}
// Per-pixel colour transform on unpremultiplied RGB; alpha kept.
template<class F> Image map(const Image&s,F f){
 Image o=s;for(auto&c:o.px){uint32_t a=c>>24;if(!a)continue;int r=(c>>16)&255,g=(c>>8)&255,b=c&255;r=r*255/a;g=g*255/a;b=b*255/a;f(r,g,b);
  r=std::max(0,std::min(255,r))*a/255;g=std::max(0,std::min(255,g))*a/255;b=std::max(0,std::min(255,b))*a/255;c=noKey((a<<24)|(r<<16)|(g<<8)|b);}
 return o;
}
inline int luma(int r,int g,int b){return (r*30+g*59+b*11)/100;}
// Native tab icons are silver relief: luminance mapped onto a blue-grey ramp.
inline Image silver(const Image&s,bool pressed){
 return map(s,[&](int&r,int&g,int&b){int l=luma(r,g,b);l=std::max(0,std::min(255,(l-40)*2+70));if(pressed)l=l*60/100;r=l*86/100;g=l*92/100;b=std::min(255,l+14);});
}
// Unaffordable/disabled: desaturated and darkened, like greyed native icons.
inline Image greyed(const Image&s){return map(s,[](int&r,int&g,int&b){int l=luma(r,g,b)*62/100+18;r=g=b=l;});}
inline Image darker(const Image&s,int percent){return map(s,[&](int&r,int&g,int&b){r=r*percent/100;g=g*percent/100;b=b*percent/100;});}
// Mana crystal: the native gem re-toned to the Dark mana teal, keeping its shading.
inline Image teal(const Image&s){return map(s,[](int&r,int&g,int&b){int l=luma(r,g,b);r=l*45/100;g=std::min(255,l*105/100+30);b=std::min(255,l*95/100+22);});}
}

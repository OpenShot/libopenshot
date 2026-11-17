// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
// Intermediate words are packed RGBA bytes or IEEE float alpha values. Binding
// 5 is retained across passes; barriers order all producer/consumer dispatches.
// GLSL's two-argument atan is undefined when x is zero. Metal's native
// approximation also differs noticeably near axes. Reduce to |z| <= tan(pi/8)
// and evaluate the convergent atan series, with explicit axes and quadrants.
float effect_atan2(float y,float x) {
    const float pi=3.141592653589793;
    if(y==0.0) return x<0.0?((floatBitsToUint(y)&0x80000000u)!=0u?-pi:pi):y;
    if(x==0.0) return y<0.0?-pi*.5:pi*.5;
    float ax=abs(x),ay=abs(y);precise float z=min(ax,ay)/max(ax,ay);
    bool reduced=z>0.414213562373095;
    if(reduced) z=(z-1.0)/(z+1.0);
    precise float z2=z*z,term=z,result=z;
    for(int n=1;n<=12;++n) {term*=-z2;result+=term/float(2*n+1);}
    if(reduced)result+=pi*.25;
    if(ay>ax)result=pi*.5-result;
    if(x<0.0)result=pi-result;
    return y<0.0?-result:result;
}
const int WORK=512+16384;
ivec4 bytes_at(ivec2 q) { return ivec4(round(texelFetch(plane_y,q,0)*255.0)); }
uint pack_bytes(ivec4 c) { return uint(c.r)|(uint(c.g)<<8)|(uint(c.b)<<16)|(uint(c.a)<<24); }
ivec4 unpack_bytes(uint c) { return ivec4(c&255u,(c>>8)&255u,(c>>16)&255u,c>>24); }
int pos_index(ivec2 q) { return q.y*p.sizes.z+q.x; }
float work_float(int base,ivec2 q) { return uintBitsToFloat(data[base+pos_index(q)]); }
void put_float(int base,ivec2 q,float v) { data[base+pos_index(q)]=floatBitsToUint(v); }
ivec4 over_bytes(ivec4 a,ivec4 b) {
    ivec4 t=b*(255-a.a)+128;
    return a+((t+(t>>8))>>8);
}
ivec3 premultiply_bytes(ivec3 rgb,int alpha) {
    ivec3 t=rgb*alpha;
    return (t+(t>>8)+128)>>8;
}
ivec4 straight_bytes(ivec4 c) {
    if(c.a==0) return ivec4(0);
    if(c.a<255) for(int channel=0;channel<3;++channel) {
        int index=c.a*256+c[channel];
        c[channel]=int((data[512+index/4]>>((index%4)*8))&255u);
    }
    return c;
}
// Qt's smooth downscaler accumulates 14-bit area weights, retaining ten
// horizontal fraction bits before its vertical pass (qimagescale.cpp).
uvec4 pixelate_row(ivec2 q,int first,int unit) {
    uvec4 sum=uvec4(bytes_at(q))*uint(first);
    int remain=16384-first;
    while(remain>unit) { ++q.x;sum+=uvec4(bytes_at(q))*uint(unit);remain-=unit; }
    ++q.x;
    if(remain>0) sum+=uvec4(bytes_at(q))*uint(remain);
    return sum;
}
int reflect101(int x,int size) {
    if(size==1) return 0;
    int period=2*(size-1);x=abs(x)%period;
    return x<size?x:period-x;
}
int outline_binary(ivec2 q) {
    q=clamp(q,ivec2(0),p.sizes.zw-1);
    return int(data[WORK+p.sizes.z*p.sizes.w+pos_index(q)]);
}
ivec2 outline_gradient(ivec2 q) {
    int a=outline_binary(q+ivec2(-1,-1)),b=outline_binary(q+ivec2(0,-1)),c=outline_binary(q+ivec2(1,-1));
    int d=outline_binary(q+ivec2(-1,0)),f=outline_binary(q+ivec2(1,0));
    int g=outline_binary(q+ivec2(-1,1)),h=outline_binary(q+ivec2(0,1)),i=outline_binary(q+ivec2(1,1));
    return ivec2(c+2*f+i-a-2*d-g,g+2*h+i-a-2*b-c);
}
int outline_magnitude(ivec2 q) {
    if(any(lessThan(q,ivec2(0))) || any(greaterThanEqual(q,p.sizes.zw))) return 0;
    ivec2 g=abs(outline_gradient(q));return g.x+g.y;
}
// Babl-compatible HSV/HSL, D50 Lab/LCH and limited-range YCbCr metrics.
int chroma_ray(ivec3 rgb) {
    int v=max(rgb.r,max(rgb.g,rgb.b));
    if((rgb.r==0 || rgb.r==v) && (rgb.g==0 || rgb.g==v) && (rgb.b==0 || rgb.b==v))
        return (int(rgb.r>0)+2*int(rgb.g>0)+4*int(rgb.b>0))*256+v;
    return -1;
}
vec3 chroma_hsv(ivec3 rgb) {
    vec3 v=vec3(value(256+rgb.r),value(256+rgb.g),value(256+rgb.b));
    float hi=max(v.r,max(v.g,v.b)),lo=min(v.r,min(v.g,v.b)),d=hi-lo,h=0;
    if(d>1e-10) {
        if(v.r==hi) h=(v.g-v.b)/d+(v.g<v.b?6.0:0.0);
        else if(v.g==hi) h=2.0+(v.b-v.r)/d;
        else h=4.0+(v.r-v.g)/d;
        h/=6.0;
    }
    int ray=chroma_ray(rgb);
    if(ray>=0) h=value(2336+65536*3+ray*5);
    return vec3(h,hi>1e-10?d/hi:0,hi);
}
float chroma_root(float x) {
    uint bits=floatBitsToUint(x);bits=bits/4u+bits/16u;bits+=bits/16u;bits+=bits/256u;
    precise float root=uintBitsToFloat(0x2a5137a0u+bits);
    root=0.33333333*(2.0*root+x/(root*root));
    root=0.33333333*(2.0*root+x/(root*root));
    return root;
}
vec3 chroma_lab(ivec3 rgb) {
    vec3 linear=vec3(value(512+rgb.r),value(512+rgb.g),value(512+rgb.b));
    precise vec3 xyz=vec3(value(768),value(771),value(774))*linear.r+
        vec3(value(769),value(772),value(775))*linear.g+
        vec3(value(770),value(773),value(776))*linear.b;
    vec3 f;
    for(int i=0;i<3;++i) f[i]=xyz[i]>(216.0/24389.0)?chroma_root(xyz[i]):((24389.0/27.0)*xyz[i]+16.0)/116.0;
    vec3 lab=vec3(116.0*f.y-16.0,500.0*(f.x-f.y),200.0*(f.y-f.z));

    return lab;
}
vec3 chroma_lch(ivec3 rgb) {
    int ray=chroma_ray(rgb);
    if(ray>=0) { int base=2336+65536*3+ray*5;return vec3(value(base+1),value(base+2),value(base+3)); }
    vec3 lab=chroma_lab(rgb);
    float hue=all(equal(lab.yz,vec2(0)))?0.0:degrees(effect_atan2(lab.z,lab.y));
    if(hue<0) hue+=360.0;
    return vec3(lab.x,length(lab.yz),hue);
}
vec3 unpack_lab(int packed) { return vec3(float(packed&255)/2.55,float((packed>>8)&255)-127.0,float((packed>>16)&255)-127.0); }
vec3 chroma_lab_byte(ivec3 rgb) {
    int ray=chroma_ray(rgb);
    if(ray>=0) return unpack_lab(int(value(2336+65536*3+ray*5+4)));
    vec3 lab=chroma_lab(rgb);
    lab=roundEven(clamp(vec3(lab.x/100.0*255.0,lab.y+128.0,lab.z+128.0),0.0,255.0));
    return vec3(lab.x/2.55,lab.y-127.0,lab.z-127.0);
}
float chroma_delta_e(vec3 key,vec3 pixel) {
    const float pi=3.141592653589793;
    if(all(equal(key,pixel))) return abs(p.matrix_x.w);
    float c1=length(key.yz),c2=length(pixel.yz),cbar=(c1+c2)/2.0;
    float multiplier=1.0+0.5*(1.0-sqrt(cbar/(cbar+25.0)));
    vec2 a=vec2(key.y*multiplier,key.z),b=vec2(pixel.y*multiplier,pixel.z);
    float c1p=length(a),c2p=length(b),cp=(c1p+c2p)/2.0,dc=c2p-c1p;
    float h1=effect_atan2(a.y,a.x)*180.0/pi,h2=effect_atan2(b.y,b.x)*180.0/pi;
    float dh=h2-h1,hbar=c1p!=0.0 && c2p!=0.0?(h1+h2)/2.0:h1+h2;
    if(dh < -180.0) { dh+=360.0;hbar+=hbar<180.0?180.0:-180.0; }
    else if(dh>180.0) { dh-=360.0;hbar+=hbar<180.0?180.0:-180.0; }
    float big_dh=2.0*sqrt(c1p*c2p)*sin(dh*pi/360.0);
    float t=1.0-0.17*cos((hbar-30.0)*pi/180.0)+0.24*cos(hbar*pi/90.0)+
        0.32*cos((3.0*hbar+6.0)*pi/180.0)-0.20*cos((4.0*hbar-64.0)*pi/180.0);
    float lbar=(key.x+pixel.x)/2.0,ld=lbar-50.0;
    float sl=1.0+0.015*ld*ld/sqrt(20.0+ld*ld),sc=1.0+0.045*cp,sh=1.0+0.015*cp*t;
    float rot=(hbar-275.0)/25.0;
    float rt=-2.0*sqrt(cp/(cp+25.0))*sin(pi/3.0*exp(-rot*rot));
    float dl=(pixel.x-key.x)/sl;
    // Preserve the existing CPU implementation's angular term here.
    return sqrt(dl*dl+(dc/sc)*(dc/sc)+(dh/sh)*(dh/sh)+rt*dc/sc*big_dh/sh);
}
ivec2 chroma_cbcr(ivec3 rgb) {
    ivec2 result;
    for(int component=0;component<2;++component) {
        int base=800+component*768;
        int accumulated=128*1048576+int(data[base+rgb.r])+int(data[base+256+rgb.g])+int(data[base+512+rgb.b]);
        int integer=accumulated>>20,fraction=accumulated&1048575;
        if(fraction>524288 || (fraction==524288 && (integer&1)!=0)) ++integer;
        result[component]=clamp(integer,16,240);
    }
    return result;
}
float chroma_metric(ivec3 rgb,ivec3 key,int method) {
    if(method==9) return chroma_delta_e(unpack_lab(int(p.clear.z)),chroma_lab_byte(rgb));
    if(all(equal(rgb,key))) return 0;
    if(method==10) return length(vec2(chroma_cbcr(rgb)-ivec2(p.clear.zw)));
    if(method<=5) {
        vec3 a=chroma_hsv(rgb),b=chroma_hsv(key);
        if(method==1) { float d=abs(a.x-p.clear.z);return min(d,1.0-d)*500.0; }
        if(method==4) return abs(a.z-p.clear.z)*255.0;
        int lowa=min(rgb.r,min(rgb.g,rgb.b)),higha=max(rgb.r,max(rgb.g,rgb.b));
        int lowb=min(key.r,min(key.g,key.b)),highb=max(key.r,max(key.g,key.b));
        int feature=method==2?0:method==3?1:2;
        return abs(value(2336+(lowa*256+higha)*3+feature)-p.clear.z)*255.0;
    }
    vec3 lch=chroma_lch(rgb);
    if(method==6) return abs(lch.x-p.clear.z);
    if(method==7) return abs(lch.y-p.clear.z);
    float d=abs(lch.z-p.clear.z);return min(d,360.0-d);
}

uint grain_mix(uint h) {
    h^=h>>16;h*=0x7feb352du;h^=h>>15;h*=0x846ca68bu;return h^(h>>16);
}
float grain_hash(ivec2 q,int t,uint salt) {
    uint h=data[272]^salt^uint(q.x)*0x9e3779b9u^uint(q.y)*0x85ebca6bu^uint(t)*0xc2b2ae35u;
    precise float v=float(grain_mix(h))*(2.0/4294967295.0)-1.0;return v;
}
float grain_noise(vec2 q,int t,uint salt) {
    ivec2 cell=ivec2(q);precise vec2 f=q-vec2(cell);f=f*f*(3.0-2.0*f);
    float a=grain_hash(cell,t,salt),b=grain_hash(cell+ivec2(0,1),t,salt);
    float c=grain_hash(cell+ivec2(1,0),t,salt),d=grain_hash(cell+ivec2(1,1),t,salt);
    precise float left=a+(b-a)*f.y,right=c+(d-c)*f.y;
    precise float result=left+(right-left)*f.x;return result;
}
void film_grain(ivec2 pixel) {
    ivec4 c=bytes_at(pixel);precise vec3 rgb=vec3(c.rgb);
    if(c.a!=255) rgb*=value(16+c.a);
    rgb*=1.0/255.0;
    precise float luma=.299*rgb.r+.587*rgb.g+.114*rgb.b;
    precise float centered=abs(luma-.5)*2.0;
    precise float tone=(1.0-luma)*(1.0-luma)*value(5)+clamp(1.0-centered*centered,0.0,1.0)*value(6)+luma*luma*value(7);
    if(c.a==0 || tone<=.00001) { imageStore(output_img,pixel,vec4(c)/255.0);return; }
    precise vec2 q=vec2(pixel)*vec2(value(12),value(13));
    const uint salts[4]=uint[4](0x1000193u,0x8da6b343u,0xd8163841u,0xcb1ab31fu);
    bool color=value(8)>.00001 && value(9)>.00001;
    precise vec4 grains=vec4(0);
    for(int s=0;s<(color?4:1);++s) {
        precise float g[2];
        for(int t=0;t<(value(15)>0.0?2:1);++t) {
            g[t]=grain_hash(ivec2(q*value(0)),int(value(14))+t,salts[s]);
            if(value(3)>.00001) {
                float soft=grain_noise(q*value(1),int(value(14))+t,salts[s]^0x51633e2du);
                g[t]+=(soft-g[t])*value(3);
            }
        }
        grains[s]=g[0];
        if(value(15)>0.0) grains[s]+=(g[1]-g[0])*value(10);
        if(value(4)>.00001) {
            float cluster=grain_noise(q*value(2),int(value(14)),salts[s]^0xa511e9b3u);
            grains[s]*=1.0+(.45+abs(cluster)*1.35-1.0)*value(4);
        }
    }
    precise vec3 delta=vec3(grains.x);
    if(color) { delta+=(grains.yzw-delta)*value(9);delta=vec3(grains.x)+(delta-vec3(grains.x))*value(8); }
    rgb=clamp(rgb+delta*(value(11)*tone),0.0,1.0)*255.0;
    if(c.a!=255) rgb*=float(c.a)*(1.0/255.0);
    imageStore(output_img,pixel,vec4(floor(rgb+.5),c.a)/255.0);
}
// Independent channel buffers keep 4K float intermediates within Vulkan storage-buffer limits.
vec3 denoise_read(int base,int index) {
    return vec3(uintBitsToFloat(data[base+index]),uintBitsToFloat(work_g[base+index]),uintBitsToFloat(work_b[base+index]));
}
void denoise_write(int base,int index,vec3 v) {
    data[base+index]=floatBitsToUint(v.r);work_g[base+index]=floatBitsToUint(v.g);work_b[base+index]=floatBitsToUint(v.b);
}
vec3 denoise_straight(ivec4 c) { return vec3(c.rgb)*(c.a==255?1.0:value(2565+c.a)); }
float denoise_luma(vec3 c) { precise float y=.299*c.r+.587*c.g+.114*c.b;return y; }
float denoise_shrink(float v,float threshold,float keep) {
    precise float result=abs(v)<=threshold?v*keep:(v<0.0?-1.0:1.0)*(threshold*keep+(abs(v)-threshold));return result;
}
void denoise(ivec2 pixel,int op) {
    int n=p.sizes.z*p.sizes.w,index=pos_index(pixel);
    if(op<38) {
        int radius=int(p.effect.x);bool horizontal=op==34 || op==36;
        int src=op==36?WORK+n:WORK,dst=op==35?WORK+n:op==37?WORK+2*n:WORK;
        precise vec3 sum=vec3(0);
        for(int k=-radius;k<=radius;++k) {
            ivec2 q=clamp(pixel+(horizontal?ivec2(k,0):ivec2(0,k)),ivec2(0),p.sizes.zw-1);
            sum+=op==34?denoise_straight(bytes_at(q)):denoise_read(src,pos_index(q));
        }
        denoise_write(dst,index,sum*(1.0/float(2*radius+1)));return;
    }
    ivec4 c=bytes_at(pixel);
    precise vec3 center=denoise_straight(c),fine=denoise_read(WORK+n,index),base=denoise_read(WORK+2*n,index);
    precise float cy=denoise_luma(center),fy=denoise_luma(fine),by=denoise_luma(base);
    int r=int(clamp(cy+.5,0.0,255.0));
    precise float out_y=by+denoise_shrink(fy-by,value(256+r),value(768+r))+denoise_shrink(cy-fy,value(r),value(512+r));
    precise vec3 rgb;
    for(int channel=0;channel<3;++channel) rgb[channel]=base[channel]+
        denoise_shrink(fine[channel]-base[channel],value(1536+r),value(2048+r))+
        denoise_shrink(center[channel]-fine[channel],value((channel==1?1280:1024)+r),value(1792+r));
    rgb+=vec3(out_y-denoise_luma(rgb));
    if(p.effect.x>0.0) {
        ivec4 prev=ivec4(round(texelFetch(background_tex,pixel,0)*255.0));
        precise vec3 previous=denoise_straight(prev),diff=abs(center-previous);
        precise float py=denoise_luma(previous),motion=max(abs(cy-py)*(1.0/255.0),(diff.r+diff.g+diff.b)*((1.0/255.0)/3.0)*.75);
        precise float t=clamp((motion-value(2563))/(value(2564)-value(2563)),0.0,1.0);
        precise float gate=1.0-t*t*(3.0-2.0*t);
        precise float mix_value=min(.35,value(2304+r)*gate*value(2560));
        precise vec3 chroma=rgb-out_y;
        out_y+=(py-out_y)*(mix_value*value(2561));
        rgb=out_y+chroma+((previous-py)-chroma)*(mix_value*value(2562));
    }
    if(c.a!=255) rgb*=float(c.a)*(1.0/255.0);
    rgb=floor(clamp(rgb,0.0,255.0)+.5);
    imageStore(output_img,pixel,vec4(rgb,c.a)/255.0);
}
uvec4 scale_sum(ivec2 q,int first,int unit,ivec2 step) {
    uvec4 sum=uvec4(bytes_at(q))*uint(first);int remain=16384-first;
    while(remain>unit) { q+=step;sum+=uvec4(bytes_at(q))*uint(unit);remain-=unit; }
    if(remain>0) sum+=uvec4(bytes_at(q+step))*uint(remain);
    return sum;
}
void scale_effect(ivec2 pixel) {
    int ax=3*pixel.x,ay=3*(p.sizes.x+pixel.y);
    ivec2 q=ivec2(data[ax],data[ay]);
    uint x=data[ax+1],y=data[ay+1];int cx=int(data[ax+2]),cy=int(data[ay+2]);
    uvec4 sum;
    if(cx==0 && cy==0) {
        // Qt's SSE2/NEON four-pixel interpolation rounds vertically first.
        uvec4 left=(uvec4(bytes_at(q))*(256u-y)+uvec4(bytes_at(q+ivec2(0,y>0u?1:0)))*y)>>8;
        uvec4 right=(uvec4(bytes_at(q+ivec2(x>0u?1:0,0)))*(256u-y)+uvec4(bytes_at(q+ivec2(x>0u?1:0,y>0u?1:0)))*y)>>8;
        sum=(left*(256u-x)+right*x)>>8;
    } else if(cx==0) {
        sum=scale_sum(q,int(y),cy,ivec2(0,1));
        if(x>0u) sum=(sum*(256u-x)+scale_sum(q+ivec2(1,0),int(y),cy,ivec2(0,1))*x)>>8;
        sum>>=14;
    } else if(cy==0) {
        sum=scale_sum(q,int(x),cx,ivec2(1,0));
        if(y>0u) sum=(sum*(256u-y)+scale_sum(q+ivec2(0,1),int(x),cx,ivec2(1,0))*y)>>8;
        sum>>=14;
    } else {
        sum=(scale_sum(q,int(x),cx,ivec2(1,0))>>4)*y;
        int remain=16384-int(y);
        while(remain>cy) { ++q.y;sum+=(scale_sum(q,int(x),cx,ivec2(1,0))>>4)*uint(cy);remain-=cy; }
        if(remain>0) sum+=(scale_sum(q+ivec2(0,1),int(x),cx,ivec2(1,0))>>4)*uint(remain);
        sum>>=24;
    }
    imageStore(output_img,pixel,vec4(sum)/255.0);
}
void displace(ivec2 pixel) {
    ivec4 map=ivec4(round(texelFetch(background_tex,pixel,0)*255.0));
    int gray=(map.r*11+map.g*16+map.b*5)>>5,processed=int(data[gray]);
    if(p.effect.z>0.0) processed=255-processed;
    processed=128+((processed-128)*map.a)/255;
    if(p.effect.w>0.0) { imageStore(output_img,pixel,vec4(vec3(processed)/255.0,1));return; }
    precise float signed_map=(float(processed)-127.5)/127.5;
    precise vec2 q=clamp(vec2(pixel)+signed_map*p.effect.xy,vec2(0),vec2(p.sizes.zw-1));
    ivec2 lo=ivec2(q),hi=min(lo+1,p.sizes.zw-1);precise vec2 f=q-vec2(lo);
    precise vec4 top=vec4(bytes_at(lo))*(1.0-f.x)+vec4(bytes_at(ivec2(hi.x,lo.y)))*f.x;
    precise vec4 bottom=vec4(bytes_at(ivec2(lo.x,hi.y)))*(1.0-f.x)+vec4(bytes_at(hi))*f.x;
    precise vec4 result=top*(1.0-f.y)+bottom*f.y;
    imageStore(output_img,pixel,floor(result+.5)/255.0);
}
uint tape_fnv(uint h,uint datum) {
    for(int i=0;i<4;++i) { h^=(datum>>(8*i))&255u;h*=16777619u; }return h;
}
float tape_hash(uint y,uint x) { return float(tape_fnv(tape_fnv(tape_fnv(data[10],data[9]),y),x))/4294967295.0; }
uint tape_step(inout uint h) { h^=h<<13;h^=h>>17;h^=h<<5;return h; }
vec3 tape_yuv(ivec2 q) {
    precise vec3 rgb=vec3(bytes_at(q).bgr)/255.0;
    precise vec3 yuv=vec3(.299*rgb.r+.587*rgb.g+.114*rgb.b,
        -.14713*rgb.r-.28886*rgb.g+.436*rgb.b,.615*rgb.r-.51499*rgb.g-.10001*rgb.b);return yuv;
}
void analog_tape(ivec2 pixel,int op) {
    int w=p.sizes.z,h=p.sizes.w,n=w*h,uw=(w+1)/2,base=int(data[11]),index=pos_index(pixel),row=16+pixel.y*16;
    if(op==41) {
        precise vec3 c=tape_yuv(pixel);data[base+index]=floatBitsToUint(c.x);
        if(pixel.x<uw) {
            ivec2 q=ivec2(2*pixel.x,pixel.y);precise vec2 uv=tape_yuv(q).yz;
            if(q.x+1<w) uv=(uv+tape_yuv(q+ivec2(1,0)).yz)*.5;
            work_g[base+pixel.y*uw+pixel.x]=floatBitsToUint(uv.x);work_b[base+pixel.y*uw+pixel.x]=floatBitsToUint(uv.y);
        }return;
    }
    if(op==42 || op==44) {
        if(pixel.x!=0) return;
        int width=op==42?w:uw,radius=int(value(op==42?2:3));
        int src=op==42?base:base+n,dst=op==42?base+n:base;
        precise vec3 sum=vec3(0);
        for(int k=-radius;k<=radius;++k) sum+=denoise_read(src,pixel.y*width+clamp(k,0,width-1));
        for(int x=0;x<width;++x) {
            int q=pixel.y*width+x;precise vec3 v=sum/float(2*radius+1);
            if(op==42) data[dst+q]=floatBitsToUint(v.r);
            else { work_g[dst+q]=floatBitsToUint(v.g);work_b[dst+q]=floatBitsToUint(v.b); }
            sum+=denoise_read(src,pixel.y*width+min(width-1,x+radius+1))-denoise_read(src,pixel.y*width+max(0,x-radius));
        }return;
    }
    if(op==43) {
        if(pixel.x>=uw) return;
        precise float xs=clamp(float(pixel.x)-value(0),0.0,float(uw-1));int lo=int(xs),hi=min(lo+1,uw-1);
        precise float f=xs-float(lo);
        precise vec3 v=denoise_read(base,pixel.y*uw+lo)*(1.0-f)+denoise_read(base,pixel.y*uw+hi)*f;
        work_g[base+n+pixel.y*uw+pixel.x]=floatBitsToUint(v.g);work_b[base+n+pixel.y*uw+pixel.x]=floatBitsToUint(v.b);return;
    }
    if(op==45) {
        // One invocation per row preserves the CPU random/streak recurrence.
        if(pixel.x!=0) return;
        uint s0=data[row+3],s1=data[row+4],yref=data[row+9];precise float sl=0,sr=0;
        for(int x=0;x<w;++x) {
            int q=pixel.y*w+x;uint xref=uint(floor(float(x)*value(7)+.5));
            if(tape_hash(yref,xref)<value(row+1)) sl=1;
            if(tape_hash(yref,data[8]-1u-xref)<value(row+1)*.7) sr=1;
            precise float noise=(float(tape_step(s0)&0xffffffu)/16777215.0)*(1.0-value(4))+
                (float(tape_step(s1)&0xffffffu)/16777215.0)*value(4);
            precise float y=uintBitsToFloat(data[base+n+q]),mt=clamp((y-.2)/(.8-.2),0.0,1.0);
            precise float val=y+value(row+5)+value(row+6)*(2.0*noise-1.0)*(.6+.4*mt)+value(row+2);
            precise float streak=value(6)*(sl+sr),result=val+streak*(value(row+7)+(1.0-val));
            data[base+q]=floatBitsToUint(clamp(result,0.0,1.0));sl*=value(5);sr*=value(5);
        }
        for(int x=0;x<uw;++x) {
            int q=base+pixel.y*uw+x;
            work_g[q]=floatBitsToUint(uintBitsToFloat(work_g[q])*value(row));
            work_b[q]=floatBitsToUint(uintBitsToFloat(work_b[q])*value(row));
        }return;
    }
    if(op==46) {
        precise float off=value(row+8),xs=clamp(float(pixel.x)+off,0.0,float(w-1));int lo=int(xs),hi=min(lo+1,w-1);
        precise float f=xs-float(lo),yv=uintBitsToFloat(data[base+pixel.y*w+lo])*(1.0-f)+uintBitsToFloat(data[base+pixel.y*w+hi])*f;
        data[base+n+index]=floatBitsToUint(yv);
        if(pixel.x<uw) {
            xs=clamp(float(pixel.x)+off*.5,0.0,float(uw-1));lo=int(xs);hi=min(lo+1,uw-1);f=xs-float(lo);
            precise vec3 v=denoise_read(base,pixel.y*uw+lo)*(1.0-f)+denoise_read(base,pixel.y*uw+hi)*f;
            work_g[base+n+pixel.y*uw+pixel.x]=floatBitsToUint(v.g);work_b[base+n+pixel.y*uw+pixel.x]=floatBitsToUint(v.b);
        }return;
    }
    int lo=pixel.x/2,hi=min(lo+1,uw-1);float f=float(pixel.x%2)*.5;
    precise vec3 uv=denoise_read(base+n,pixel.y*uw+lo)*(1.0-f)+denoise_read(base+n,pixel.y*uw+hi)*f;
    uv*=value(1);float yv=uintBitsToFloat(data[base+n+index]);
    precise vec3 rgb=vec3(yv+1.13983*uv.b,yv-.39465*uv.g-.58060*uv.b,yv+2.03211*uv.g);
    imageStore(output_img,pixel,vec4(ivec3(clamp(rgb.bgr,0.0,1.0)*255.0),bytes_at(pixel).a)/255.0);
}
ivec4 byte_multiply(ivec4 c,int alpha) {
    ivec4 t=c*alpha;return (t+(t>>8)+128)>>8;
}
ivec3 flare_add(ivec3 rgb,float amount,int color) {
    precise vec3 delta=vec3(ivec3(255)-rgb)*amount;
    delta*=vec3(value(color),value(color+1),value(color+2));
    return clamp(rgb+ivec3(delta),ivec3(0),ivec3(255));
}
void lens_flare(ivec2 pixel) {
    precise vec2 delta=vec2(pixel)-vec2(value(0),value(1));
    precise float d=sqrt(delta.x*delta.x+delta.y*delta.y);
    ivec3 rgb=ivec3(0);
    for(int ring=0;ring<5;++ring) {
        float radius=value(4+ring);precise float amount;
        if(ring<4) {
            if(d>=radius) continue;
            amount=(radius-d)/radius;if(ring<3) amount*=amount;
        } else {
            amount=abs((d-radius)/(radius*.07));if(!(amount<1.0)) continue;
            amount=1.0-amount;
        }
        rgb=flare_add(rgb,amount,16+3*ring);
    }
    for(int ref=0;ref<int(value(9));++ref) {
        int base=32+8*ref,type=int(value(base+3));float radius=value(base+2);
        precise vec2 q=vec2(value(base),value(base+1))-vec2(pixel);
        precise float distance=sqrt(q.x*q.x+q.y*q.y),amount=0;
        if(type==1) { amount=(radius-distance)/radius;if(!(amount>0.0)) continue;amount*=amount; }
        else if(type==2 || type==3) {
            amount=(radius-distance)/(radius*(type==2?.15:.12));if(!(amount>0.0)) continue;
            amount=min(amount,1.0);if(type==3) amount=1.0-amount*.12;
        } else {
            amount=abs((distance-radius)/(radius*.04));if(!(amount<1.0)) continue;amount=1.0-amount;
        }
        rgb=flare_add(rgb,amount,base+4);
    }
    int alpha=max(rgb.r,max(rgb.g,rgb.b));
    ivec4 original=bytes_at(pixel),overlay=ivec4(premultiply_bytes(rgb,alpha),alpha);
    int coverage=int(value(3));
    ivec4 sum=min(original+overlay,ivec4(255));
    ivec4 mixed=sum*coverage+original*(255-coverage);
    mixed=(mixed+(mixed>>8)+128)>>8;
    precise float oa=float(original.a)/255.0,fa=(float(alpha)/255.0)*value(2);
    int final_alpha=int(clamp(max(oa,fa)*255.0,0.0,255.0));
    imageStore(output_img,pixel,vec4(byte_multiply(mixed,final_alpha))/255.0);
}
void object_mask(ivec2 pixel,int op) {
    if(op==49) {
        uint index=uint(pixel.y*p.sizes.x+pixel.x),lo=0u,hi=data[0];
        while(lo<hi) { uint mid=lo+(hi-lo)/2u;if(data[1u+mid]<=index) lo=mid+1u;else hi=mid; }
        float alpha=lo<data[0] && (lo&1u)!=0u?1.0:0.0;
        imageStore(output_img,pixel,vec4(alpha));return;
    }
    if(op==50) {
        if(pixel.x!=0) return;
        uint sum=0u;
        for(int x=0;x<p.sizes.z;++x) { sum+=uint(bytes_at(ivec2(x,pixel.y)).a>0);data[WORK+pixel.y*p.sizes.z+x]=sum; }
        return;
    }
    int mask_alpha=int(round(texelFetch(background_tex,pixel,0).a*255.0));
    ivec4 fill=ivec4(p.effect);fill.rgb=premultiply_bytes(fill.rgb,fill.a);
    ivec4 result=over_bytes(byte_multiply(fill,mask_alpha),bytes_at(pixel));
    if(data[0]>0u && mask_alpha==0) {
        bool dilated=false;int radius=int(data[1]);
        for(int row=0;row<int(data[0]);++row) {
            int y=pixel.y+row-radius;if(y<0 || y>=p.sizes.w) continue;
            int extent=int(data[16+row]),left=max(0,pixel.x-extent),right=min(p.sizes.z-1,pixel.x+extent);
            uint count=data[WORK+y*p.sizes.z+right];
            if(left>0) count-=data[WORK+y*p.sizes.z+left-1];
            if(count>0u) { dilated=true;break; }
        }
        if(dilated) { ivec4 stroke=ivec4(p.clear);stroke.rgb=premultiply_bytes(stroke.rgb,stroke.a);result=over_bytes(stroke,result); }
    }
    imageStore(output_img,pixel,vec4(result)/255.0);
}
ivec3 stabilize_at(ivec2 q) {
    if(any(lessThan(q,ivec2(0))) || any(greaterThanEqual(q,p.sizes.zw))) return ivec3(0);
    return bytes_at(q).rgb;
}
void stabilize(ivec2 pixel) {
    if(p.effect.x==0.0) { imageStore(output_img,pixel,vec4(vec3(bytes_at(pixel).rgb)/255.0,1));return; }
    int x=(int(data[2*pixel.x])+int(data[2*(p.sizes.z+pixel.y)]))>>5;
    int y=(int(data[2*pixel.x+1])+int(data[2*(p.sizes.z+pixel.y)+1]))>>5;
    ivec2 q=clamp(ivec2(x>>5,y>>5),ivec2(-32768),ivec2(32767));int fx=x&31,fy=y&31;
    ivec3 c=stabilize_at(q)*((32-fx)*(32-fy))+stabilize_at(q+ivec2(1,0))*(fx*(32-fy))+
        stabilize_at(q+ivec2(0,1))*((32-fx)*fy)+stabilize_at(q+ivec2(1,1))*(fx*fy);
    imageStore(output_img,pixel,vec4(vec3((c+512)>>10)/255.0,1));
}
vec2 spherical_uv(ivec2 pixel) {
    int model=int(value(15)),projection=int(value(16));precise vec3 ray;
    if(projection<=1) {
        precise float x=(2.0*(float(pixel.x)+.5)/float(p.sizes.z)-1.0)*value(11);
        precise float y=-(2.0*(float(pixel.y)+.5)/float(p.sizes.w)-1.0)*value(12);
        ray=vec3(value(9)*x+value(10)*y,-value(10)*x+value(9)*y,-1);
        ray/=sqrt(ray.x*ray.x+ray.y*ray.y+1.0);
    } else {
        float radius=.5*float(min(p.sizes.z,p.sizes.w));
        precise float x=(float(pixel.x)+.5-float(p.sizes.z)*.5)/radius,y=-(float(pixel.y)+.5-float(p.sizes.w)*.5)/radius;
        precise vec2 q=vec2(value(9)*x+value(10)*y,-value(10)*x+value(9)*y);
        float r=length(q),tmax=value(14)*.5;if(r>1.0) return vec2(-1);
        float theta=projection==3?2.0*asin(clamp(r*sin(tmax*.5),-1.0,1.0)):
            projection==4?2.0*atan(r*tan(tmax*.5)):projection==5?asin(clamp(r*sin(tmax),-1.0,1.0)):r*tmax;
        float phi=r>0.0?effect_atan2(q.y,q.x):0.0;
        ray=vec3(sin(theta)*cos(phi),sin(theta)*sin(phi),-cos(theta));
    }
    precise vec3 world=vec3(value(0)*ray.x+value(1)*ray.y+value(2)*ray.z,
        value(3)*ray.x+value(4)*ray.y+value(5)*ray.z,value(6)*ray.x+value(7)*ray.y+value(8)*ray.z);
    const float pi=3.141592653589793;
    if(model==0) {
        float lon=effect_atan2(world.x,-world.z),lat=effect_atan2(world.y,length(world.xz));
        if(projection==1) lon=clamp(lon,-pi*.5,pi*.5);
        return vec2((lon+(projection==1?pi*.5:pi))/(projection==1?pi:2.0*pi)*float(p.sizes.z),(pi*.5-lat)/pi*float(p.sizes.w));
    }
    float theta=effect_atan2(length(world.xy),world.z*(value(17)>0.0?1.0:-1.0)),tmax=max(.000001,value(13)*.5);
    float r=model==2?sin(theta*.5)/max(1e-12,sin(tmax*.5)):model==3?tan(theta*.5)/max(1e-12,tan(tmax*.5)):
        model==4?sin(theta)/max(1e-12,sin(tmax)):theta/tmax;
    float phi=length(world.xy)>0.0?effect_atan2(world.y,world.x):0.0;
    float radius=.5*float(min(p.sizes.z,p.sizes.w))*r;
    return vec2(float(p.sizes.z)*.5+radius*cos(phi),float(p.sizes.w)*.5-radius*sin(phi));
}
ivec4 spherical_at(ivec2 q,int level) {
    if(level<0) return straight_bytes(bytes_at(clamp(q,ivec2(0),p.sizes.zw-1)));
    ivec2 size=ivec2(data[33+3*level],data[34+3*level]);q=clamp(q,ivec2(0),size-1);
    return unpack_bytes(data[int(data[35+3*level])+q.y*size.x+q.x]);
}
vec4 spherical_cubic(vec4 a,vec4 b,vec4 c,vec4 d,float t) {
    precise vec4 a0=-.5*a+1.5*b-1.5*c+.5*d,a1=a-2.5*b+2.0*c-.5*d,a2=-.5*a+.5*c;
    precise vec4 v=((a0*t+a1)*t+a2)*t+b;return v;
}
void spherical(ivec2 pixel,int op) {
    int index=pos_index(pixel),n=p.sizes.z*p.sizes.w;
    if(op==53) { vec2 uv=spherical_uv(pixel);data[WORK+2*index]=floatBitsToUint(uv.x);data[WORK+2*index+1]=floatBitsToUint(uv.y);return; }
    if(op==55) {
        int level=int(p.effect.x),w=int(data[33+3*level]),h=int(data[34+3*level]),base=int(data[35+3*level]);
        if(pixel.x>=w || pixel.y>=h) return;
        ivec4 c=level==0?straight_bytes(bytes_at(pixel)):(spherical_at(pixel*2,level-1)+spherical_at(pixel*2+ivec2(1,0),level-1)+
            spherical_at(pixel*2+ivec2(0,1),level-1)+spherical_at(pixel*2+ivec2(1,1),level-1))/4;
        data[base+pixel.y*w+pixel.x]=pack_bytes(c);return;
    }
    precise vec2 uv=vec2(uintBitsToFloat(data[WORK+2*index]),uintBitsToFloat(data[WORK+2*index+1]));
    int projection=int(value(16)),model=int(value(15)),quality=int(value(18));
    if(model==0 && projection==0) { if(uv.x<0.0) uv.x+=float(p.sizes.z);if(uv.x>=float(p.sizes.z)) uv.x-=float(p.sizes.z);uv.y=clamp(uv.y,0.0,float(p.sizes.w-1)); }
    else if(model==0 && projection==1) uv=clamp(uv,vec2(0),vec2(p.sizes.zw-1));
    else if(any(lessThan(uv,vec2(0))) || any(greaterThanEqual(uv,vec2(p.sizes.zw)))) { imageStore(output_img,pixel,vec4(0));return; }
    int level=-1;
    if(quality==3) {
        vec2 dx=vec2(0),dy=vec2(0);
        if(pixel.x+1<p.sizes.z) dx=vec2(uintBitsToFloat(data[WORK+2*index+2]),uintBitsToFloat(data[WORK+2*index+3]))-uv;
        if(pixel.y+1<p.sizes.w) dy=vec2(uintBitsToFloat(data[WORK+2*(index+p.sizes.z)]),uintBitsToFloat(data[WORK+2*(index+p.sizes.z)+1]))-uv;
        float scale=max(length(dx),length(dy));level=scale>1.0?min(int(floor(log2(scale))),int(data[32])-1):0;
        uv/=float(1<<level);
    }
    ivec2 size=level<0?p.sizes.zw:ivec2(data[33+3*level],data[34+3*level]),q=clamp(ivec2(floor(uv)),ivec2(0),size-1);
    precise vec2 f=uv-vec2(q);precise vec4 c;
    if(quality==0) c=vec4(spherical_at(q,level));
    else if(quality==2) {
        vec4 rows[4];
        for(int y=-1;y<=2;++y) rows[y+1]=spherical_cubic(vec4(spherical_at(q+ivec2(-1,y),level)),vec4(spherical_at(q+ivec2(0,y),level)),
            vec4(spherical_at(q+ivec2(1,y),level)),vec4(spherical_at(q+ivec2(2,y),level)),f.x);
        c=spherical_cubic(rows[0],rows[1],rows[2],rows[3],f.y);
    } else {
        precise vec4 top=vec4(spherical_at(q,level))*(1.0-f.x)+vec4(spherical_at(q+ivec2(1,0),level))*f.x;
        precise vec4 bottom=vec4(spherical_at(q+ivec2(0,1),level))*(1.0-f.x)+vec4(spherical_at(q+ivec2(1,1),level))*f.x;
        c=top*(1.0-f.y)+bottom*f.y;
    }
    ivec4 result=ivec4(floor(clamp(c,0.0,255.0)+.5));result.rgb=premultiply_bytes(result.rgb,result.a);
    imageStore(output_img,pixel,vec4(result)/255.0);
}
float paint_primitive(float x) { return x<=0.0?0.0:x>=1.0?x-.5:.5*x*x; }
// Fixed-point edge intersections, matching Qt's 8-bit area accumulator.
int gray_edge(vec2 a,vec2 b,ivec2 pixel) {
    int direction=a.y<b.y?1:-1;if(a.y>b.y){vec2 temp=a;a=b;b=temp;}
    a*=256.0;b*=256.0;
    float top=max(a.y,float(pixel.y*256)),bottom=min(b.y,float((pixel.y+1)*256));if(bottom<=top)return 0;
    float slope=(b.x-a.x)/(b.y-a.y);
    vec2 start=vec2(top==a.y?a.x:floor(a.x+(top-a.y)*slope),top),end=vec2(bottom==b.y?b.x:floor(a.x+(bottom-a.y)*slope),bottom);
    float left=float(pixel.x*256),right=left+256.0,dy=bottom-top;
    if(max(start.x,end.x)<=left)return direction*int(dy*512.0);
    if(min(start.x,end.x)>=right)return 0;
    if(start.x==end.x)return direction*int(dy*(512.0-2.0*(start.x-left)));
    float yleft=clamp(floor(top+(left-start.x)*dy/(end.x-start.x)),top,bottom),
        yright=clamp(floor(top+(right-start.x)*dy/(end.x-start.x)),top,bottom);
    float first=min(yleft,yright),last=max(yleft,yright),left_cover=end.x>start.x?yleft-top:bottom-yleft;
    float xfirst=clamp(start.x,left,right)-left,xlast=clamp(end.x,left,right)-left;
    return direction*int(512.0*(left_cover+last-first)-(xfirst+xlast)*(last-first));
}
void paint_path(ivec2 pixel) {
    ivec4 original=bytes_at(pixel);
    if(float(pixel.x+1)<p.clear.x || float(pixel.y+1)<p.clear.y || float(pixel.x)>p.clear.z || float(pixel.y)>p.clear.w) {
        imageStore(output_img,pixel,vec4(original)/255.0);return;
    }
    int area=0;
    for(int edge=0;edge<int(p.matrix_x.w);++edge) {
        int base=4*edge;vec2 a=vec2(value(base),value(base+1)),b=vec2(value(base+2),value(base+3));
        if(a.y!=b.y)area+=gray_edge(a,b,pixel);
    }
    int alpha=abs(area>>9);
    if(fract(p.matrix_x.w)>0.0) {alpha=alpha%512;if(alpha>256)alpha=512-alpha;}
    alpha=clamp(alpha,0,255);
    ivec4 color=ivec4(p.effect);color.rgb=premultiply_bytes(color.rgb,color.a);
    imageStore(output_img,pixel,vec4(over_bytes(byte_multiply(color,alpha),original))/255.0);
}
void paint_glyph(ivec2 pixel) {
    ivec2 q=pixel-ivec2(p.effect.xy);ivec4 original=bytes_at(pixel);
    if(any(lessThan(q,ivec2(0))) || any(greaterThanEqual(q,ivec2(p.effect.zw)))) { imageStore(output_img,pixel,vec4(original)/255.0);return; }
    int alpha=int(round(texelFetch(background_tex,q+ivec2(int(data[0]),int(p.matrix_x.w)),0).a*255.0));
    ivec4 color=ivec4(p.clear);color.rgb=premultiply_bytes(color.rgb,color.a);
    imageStore(output_img,pixel,vec4(over_bytes(byte_multiply(color,alpha),original))/255.0);
}
void cosmetic_stroke(ivec2 pixel) {
    ivec4 result=bytes_at(pixel),color=ivec4(p.effect);color.rgb=premultiply_bytes(color.rgb,color.a);
    for(int segment=0;segment<int(p.clear.x);++segment) {
        int base=8*segment;bool vertical=data[base]!=0u;int major=vertical?pixel.y:pixel.x,minor=vertical?pixel.x:pixel.y;
        int start=int(data[base+1]),end=int(data[base+2]);if(major<start || major>end) continue;
        int extent=major==start?int(data[base+5]):major==end?int(data[base+6]):64;if(extent==0) continue;
        int position=int(data[base+3])+(major-start)*int(data[base+4]),lo=position>>16,fraction=(position>>8)&255;
        int coverage=minor==lo?255-fraction:minor==lo+1?fraction:0;
        coverage=(coverage*extent)>>6;coverage=(coverage*int(data[base+7]))>>8;
        if(coverage>0) result=over_bytes(byte_multiply(color,coverage),result);
    }
    imageStore(output_img,pixel,vec4(result)/255.0);
}
void fill_rect(ivec2 pixel) {
    int left=int(data[0]),top=int(data[1]),right=int(data[2]),bottom=int(data[3]),l=left>>16,r=right>>16;
    int width=0;
    if(pixel.x>=l && pixel.x<=r) {
        // Preserve Qt's raster fill coverage for rectangles inside one column,
        // including its extra full-column contribution and byte truncation.
        if(l==r) width=65536-(left&65535)+(right&65535);
        else width=pixel.x==l?65536-(left&65535):pixel.x==r?right&65535:65536;
    }
    int height=max(0,min((pixel.y+1)*65536,bottom)-max(pixel.y*65536,top));
    uint high,low;umulExtended(uint(height),uint(width*255),high,low);
    int coverage=int(high&255u); // Qt stores span coverage as an unsigned byte.
    ivec4 color=ivec4(p.effect);color.rgb=premultiply_bytes(color.rgb,color.a);
    imageStore(output_img,pixel,vec4(over_bytes(byte_multiply(color,coverage),bytes_at(pixel)))/255.0);
}
ivec4 vector_color(int cmd,ivec2 pixel) {
    int kind=int(data[cmd+5]);
    if(kind==0) {uint c=data[cmd+17];return ivec4(c&255u,(c>>8)&255u,(c>>16)&255u,c>>24);}
    vec2 point=vec2(pixel)+.5,a=vec2(value(cmd+12),value(cmd+13));float t;
    if(kind==1) {vec2 d=vec2(value(cmd+14),value(cmd+15))-a;t=dot(point-a,d)/max(dot(d,d),1e-10);}
    else if(kind==2) t=length(point-a)/max(value(cmd+14),1e-10);
    else {vec2 d=point-a;t=1.0-(effect_atan2(d.y,d.x)+radians(value(cmd+14)))/6.28318530718;}
    int spread=int(data[cmd+16]);
    int index=int(t*1023.0+.5);
    if(kind==3 || spread==2){index=index%1024;if(index<0)index+=1024;}
    else if(spread==1){index=index%2048;if(index<0)index+=2048;if(index>=1024)index=2047-index;}
    else index=clamp(index,0,1023);
    t=float(index)/1023.0;
    int first=int(data[cmd+6]),count=int(data[cmd+7]);
    vec4 color=vec4(value(first+1),value(first+2),value(first+3),value(first+4));
    for(int i=1;i<count;++i) {
        int previous=first+5*(i-1),next=first+5*i;
        vec4 c=vec4(value(next+1),value(next+2),value(next+3),value(next+4));
        if(t<=value(next)) {color=mix(color,c,clamp((t-value(previous))/max(value(next)-value(previous),1e-10),0.0,1.0));break;}
        color=c;
    }
    return ivec4(floor(color+.5));
}
void vector_paint(ivec2 pixel) {
    ivec4 result=bytes_at(pixel);int tile=(pixel.y/16)*int(data[0])+pixel.x/16;
    int begin=int(data[4+2*tile]),count=int(data[5+2*tile]),commands=4+2*int(data[0]*data[1]);
    for(int item=0;item<count;++item) {
        int cmd=commands+24*int(data[begin+item]);
        if(float(pixel.x)<value(cmd+8) || float(pixel.y)<value(cmd+9) || float(pixel.x)>=value(cmd+10) || float(pixel.y)>=value(cmd+11))continue;
        if(data[cmd]==2u) {
            ivec4 color=vector_color(cmd,pixel);
            for(int line=0;line<int(data[cmd+3]);++line) {
                int base=int(data[cmd+2])+8*line;bool vertical=data[base]!=0u;
                int major=vertical?pixel.y:pixel.x,minor=vertical?pixel.x:pixel.y,start=int(data[base+1]),end=int(data[base+2]);
                if(major<start || major>end)continue;
                int pos=int(data[base+3])+(major-start)*int(data[base+4]),coverage;
                if(data[cmd+1]!=0u) {
                    int extent=major==start?int(data[base+5]):major==end?int(data[base+6]):64;
                    int lo=pos>>16,fraction=(pos>>8)&255;coverage=minor==lo?255-fraction:minor==lo+1?fraction:0;coverage=(coverage*extent)>>6;
                } else coverage=minor==(pos>>16)?255:0;
                coverage=(coverage*int(data[base+7]))>>8;
                if(coverage>0)result=over_bytes(byte_multiply(color,coverage),result);
            }
            continue;
        }
        bool aa=data[cmd+1]!=0u;int alpha=0;
        if(data[cmd]==1u) {
            vec4 r=vec4(value(cmd+18),value(cmd+19),value(cmd+20),value(cmd+21));
            if(aa) {
                int l=int(clamp(r.x,0.0,float(p.sizes.z))*65536.0),t=int(clamp(r.y,0.0,float(p.sizes.w))*65536.0),
                    right=int(clamp(r.z,0.0,float(p.sizes.z))*65536.0),bottom=int(clamp(r.w,0.0,float(p.sizes.w))*65536.0);
                int x0=l>>16,x1=right>>16,width=0;
                if(pixel.x>=x0 && pixel.x<=x1) width=x0==x1?65536-(l&65535)+(right&65535):pixel.x==x0?65536-(l&65535):pixel.x==x1?right&65535:65536;
                int height=max(0,min((pixel.y+1)*65536,bottom)-max(pixel.y*65536,t));uint hi,lo;umulExtended(uint(height),uint(width*255),hi,lo);alpha=int(hi&255u);
            } else alpha=all(greaterThanEqual(vec2(pixel),floor(r.xy+.5))) && all(lessThan(vec2(pixel),floor(r.zw+.5)))?255:0;
        } else {
            int area=0,winding=0;
            int table=int(data[cmd+22]),row=table+2*(pixel.y-int(data[cmd+23]));
            int edges=table+int(data[row]),edge_count=int(data[row+1]);
            if(!aa) {
                for(int item=0;item<edge_count;++item) {
                    int edge=int(data[edges+item]),base=int(data[cmd+2])+5*edge,top=int(data[base]),bottom=int(data[base+1]);
                    if(pixel.y<top || pixel.y>bottom)continue;
                    int boundary=(int(data[base+2])+(pixel.y-top)*int(data[base+3]))>>16;
                    if(boundary>pixel.x)winding+=int(data[base+4]);
                }
                alpha=(data[cmd+4]!=0u?(abs(winding)%2)!=0:winding!=0)?255:0;
            } else {
                for(int item=0;item<edge_count;++item) {
                    int edge=int(data[edges+item]),base=int(data[cmd+2])+4*edge;vec2 a=vec2(value(base),value(base+1)),b=vec2(value(base+2),value(base+3));
                    if(a.y!=b.y)area+=gray_edge(a,b,pixel);
                }
                alpha=abs(area>>9);if(data[cmd+4]!=0u){alpha=alpha%512;if(alpha>256)alpha=512-alpha;}alpha=clamp(alpha,0,255);
            }
        }
        if(alpha>0)result=over_bytes(byte_multiply(vector_color(cmd,pixel),alpha),result);
    }
    imageStore(output_img,pixel,vec4(result)/255.0);
}
int blend_div255(int x) {x+=128;return (x+(x>>8))>>8;}
int blend_channel(int d,int s,int da,int sa,int mode) {
    int temp=s*(255-da)+d*(255-sa);
    if(mode==13)return blend_div255(s*d+temp);
    if(mode==14)return 255-(((255-s)*(255-d))>>8);
    if(mode==15)return blend_div255(2*d<da?2*s*d+temp:sa*da-2*(da-d)*(sa-s)+temp);
    if(mode==16)return blend_div255(min(s*da,d*sa)+temp);
    if(mode==17)return blend_div255(max(s*da,d*sa)+temp);
    if(mode==18){
        if(s*da+d*sa>sa*da)return blend_div255(sa*da+temp);
        if(s==sa || sa==0)return blend_div255(temp);
        return blend_div255(255*d*sa/(255-255*s/sa)+temp);
    }
    if(mode==19){
        if(s*da+d*sa<sa*da)return blend_div255(temp);
        if(s==0)return blend_div255(d*sa+temp);
        return blend_div255(sa*(s*da+d*sa-sa*da)/s+temp);
    }
    if(mode==20)return blend_div255(2*s<sa?2*s*d+temp:sa*da-2*(da-d)*(sa-s)+temp);
    if(mode==21){
        int dn=da!=0?255*d/da:0,t=temp*255;
        if(2*s<sa)return (d*(sa*255+(2*s-sa)*(255-dn))+t)/65025;
        if(4*d<=da)return (d*sa*255+da*(2*s-sa)*((((16*dn-12*255)*dn+3*65025)*dn)/65025)+t)/65025;
        return (d*sa*255+da*(2*s-sa)*(int(sqrt(float(dn*255)))-dn)+t)/65025;
    }
    if(mode==22)return s+d-blend_div255(2*min(s*da,d*sa));
    return s+d-((s*d)>>7); // Exclusion, matching Qt's 8-bit path.
}
ivec4 composite_bytes(ivec4 s,ivec4 d,int mode) {
    if(mode==0)return over_bytes(s,d);
    if(mode==1)return over_bytes(d,s);
    if(mode==2)return ivec4(0);
    if(mode==3)return s;
    if(mode==4)return d;
    if(mode==5)return byte_multiply(s,d.a);
    if(mode==6)return byte_multiply(d,s.a);
    if(mode==7)return byte_multiply(s,255-d.a);
    if(mode==8)return byte_multiply(d,255-s.a);
    if(mode==9)return byte_multiply(s,d.a)+byte_multiply(d,255-s.a);
    if(mode==10)return byte_multiply(d,s.a)+byte_multiply(s,255-d.a);
    if(mode==11)return byte_multiply(s,255-d.a)+byte_multiply(d,255-s.a);
    if(mode==12)return min(s+d,ivec4(255));
    return ivec4(blend_channel(d.r,s.r,d.a,s.a,mode),blend_channel(d.g,s.g,d.a,s.a,mode),
        blend_channel(d.b,s.b,d.a,s.a,mode),255-(((255-s.a)*(255-d.a))>>8));
}
void composite_frame(ivec2 pixel) {
    ivec4 d;
    if(p.modes.y==1)d=ivec4(round(texelFetch(background_tex,pixel,0)*255.0));
    else {d=ivec4(p.effect.yzw,p.clear.x);d.rgb=premultiply_bytes(d.rgb,d.a);}
    ivec4 result=composite_bytes(bytes_at(pixel),d,int(p.effect.x));int alpha=int(p.clear.y);
    if(alpha<255)for(int i=0;i<4;++i)result[i]=blend_div255(result[i]*alpha+d[i]*(255-alpha));
    imageStore(output_img,pixel,vec4(clamp(result,0,255))/255.0);
}
void paint_text_mask(ivec2 pixel) {
    ivec4 original=bytes_at(pixel);float coverage=texelFetch(background_tex,pixel,0).a;
    if(value(8)>0.0){
        int area=0;for(int edge=0;edge<int(p.effect.y);++edge){int base=9+4*edge;
            vec2 a=vec2(value(base),value(base+1)),b=vec2(value(base+2),value(base+3));if(a.y!=b.y)area+=gray_edge(a,b,pixel);}
        coverage*=float(clamp(abs(area>>9),0,255))/255.0;
    }
    int alpha=clamp(int(round(coverage*255.0)),0,255);ivec4 painted=composite_bytes(ivec4(255),original,int(p.effect.x));
    ivec4 result;for(int i=0;i<4;++i)result[i]=blend_div255(painted[i]*alpha+original[i]*(255-alpha));
    imageStore(output_img,pixel,vec4(result)/255.0);
}
void spatial_effect(ivec2 pixel) {
    if(p.modes.w==66){paint_text_mask(pixel);return;}
    if(p.modes.w==64){composite_frame(pixel);return;}
    if(p.modes.w==65){
        imageStore(output_img,pixel,(pixel.y&1)==0?texelFetch(background_tex,pixel,0):texelFetch(plane_y,pixel,0));return;
    }
    if(p.modes.w==63) {vector_paint(pixel);return;}
    if(p.modes.w==62) {
        ivec4 c=ivec4(p.effect);c.rgb=premultiply_bytes(c.rgb,c.a);
        imageStore(output_img,pixel,vec4(c)/255.0);return;
    }
    if(p.modes.w==61) { fill_rect(pixel);return; }
    if(p.modes.w==60) { cosmetic_stroke(pixel);return; }
    if(p.modes.w==57) { paint_path(pixel);return; }
    if(p.modes.w==59) { paint_glyph(pixel);return; }
    if(p.modes.w>=53 && p.modes.w<=55) { spherical(pixel,p.modes.w);return; }
    if(p.modes.w==56) { stabilize(pixel);return; }
    if(p.modes.w==52) { float alpha=texelFetch(plane_y,pixel,0).a;imageStore(output_img,pixel,vec4(vec3(alpha),1));return; }
    if(p.modes.w>=49 && p.modes.w<=51) { object_mask(pixel,p.modes.w);return; }
    if(p.modes.w==48) { lens_flare(pixel);return; }
    if(p.modes.w>=41 && p.modes.w<=47) { analog_tape(pixel,p.modes.w);return; }
    if(p.modes.w==39) { scale_effect(pixel);return; }
    if(p.modes.w==40) { displace(pixel);return; }
    if(p.modes.w>=34 && p.modes.w<=38) { denoise(pixel,p.modes.w);return; }
    if(p.modes.w==33) { film_grain(pixel);return; }
    if(p.modes.w==31) {
        int index=pixel.y*p.sizes.z+pixel.x;
        precise float moved=float(index)+value(pixel.y);
        int q=int(moved);
        if(abs(moved-float(q))>=0.5) q+=moved<0.0?-1:1;
        q=clamp(q,0,p.sizes.z*p.sizes.w-1);
        imageStore(output_img,pixel,texelFetch(plane_y,ivec2(q%p.sizes.z,q/p.sizes.z),0));return;
    }
    if(p.modes.w==32) {
        imageStore(output_img,pixel,texelFetch(plane_y,ivec2(pixel.x,int(data[pixel.y])),0));return;
    }
    int op=p.modes.w;
    int n=p.sizes.z*p.sizes.w;
    int index=pos_index(pixel);
    if(op==14) {
        ivec4 c=bytes_at(pixel);
        int method=int(p.effect.w);
        if(method>0 && method<11) {
            float distance=chroma_metric(c.rgb,ivec3(p.effect.xyz),method);
            if(distance<=p.clear.x) c=ivec4(0);
            else if(method!=8 && p.clear.y>0.0 && distance<=p.clear.x+p.clear.y)
                c=ivec4(vec4(c)*((distance-p.clear.x)/p.clear.y));
        } else if(c.a>0) {
            ivec3 rgb=c.rgb;
            if(c.a<255) rgb=ivec3(vec3(rgb)*value(c.a));
            bool hard=p.effect.w==0.0;
            rgb=hard?(rgb&255):clamp(rgb,ivec3(0),ivec3(255));
            ivec3 d=rgb-ivec3(p.effect.xyz);
            int mean=(rgb.r+int(p.effect.x))/2;
            int distance2=(((512+mean)*d.r*d.r)>>8)+4*d.g*d.g+(((767-mean)*d.b*d.b)>>8);
            int distance=int(sqrt(float(distance2))+0.0001);
            if(distance<=int(p.clear.x)) c=ivec4(0);
            else if(!hard && p.clear.y>0.0 && distance2<=int(p.clear.z)) {
                precise float factor=(sqrt(float(distance2))-p.clear.x)*p.clear.w;
                c=ivec4(vec4(c)*factor);
            }
        }
        imageStore(output_img,pixel,vec4(c)/255.0);return;
    }
    if(op==15) {
        ivec2 origin=ivec2(p.effect.xy),size=ivec2(p.effect.zw),small=p.sizes.xy;
        ivec2 increment=(size*65536)/small;
        ivec2 position=pixel*increment;
        ivec2 q=origin+(position>>16);
        ivec2 unit=(small*16384+size-1)/size;
        ivec2 first=((65536-(position&65535))*unit)>>16;
        uvec4 sum;
        if(small.x==size.x) { // A no-op scale still paints source-over below.
            sum=uvec4(bytes_at(q));
        } else if(small.y==size.y) {
            sum=pixelate_row(q,first.x,unit.x)>>14;
        } else {
            sum=(pixelate_row(q,first.x,unit.x)>>4)*uint(first.y);
            int remain=16384-first.y;
            while(remain>unit.y) { ++q.y;sum+=(pixelate_row(q,first.x,unit.x)>>4)*uint(unit.y);remain-=unit.y; }
            ++q.y;
            if(remain>0) sum+=(pixelate_row(q,first.x,unit.x)>>4)*uint(remain);
            sum>>=24;
        }
        imageStore(output_img,pixel,vec4(sum)/255.0);return;
    }
    if(op==16) {
        ivec4 c=bytes_at(pixel);
        ivec2 rel=pixel-ivec2(p.effect.xy),area=ivec2(p.effect.zw);
        if(all(greaterThanEqual(rel,ivec2(0))) && all(lessThan(rel,area))) {
            ivec2 small=textureSize(background_tex,0);
            ivec2 step=(small*65536)/area;
            ivec2 q=(rel*step+step/2)>>16;
            ivec4 front=ivec4(round(texelFetch(background_tex,min(q,small-1),0)*255.0));
            c=over_bytes(front,c);
        }
        imageStore(output_img,pixel,vec4(c)/255.0);return;
    }
    if(op==17) { data[WORK+index]=pack_bytes(straight_bytes(bytes_at(pixel)));return; }
    if(op==18) {
        int src=int(p.effect.x),dst=int(p.effect.y),radius=int(p.effect.z);
        ivec2 direction=p.effect.w==0.0?ivec2(1,0):ivec2(0,1);
        ivec3 total=ivec3(0);
        for(int i=-radius;i<=radius;++i) total+=unpack_bytes(data[src+pos_index(clamp(pixel+i*direction,ivec2(0),p.sizes.zw-1))]).rgb;
        precise vec3 scaled=vec3(total)*p.clear.x+0.5;
        data[dst+index]=pack_bytes(ivec4(ivec3(scaled),255));return;
    }
    if(op==19 || op==20) {
        ivec4 c=straight_bytes(bytes_at(pixel));
        ivec3 b=unpack_bytes(data[int(p.effect.x)+index]).rgb;
        vec3 d=vec3(c.rgb-b);
        precise float dy=0.114*d.b+0.587*d.g+0.299*d.r;
        if(op==19) { atomicMax(data[0],floatBitsToUint(abs(dy)));return; }
        float threshold=p.effect.z*uintBitsToFloat(data[0]);
        if(abs(dy)>=threshold) {
            float amount=p.effect.y;
            vec3 outc;
            if(p.effect.w==1.0) {
                if(p.clear.x==1.0) outc=vec3(b)+amount*dy*vec3(0.299,0.587,0.114);
                else if(p.clear.x==2.0) outc=vec3(b)+amount*(d-dy*vec3(0.299,0.587,0.114));
                else outc=vec3(b)+amount*d;
            } else {
                if(p.clear.x==1.0) outc=vec3(c.rgb)+amount*dy*((255.0-abs(dy))*(1.0/255.0));
                else {
                    if(p.clear.x==2.0) d-=dy;
                    outc=vec3(c.rgb)+amount*d*((255.0-abs(d))*(1.0/255.0));
                }
            }
            c.rgb=ivec3(clamp(outc,0.0,255.0)+0.5);
        }
        c.rgb=premultiply_bytes(c.rgb,c.a);
        imageStore(output_img,pixel,vec4(c)/255.0);return;
    }
    if(op==21) { put_float(WORK,pixel,float(bytes_at(pixel).a)/255.0);return; }
    if(op==22 || op==24) {
        int src=int(p.effect.x),dst=int(p.effect.y),radius=int(p.effect.z);
        ivec2 direction=p.effect.w==0.0?ivec2(1,0):ivec2(0,1);
        float result=0.0;
        for(int i=-radius;i<=radius;++i) {
            float v=work_float(src,clamp(pixel+i*direction,ivec2(0),p.sizes.zw-1));
            if(op==22) result=max(result,v);else result+=v;
        }
        if(op==24) result*=p.clear.x;
        put_float(dst,pixel,result);return;
    }
    if(op==23) {
        ivec2 q=pixel-ivec2(p.effect.xy);
        float v=all(greaterThanEqual(q,ivec2(0))) && all(lessThan(q,p.sizes.zw))?work_float(WORK,q):0.0;
        put_float(WORK+n,pixel,p.effect.z!=0.0?1.0-v:v);return;
    }
    if(op==25) {
        float alpha=work_float(WORK+n,pixel);
        if(p.effect.x!=0.0) alpha*=work_float(WORK,pixel);
        alpha=clamp(alpha*p.effect.y*(p.clear.w/255.0),0.0,1.0);
        ivec4 overlay=ivec4(vec4(p.clear.rgb,255)*alpha),source=bytes_at(pixel);
        ivec4 result=p.effect.x!=0.0?over_bytes(overlay,source):over_bytes(source,overlay);
        imageStore(output_img,pixel,vec4(result)/255.0);return;
    }
    if(op==26 || op==29) {
        int count=op==26?int(data[0]):7;
        int radius=count/2,base=op==26?1:224;
        uint sum=0u;
        for(int i=-radius;i<=radius;++i) {
            ivec2 q=ivec2(reflect101(pixel.x+i,p.sizes.z),pixel.y);
            uint v=op==26?uint(bytes_at(q).a):data[WORK+pos_index(q)];
            sum+=v*data[base+i+radius];
        }
        data[(op==26?WORK:WORK+2*n)+index]=sum;return;
    }
    if(op==27 || op==30) {
        int count=op==27?int(data[0]):7;
        int radius=count/2,base=op==27?1:224;
        uint sum=0u;
        for(int i=-radius;i<=radius;++i) {
            ivec2 q=ivec2(pixel.x,reflect101(pixel.y+i,p.sizes.w));
            sum+=data[(op==27?WORK:WORK+2*n)+pos_index(q)]*data[base+i+radius];
        }
        uint blurred=(sum+32768u)>>16;
        if(op==27) { data[WORK+n+index]=blurred>0u?255u:0u;return; }
        ivec4 c=bytes_at(pixel);
        if(c.a>0) c.rgb=premultiply_bytes(c.rgb,c.a);
        else if(data[WORK+n+index]!=0u || blurred>0u) {
            c=ivec4(p.effect);c.rgb=premultiply_bytes(c.rgb,c.a);
        } else c=ivec4(0);
        imageStore(output_img,pixel,vec4(c)/255.0);return;
    }
    if(op==28) {
        ivec2 gradient=outline_gradient(pixel);
        int x=abs(gradient.x),y=abs(gradient.y)<<15,m=x+abs(gradient.y);
        bool edge=false;
        if(m>255) {
            int tg22=x*13573;
            if(y<tg22) edge=m>outline_magnitude(pixel+ivec2(-1,0)) && m>=outline_magnitude(pixel+ivec2(1,0));
            else if(y>tg22+(x<<16)) edge=m>outline_magnitude(pixel+ivec2(0,-1)) && m>=outline_magnitude(pixel+ivec2(0,1));
            else {
                int sign=(gradient.x^gradient.y)<0?-1:1;
                edge=m>outline_magnitude(pixel+ivec2(-sign,-1)) && m>outline_magnitude(pixel+ivec2(sign,1));
            }
        }
        data[WORK+index]=edge?255u:0u;return;
    }

}

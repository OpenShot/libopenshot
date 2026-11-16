// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
// Intermediate words are packed RGBA bytes or IEEE float alpha values. Binding
// 5 is retained across passes; barriers order all producer/consumer dispatches.
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
    float hue=all(equal(lab.yz,vec2(0)))?0.0:degrees(atan(lab.z,lab.y));
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
    float h1=atan(a.y,a.x)*180.0/pi,h2=atan(b.y,b.x)*180.0/pi;
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

void spatial_effect(ivec2 pixel) {
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

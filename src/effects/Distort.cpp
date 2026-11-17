// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "Distort.h"
#include "Exceptions.h"
#include "Logger.h"
#include "MeshWarp.h"
#ifdef OPENSHOT_ENABLE_GPU_FRAME
#include "VulkanCompositor.h"
#include "VulkanFrameImage.h"
#endif
#include <algorithm>
#include <cmath>
#include <vector>
#include <QPointF>

using namespace openshot;
namespace {
using Grid = std::array<QPointF, 16>;
constexpr double pi = 3.14159265358979323846;
QPointF catmull(QPointF a, QPointF b, QPointF c, QPointF d, double t) {
    return (b * 2.0 + (c-a)*t + (a*2.0-b*5.0+c*4.0-d)*t*t
            + (-a+b*3.0-c*3.0+d)*t*t*t) * 0.5;
}
// Linear extrapolation at boundaries preserves an exactly flat identity grid.
QPointF at(const Grid& g, int x, int y) {
    if (x < 0) return at(g,0,y)*2.0-at(g,1,y);
    if (x > 3) return at(g,3,y)*2.0-at(g,2,y);
    if (y < 0) return at(g,x,0)*2.0-at(g,x,1);
    if (y > 3) return at(g,x,3)*2.0-at(g,x,2);
    return g[y*4+x];
}
QPointF surface(const Grid& g, double u, double v) {
    int x = std::min(2, int(u*3)), y = std::min(2, int(v*3));
    double tx = u*3-x, ty = v*3-y;
    QPointF rows[4];
    for (int r=0; r<4; ++r)
        rows[r] = catmull(at(g,x-1,y+r-1),at(g,x,y+r-1),
                          at(g,x+1,y+r-1),at(g,x+2,y+r-1),tx);
    return catmull(rows[0],rows[1],rows[2],rows[3],ty);
}
Grid controls(const Distort& effect, int64_t frame) {
    Grid g;
    double amount = effect.strength.GetValue(frame);
    if (!std::isfinite(amount)) amount = 0.0;
    amount = std::max(-2.0, std::min(2.0, amount));
    for (int y=0; y<4; ++y) for (int x=0; x<4; ++x) {
        int i=y*4+x;
        double u=x/3.0, v=y/3.0, dx=0, dy=0;
        switch (effect.preset) {
        case 1: dy=-0.22*4*u*(1-u); break;
        case 2: dy=0.16*std::sin(2*pi*u); break;
        case 3: case 4: {
            double f=(effect.preset==3 ? 0.6 : -0.6)*std::sin(pi*u)*std::sin(pi*v);
            dx=(u-0.5)*f; dy=(v-0.5)*f; break;
        }
        case 5: dx=(u-0.5)*(-0.5*(1-v)); break;
        case 6: dx=-(u-0.5)*0.65*std::sin(pi*v); break;
        }
        double ox=effect.points_x[i].GetValue(frame), oy=effect.points_y[i].GetValue(frame);
        if (!std::isfinite(ox)) ox=0;
        if (!std::isfinite(oy)) oy=0;
        ox=std::max(-1.0,std::min(1.0,ox)); oy=std::max(-1.0,std::min(1.0,oy));
        g[i]=QPointF(u+amount*(dx+ox),v+amount*(dy+oy));
    }
    return g;
}
struct Vertex { QPointF p, uv; };
struct Triangle { Vertex a,b,c; double det; int left,right,top,bottom; };
double cross(QPointF a, QPointF b) { return a.x()*b.y()-a.y()*b.x(); }
}

Distort::Distort() : strength(1.0) {
    for (int i=0; i<16; ++i) { points_x[i]=Keyframe(0.0); points_y[i]=Keyframe(0.0); }
    init_effect_details();
}
void Distort::init_effect_details() {
    InitEffectInfo();
    info.class_name="Distort"; info.name="Distort";
    info.description="Bend and stretch an image with a smooth, animated 4 by 4 mesh.";
    info.has_audio=false; info.has_video=true;
}
std::string Distort::MeshJSON(int64_t number, int divisions) const {
    divisions=std::max(3,std::min(96,divisions));
    Grid g=controls(*this,number);
    Json::Value root(Json::arrayValue);
    for (int y=0;y<=divisions;++y) for(int x=0;x<=divisions;++x) {
        QPointF q=surface(g,x/double(divisions),y/double(divisions));
        Json::Value point(Json::arrayValue); point.append(q.x()); point.append(q.y());
        root.append(point);
    }
    return root.toStyledString();
}
std::shared_ptr<Frame> Distort::GetFrame(std::shared_ptr<Frame> frame, int64_t number) {
    Grid g=controls(*this,number);
    bool identity=true;
    for(int i=0;i<16;++i)
        if ((g[i]-QPointF((i%4)/3.0,(i/4)/3.0)).manhattanLength()>1e-12) identity=false;
    if(identity) return frame;
#ifdef OPENSHOT_ENABLE_GPU_FRAME
    if (VulkanRenderingEnabled()) {
        try {
            auto source = frame->GetDeferredImage();
            if (!source && ParentClip() && frame->has_image_data)
                source = UploadVulkanImage(*frame->GetImage());
            if (source) {
                std::vector<QPointF> mesh;
                mesh.reserve(37 * 37);
                for (int y = 0; y <= 36; ++y)
                    for (int x = 0; x <= 36; ++x)
                        mesh.push_back(surface(g, x / 36.0, y / 36.0));
                frame->AddDeferredImage(DistortVulkanImage(source, mesh, 36));
                return frame;
            }
        } catch (const std::exception& error) {
            Logger::Instance()->Log(std::string("Vulkan distort CPU fallback: ") + error.what());
        }
    }
#endif
    auto image=frame->GetImage();
    if(!image || image->isNull()) return frame;
    // Interpolate premultiplied colors to avoid dark fringes on transparent logos.
    QImage source=image->convertToFormat(QImage::Format_RGBA8888_Premultiplied);
    int w=source.width(), h=source.height();
    QImage output(w,h,source.format()); output.fill(Qt::transparent);
    constexpr int n=36;
    std::vector<Vertex> vertices;
    for(int y=0;y<=n;++y) for(int x=0;x<=n;++x) {
        QPointF uv(x/double(n),y/double(n)), p=surface(g,uv.x(),uv.y());
        vertices.push_back({QPointF(p.x()*w,p.y()*h),uv});
    }
    std::vector<Triangle> triangles;
    std::vector<std::vector<int>> rows(h);
    auto add=[&](Vertex a,Vertex b,Vertex c) {
        double det=cross(b.p-a.p,c.p-a.p);
        if(std::abs(det)<1e-8) return;
        int left=std::max(0,int(std::ceil(std::min({a.p.x(),b.p.x(),c.p.x()})-0.5)));
        int right=std::min(w-1,int(std::floor(std::max({a.p.x(),b.p.x(),c.p.x()})-0.5)));
        int top=std::max(0,int(std::ceil(std::min({a.p.y(),b.p.y(),c.p.y()})-0.5)));
        int bottom=std::min(h-1,int(std::floor(std::max({a.p.y(),b.p.y(),c.p.y()})-0.5)));
        if(left>right || top>bottom) return;
        int index=triangles.size(); triangles.push_back({a,b,c,det,left,right,top,bottom});
        for(int y=top;y<=bottom;++y) rows[y].push_back(index);
    };
    for(int y=0;y<n;++y) for(int x=0;x<n;++x) {
        int i=y*(n+1)+x;
        add(vertices[i],vertices[i+1],vertices[i+n+2]);
        add(vertices[i],vertices[i+n+2],vertices[i+n+1]);
    }
    unsigned char* dest=output.bits(); const unsigned char* src=source.constBits();
    int ds=output.bytesPerLine(), ss=source.bytesPerLine();
    // Each worker owns complete destination rows; overlapping folds have stable order.
    #pragma omp parallel for
    for(int y=0;y<h;++y) for(int index:rows[y]) {
        const Triangle& t=triangles[index];
        const auto span=mesh_warp::Span(t.a.p,t.b.p,t.c.p,y,t.left,t.right);
        for(int x=span.first;x<=span.second;++x) {
            QPointF p(x+0.5,y+0.5), d=p-t.a.p;
            double b=cross(d,t.c.p-t.a.p)/t.det, c=cross(t.b.p-t.a.p,d)/t.det;
            QPointF uv=t.a.uv+(t.b.uv-t.a.uv)*b+(t.c.uv-t.a.uv)*c;
            double sx=std::max(0.0,std::min(w-1.0,uv.x()*w-0.5));
            double sy=std::max(0.0,std::min(h-1.0,uv.y()*h-0.5));
            int x0=int(sx), y0=int(sy), x1=std::min(w-1,x0+1), y1=std::min(h-1,y0+1);
            double fx=sx-x0, fy=sy-y0;
            for(int channel=0;channel<4;++channel) {
                double upper=src[y0*ss+x0*4+channel]*(1-fx)+src[y0*ss+x1*4+channel]*fx;
                double lower=src[y1*ss+x0*4+channel]*(1-fx)+src[y1*ss+x1*4+channel]*fx;
                dest[y*ds+x*4+channel]=static_cast<unsigned char>(std::lround(upper*(1-fy)+lower*fy));
            }
        }
    }
    *image=output.convertToFormat(image->format());
    return frame;
}
std::string Distort::Json() const { return JsonValue().toStyledString(); }
Json::Value Distort::JsonValue() const {
    Json::Value root=EffectBase::JsonValue(); root["type"]=info.class_name;
    root["preset"]=preset; root["selected_point"]=selected_point; root["strength"]=strength.JsonValue();
    for(int i=0;i<16;++i) {
        root["point_"+std::to_string(i)+"_x"]=points_x[i].JsonValue();
        root["point_"+std::to_string(i)+"_y"]=points_y[i].JsonValue();
    }
    return root;
}
void Distort::SetJson(const std::string value) {
    try { SetJsonValue(openshot::stringToJson(value)); }
    catch(const std::exception&) { throw InvalidJSON("Invalid Distort JSON"); }
}
void Distort::SetJsonValue(const Json::Value root) {
    EffectBase::SetJsonValue(root);
    if(!root["preset"].isNull()) preset=std::max(0,std::min(6,root["preset"].asInt()));
    if(!root["selected_point"].isNull()) selected_point=std::max(0,std::min(15,root["selected_point"].asInt()));
    if(!root["strength"].isNull()) strength.SetJsonValue(root["strength"]);
    for(int i=0;i<16;++i) {
        std::string key="point_"+std::to_string(i);
        if(!root[key+"_x"].isNull()) points_x[i].SetJsonValue(root[key+"_x"]);
        if(!root[key+"_y"].isNull()) points_y[i].SetJsonValue(root[key+"_y"]);
    }
}
std::string Distort::PropertiesJSON(int64_t number) const {
    Json::Value root=BasePropertiesJSON(number);
    root["preset"]=add_property_json("Preset",preset,"int","",nullptr,0,6,false,number);
    const char* names[]={"Custom","Bend","Flag","Bulge","Pinch","Taper","Squeeze"};
    for(int i=0;i<7;++i) root["preset"]["choices"].append(add_property_choice_json(names[i],i,preset));
    root["strength"]=add_property_json("Strength",strength.GetValue(number),"float","",&strength,-2,2,false,number);
    root["selected_point"]=add_property_json("Selected Point",selected_point,"int","",nullptr,0,15,false,number);
    for(int i=0;i<16;++i) {
        std::string label="Row "+std::to_string(i/4+1)+", Column "+std::to_string(i%4+1);
        root["selected_point"]["choices"].append(add_property_choice_json(label,i,selected_point));
    }
    int i=std::max(0,std::min(15,selected_point));
    root["point_"+std::to_string(i)+"_x"]=add_property_json("Point X Offset",points_x[i].GetValue(number),"float","",&points_x[i],-1,1,false,number);
    root["point_"+std::to_string(i)+"_y"]=add_property_json("Point Y Offset",points_y[i].GetValue(number),"float","",&points_y[i],-1,1,false,number);
    return root.toStyledString();
}

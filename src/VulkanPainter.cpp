// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "VulkanPainter.h"
#include "VulkanCompositor.h"
#include <QPaintEngine>
#include <QPainter>
#include <QPainterPathStroker>
#include <QGradient>
#include <array>
#include <algorithm>
#include <iterator>
#include <cstring>
#include <cmath>
#include <stdexcept>
#include <functional>
#include <climits>

namespace openshot {
namespace {
uint32_t Word(float f) { uint32_t word;std::memcpy(&word,&f,4);return word; }
}
struct VulkanPaintDevice::Engine : QPaintEngine {
    int width,height;
    QPen pen;
    QBrush brush;
    bool aa=false,polyline=false;
    struct Command { std::array<uint32_t,24> words{};QRect bounds; };
    std::vector<Command> commands;
    std::vector<uint32_t> extra;
    Engine(int w,int h):QPaintEngine(PainterPaths|Antialiasing|LinearGradientFill|RadialGradientFill|ConicalGradientFill|AlphaBlend|BrushStroke),width(w),height(h) {}
    bool begin(QPaintDevice*) override { setActive(true);return true; }
    bool end() override { setActive(false);return true; }
    Type type() const override { return User; }
    void updateState(const QPaintEngineState& s) override {
        if(s.state()&DirtyPen) pen=s.pen();
        if(s.state()&DirtyBrush) brush=s.brush();
        if(s.state()&DirtyHints) aa=s.renderHints()&QPainter::Antialiasing;
        if((s.state()&DirtyTransform) && !s.transform().isIdentity()) throw std::runtime_error("Unsupported Vulkan painter transform");
        if((s.state()&DirtyCompositionMode) && s.compositionMode()!=QPainter::CompositionMode_SourceOver) throw std::runtime_error("Unsupported Vulkan painter composition");
    }
    std::vector<uint32_t> aliasedEdges(const QPainterPath& path) {
        std::vector<uint32_t> lines;
        auto line=[&](QPoint a,QPoint b) {
            int direction=1;if(a.y()>b.y()){std::swap(a,b);direction=-1;}
            int top=std::max(0,(a.y()+32)>>6),bottom=std::min(height-1,(b.y()-32)>>6);
            if(top>bottom || a.y()==b.y())return;
            int slope=int(double(b.x()-a.x())/(b.y()-a.y())*65536.0);
            int start=32768+a.x()*1024+int((int64_t(slope)*(int64_t(top)*65536+32768-int64_t(a.y())*1024))>>16);
            for(int word:{top,bottom,start,slope,direction})lines.push_back(uint32_t(word));
        };
        const auto edges=VulkanPathEdges(path,width,height);
        for(size_t i=0;i<edges.size();i+=4) line(QPoint(qRound(edges[i]*64),qRound(edges[i+1]*64)),QPoint(qRound(edges[i+2]*64),qRound(edges[i+3]*64)));
        return lines;
    }

    void add(const QPainterPath& path,const QBrush& fill,const QRectF* rect=nullptr) {
        if(fill.style()==Qt::NoBrush || path.isEmpty()) return;
        Command c;auto& w=c.words;w[0]=rect?1:0;w[1]=aa;w[4]=path.fillRule()==Qt::OddEvenFill;
        if(!rect && !path.controlPointRect().toRect().intersects(QRect(0,0,width,height))) return;
        c.bounds=path.boundingRect().adjusted(-1,-1,1,1).toAlignedRect().intersected(QRect(0,0,width,height));
        w[8]=Word(c.bounds.left());w[9]=Word(c.bounds.top());w[10]=Word(c.bounds.right()+1);w[11]=Word(c.bounds.bottom()+1);
        if(rect) { w[18]=Word(rect->left());w[19]=Word(rect->top());w[20]=Word(rect->right());w[21]=Word(rect->bottom()); }
        else {
            w[2]=extra.size();
            if(!aa) {auto lines=aliasedEdges(path);w[3]=lines.size()/5;extra.insert(extra.end(),lines.begin(),lines.end());}
            else {auto edges=VulkanPathEdges(path,width,height);w[3]=edges.size()/4;std::transform(edges.begin(),edges.end(),std::back_inserter(extra),Word);}

        }
        if(const auto* gradient=fill.gradient()) {
            w[6]=extra.size();auto stops=gradient->stops();w[7]=stops.size();
            for(const auto& stop:stops) {
                const auto color=stop.second.rgba64().premultiplied();
                for(float f:{float(stop.first),color.red()/257.f,color.green()/257.f,color.blue()/257.f,color.alpha()/257.f}) extra.push_back(Word(f));
            }
            w[16]=gradient->spread();
            if(gradient->type()==QGradient::LinearGradient) {
                auto* g=static_cast<const QLinearGradient*>(gradient);w[5]=1;
                w[12]=Word(g->start().x());w[13]=Word(g->start().y());w[14]=Word(g->finalStop().x());w[15]=Word(g->finalStop().y());
            } else if(gradient->type()==QGradient::RadialGradient) {
                auto* g=static_cast<const QRadialGradient*>(gradient);w[5]=2;
                w[12]=Word(g->center().x());w[13]=Word(g->center().y());w[14]=Word(g->radius());
            } else {
                auto* g=static_cast<const QConicalGradient*>(gradient);w[5]=3;
                w[12]=Word(g->center().x());w[13]=Word(g->center().y());w[14]=Word(g->angle());
            }
        } else {
            const QRgb color=qPremultiply(fill.color().rgba());
            w[17]=uint32_t(qRed(color))|(uint32_t(qGreen(color))<<8)|(uint32_t(qBlue(color))<<16)|(uint32_t(qAlpha(color))<<24);
        }
        if(w[0]==0 && !c.bounds.isEmpty()) {
            // Index vector edges by scanline. Dense waveform paths should not
            // visit thousands of unrelated edges for every output pixel.
            std::vector<std::vector<uint32_t>> rows(c.bounds.height());
            for(uint32_t edge=0;edge<w[3];++edge) {
                const auto base=w[2]+(aa?4:5)*edge;int top,bottom;
                if(aa) {float y1,y2;std::memcpy(&y1,&extra[base+1],4);std::memcpy(&y2,&extra[base+3],4);top=int(std::floor(std::min(y1,y2)));bottom=int(std::ceil(std::max(y1,y2)))-1;}
                else {top=int(extra[base]);bottom=int(extra[base+1]);}
                top=std::max(top,c.bounds.top());bottom=std::min(bottom,c.bounds.bottom());
                for(int y=top;y<=bottom;++y)rows[y-c.bounds.top()].push_back(edge);
            }
            w[22]=extra.size();w[23]=c.bounds.top();extra.resize(extra.size()+2*rows.size());
            for(size_t row=0;row<rows.size();++row){extra[w[22]+2*row]=extra.size()-w[22];extra[w[22]+2*row+1]=rows[row].size();extra.insert(extra.end(),rows[row].begin(),rows[row].end());}
        }
        commands.push_back(c);
    }
    void cosmetic(const QPainterPath& path) {
        // Match Qt's cosmetic stroker using compact fixed-point line records.
        // This generates geometry only; pixel coverage is evaluated on the GPU.
        struct Segment {QPointF a,b;};std::vector<Segment> segments;
        std::function<void(QPointF,QPointF,QPointF,QPointF,int)> cubic;
        cubic=[&](QPointF a,QPointF b,QPointF c,QPointF d,int level) {
            double dx=a.x()-d.x(),dy=a.y()-d.y(),limit=.25*(std::abs(dx)+std::abs(dy));
            if(level && (std::abs(dx*(d.y()-b.y())-dy*(d.x()-b.x()))>=limit || std::abs(dx*(d.y()-c.y())-dy*(d.x()-c.x()))>=limit)) {
                auto ab=(a+b)*.5,bc=(b+c)*.5,cd=(c+d)*.5,abc=(ab+bc)*.5,bcd=(bc+cd)*.5,center=(abc+bcd)*.5;
                cubic(a,ab,abc,center,level-1);cubic(center,bcd,cd,d,level-1);
            } else segments.push_back({a,d});
        };
        QPointF previous;
        for(int i=0;i<path.elementCount();++i) {
            auto e=path.elementAt(i);QPointF p(e.x,e.y);
            if(e.isMoveTo()) previous=p;
            else if(e.isLineTo()) {segments.push_back({previous,p});previous=p;}
            else if(e.type==QPainterPath::CurveToElement) {auto c=path.elementAt(++i),d=path.elementAt(++i);cubic(previous,p,QPointF(c.x,c.y),QPointF(d.x,d.y),6);previous=QPointF(d.x,d.y);}
        }
        if(segments.empty())return;
        QPainterPathStroker stroker;stroker.setWidth(2);stroker.setCapStyle(Qt::SquareCap);
        const auto before=commands.size();add(stroker.createStroke(path),pen.brush());if(commands.size()==before)return;
        auto& w=commands.back().words;w[0]=2;w[2]=extra.size();w[3]=0;
        const bool closed=segments.front().a==segments.back().b,draw_caps=pen.capStyle()!=Qt::FlatCap;
        int last_dir=0;bool last_axis=false;QPoint last(INT_MIN,INT_MIN);
        for(size_t i=0;i<segments.size();++i) {
            auto a=segments[i].a,b=segments[i].b;
            auto clip=[](double& a,double& b,double& c,double& d,double low,double high) {
                if(a<low){if(b<=low)return false;c+=(d-c)/(b-a)*(low-a);a=low;}
                else if(a>high){if(b>=high)return false;c+=(d-c)/(b-a)*(high-a);a=high;}
                if(b<low){d+=(d-c)/(b-a)*(low-b);b=low;}
                else if(b>high){d+=(d-c)/(b-a)*(high-b);b=high;}return true;
            };
            double ax=a.x(),ay=a.y(),bx=b.x(),by=b.y();
            if(!clip(ax,bx,ay,by,-1,width+1)||!clip(ay,by,ax,bx,-1,height+1))continue;
            int x1=int(ax*64),y1=int(ay*64),x2=int(bx*64),y2=int(by*64);
            bool vertical=std::abs(x2-x1)<std::abs(y2-y1);
            if(vertical){std::swap(x1,y1);std::swap(x2,y2);}
            if(x1==x2){if(polyline && !aa && i+1<segments.size() && i>0 && std::abs(a.x()-b.x())+std::abs(a.y()-b.y())<1)segments[i+1].a=a;continue;}bool swapped=x1>x2;
            int caps=(!closed && draw_caps)?((i==0?1:0)|(i+1==segments.size()?2:0)):0;
            if(swapped){std::swap(x1,x2);std::swap(y1,y2);caps=((caps&1)<<1)|((caps&2)>>1);}
            int increment=int(int64_t(y2-y1)*65536/(x2-x1)),minor,begin,end,first=64,final=0;
            if(aa) {
                minor=(y1-32)*1024-int((int64_t((x1&63)-32)*increment)>>6);
                if(caps&1){x1-=32;minor-=increment>>1;}if(caps&2)x2+=32;
                begin=x1>>6;end=x2>>6;first=begin==end?x2-x1:64-(x1&63);final=begin==end?0:x2&63;
            } else {
                int dir=vertical?(swapped?2:1):(swapped?8:4);
                if((last_dir^(vertical?3:12))==dir)caps|=swapped?2:1;
                minor=y1*1024;if(caps&1){x1-=32;minor-=increment>>1;}if(caps&2)x2+=32;
                begin=(x1+32)>>6;end=(x2+32)>>6;
                const int last_major=vertical?last.y():last.x();if((caps&1)&&last_major==begin+1)++begin;
                if(begin==end){if(polyline && i+1<segments.size() && i>0 && std::abs(a.x()-b.x())+std::abs(a.y()-b.y())<1)segments[i+1].a=a;continue;}
                minor+=int((int64_t(begin*64+(increment>0?32:0)-x1)*increment)>>6);
                auto point=[&](int major,int minor){return vertical?QPoint(minor,major):QPoint(major,minor);};
                QPoint first_point=point(begin,minor>>16),last_point=point(end-1,(minor+(end-begin-1)*increment)>>16);
                if(swapped)std::swap(first_point,last_point);
                const bool axis=std::abs(increment)<(1<<14);
                if(last.x()!=INT_MIN) {
                    if(first_point==last){if(swapped)--end;else{++begin;minor+=increment;}}
                    else if(last_dir!=dir && ((axis&&last_axis&&last.x()!=first_point.x()&&last.y()!=first_point.y())||std::abs(last.x()-first_point.x())>1||std::abs(last.y()-first_point.y())>1)) {
                        if(swapped)++end;else{--begin;minor-=increment;}
                    } else if(last_dir==dir && std::abs(last.x()-first_point.x())<=1 && std::abs(last.y()-first_point.y())>1) {
                        minor+=increment>>1;last_point=point(swapped?begin:end-1,(minor+(swapped?0:end-begin-1)*increment)>>16);
                    }
                }
                last=last_point;last_dir=dir;last_axis=axis;
                // Qt's dropout correction draws one pixel even when trimming
                // the duplicated endpoint empties the original interval.
                end=std::max(begin,end-1);
            }
            int opacity=pen.widthF()==0?256:std::clamp(int(256*pen.widthF()),0,256);
            for(int word:{int(vertical),begin,end,minor,increment,first,final,opacity})extra.push_back(uint32_t(word));
            ++w[3];
        }
    }
    void drawPath(const QPainterPath& path) override {
        add(path,brush);
        if(pen.style()!=Qt::NoPen) {
            if(pen.widthF()<=1) {cosmetic(path);return;}
            QPainterPathStroker stroker;stroker.setWidth(pen.widthF()==0?1:pen.widthF());
            stroker.setJoinStyle(pen.joinStyle());stroker.setCapStyle(pen.capStyle());stroker.setMiterLimit(pen.miterLimit());
            add(stroker.createStroke(path),pen.brush());
        }
    }
    void drawRects(const QRectF* rects,int count) override {
        for(int i=0;i<count;++i) { QPainterPath path;auto rect=rects[i].normalized();path.addRect(rect);add(path,brush,&rect);
            if(pen.style()!=Qt::NoPen) { auto saved=brush;brush=Qt::NoBrush;drawPath(path);brush=saved; } }
    }
    void drawRects(const QRect* rects,int count) override { for(int i=0;i<count;++i) { QRectF r(rects[i]);drawRects(&r,1); } }
    void drawEllipse(const QRectF& rect) override { QPainterPath path;path.addEllipse(rect);drawPath(path); }
    void drawPolygon(const QPointF* points,int count,PolygonDrawMode mode) override {
        if(!count) return;QPainterPath path;path.moveTo(points[0]);for(int i=1;i<count;++i) path.lineTo(points[i]);
        if(mode!=PolylineMode) {path.closeSubpath();path.setFillRule(mode==OddEvenMode?Qt::OddEvenFill:Qt::WindingFill);}
        if(mode==PolylineMode) {auto saved=brush;brush=Qt::NoBrush;polyline=true;drawPath(path);polyline=false;brush=saved;} else drawPath(path);
    }
    void drawLines(const QLineF* lines,int count) override {
        auto saved=brush;brush=Qt::NoBrush;
        for(int i=0;i<count;++i) { QPainterPath path;path.moveTo(lines[i].p1());path.lineTo(lines[i].p2());drawPath(path); }
        brush=saved;
    }
    void drawPixmap(const QRectF&,const QPixmap&,const QRectF&) override { throw std::runtime_error("Vulkan vector painter cannot upload pixmaps"); }
    void drawImage(const QRectF&,const QImage&,const QRectF&,Qt::ImageConversionFlags) override { throw std::runtime_error("Vulkan vector painter cannot upload images"); }
    std::shared_ptr<const FrameImage> render(std::shared_ptr<const FrameImage> source) {
        if(commands.empty()) return source;
        const int tx=(width+15)/16,ty=(height+15)/16;std::vector<std::vector<uint32_t>> tiles(tx*ty);
        for(size_t i=0;i<commands.size();++i) {
            const auto b=commands[i].bounds;if(b.isEmpty()) continue;
            for(int y=b.top()/16;y<=b.bottom()/16;++y) for(int x=b.left()/16;x<=b.right()/16;++x) tiles[y*tx+x].push_back(i);
        }
        std::vector<uint32_t> data(4+2*tiles.size()+24*commands.size());data[0]=tx;data[1]=ty;
        const size_t base=4+2*tiles.size();
        for(size_t i=0;i<tiles.size();++i) { data[4+2*i]=data.size();data[5+2*i]=tiles[i].size();data.insert(data.end(),tiles[i].begin(),tiles[i].end()); }
        const auto offset=data.size();data.insert(data.end(),extra.begin(),extra.end());
        for(size_t i=0;i<commands.size();++i) {auto w=commands[i].words;w[2]+=offset;w[6]+=offset;if(w[0]==0)w[22]+=offset;std::copy(w.begin(),w.end(),data.begin()+base+24*i);}
        return PaintVulkanCommands(source,data);
    }
};
VulkanPaintDevice::VulkanPaintDevice(int w,int h):engine(new Engine(w,h)),width(w),height(h) {}
VulkanPaintDevice::~VulkanPaintDevice()=default;
QPaintEngine* VulkanPaintDevice::paintEngine() const { return engine.get(); }
int VulkanPaintDevice::metric(PaintDeviceMetric m) const {
    switch(m) {case PdmWidth:return width;case PdmHeight:return height;case PdmWidthMM:return width*254/960;case PdmHeightMM:return height*254/960;
    case PdmDpiX:case PdmDpiY:case PdmPhysicalDpiX:case PdmPhysicalDpiY:return 96;case PdmDepth:return 32;case PdmDevicePixelRatio:return 1;case PdmDevicePixelRatioScaled:return 65536;default:return 0;}
}
std::shared_ptr<const FrameImage> VulkanPaintDevice::Render(std::shared_ptr<const FrameImage> source) {return engine->render(source);}
}

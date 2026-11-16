// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "effects/Distort.h"
#include "EffectInfo.h"
#include "openshot_catch.h"
#include <QColor>
#include <memory>
#include <algorithm>
using namespace openshot;
namespace {
std::shared_ptr<Frame> input(int w=48,int h=36) {
    auto frame=std::make_shared<Frame>();
    QImage image(w,h,QImage::Format_RGBA8888);
    for(int y=0;y<h;++y) for(int x=0;x<w;++x)
        image.setPixelColor(x,y,QColor((x*5)%256,(y*7)%256,170,255));
    *frame->GetImage()=image; return frame;
}
}
TEST_CASE("Distort identity and zero strength preserve pixels", "[effect][distort]") {
    Distort effect;
    auto frame=input(); QImage original=frame->GetImage()->copy();
    CHECK(*effect.GetFrame(frame,1)->GetImage()==original);
    effect.preset=3; effect.points_x[5]=Keyframe(0.25); effect.strength=Keyframe(0.0);
    CHECK(*effect.GetFrame(frame,1)->GetImage()==original);
}
TEST_CASE("Distort translates forward and leaves exposed pixels transparent", "[effect][distort]") {
    Distort effect;
    std::fill(effect.points_x.begin(), effect.points_x.end(), Keyframe(0.25));
    auto frame=input(); QImage original=frame->GetImage()->copy();
    QImage result=*effect.GetFrame(frame,1)->GetImage();
    CHECK(result.pixelColor(0,18).alpha()==0);
    CHECK(result.pixelColor(11,18).alpha()==0);
    for(int y=0;y<36;++y) for(int x=12;x<48;++x)
        CHECK(result.pixelColor(x,y)==original.pixelColor(x-12,y));
}
TEST_CASE("Distort preserves translucent color when sampling", "[effect][distort]") {
    auto frame=input(); frame->GetImage()->fill(QColor(255,0,0,128));
    Distort effect;
    std::fill(effect.points_x.begin(), effect.points_x.end(), Keyframe(0.01));
    auto result=effect.GetFrame(frame,1)->GetImage();
    CHECK(result->pixelColor(24,18)==QColor(255,0,0,128));
}
TEST_CASE("Distort control handles lie on its smooth surface", "[effect][distort]") {
    Distort effect; effect.points_x[5]=Keyframe(0.2); effect.points_y[5]=Keyframe(-0.1);
    auto mesh=stringToJson(effect.MeshJSON(1,36));
    CHECK(mesh[12*37+12][0].asDouble()==Approx(1.0/3+0.2));
    CHECK(mesh[12*37+12][1].asDouble()==Approx(1.0/3-0.1));
    CHECK(mesh[0][0].asDouble()==Approx(0));
    CHECK(mesh[36*37+36][1].asDouble()==Approx(1));
}
TEST_CASE("Distort saves animation and factory metadata", "[effect][distort]") {
    Distort effect; effect.preset=2; effect.selected_point=10;
    effect.points_x[10].AddPoint(20,0.2,LINEAR);
    effect.strength.AddPoint(20,0.5,LINEAR);
    Distort restored; restored.SetJson(effect.Json());
    CHECK(restored.JsonValue()==effect.JsonValue());
    CHECK(restored.MeshJSON(10)==effect.MeshJSON(10));
    CHECK(restored.MeshJSON(1)!=restored.MeshJSON(20));
    auto props=stringToJson(restored.PropertiesJSON(10));
    CHECK(props.isMember("point_10_x")); CHECK_FALSE(props.isMember("point_5_x"));
    std::unique_ptr<EffectBase> created(EffectInfo().CreateEffect("Distort"));
    REQUIRE(created); CHECK(created->JsonValue()["type"].asString()=="Distort");
}
TEST_CASE("Distort presets render tiny and folded frames deterministically", "[effect][distort]") {
    for(int preset=1;preset<=6;++preset) {
        Distort effect; effect.preset=preset;
        CHECK(effect.MeshJSON(1)!=Distort().MeshJSON(1));
        for(int size:{1,2,20}) {
            auto a=effect.GetFrame(input(size,size),1)->GetImage()->copy();
            auto b=effect.GetFrame(input(size,size),1)->GetImage()->copy();
            CHECK(a==b); CHECK(a.width()==size); CHECK(a.height()==size);
        }
    }
    Distort folded; folded.points_x[5]=Keyframe(1.0); folded.points_y[6]=Keyframe(-1.0);
    CHECK(*folded.GetFrame(input(),1)->GetImage()==*folded.GetFrame(input(),1)->GetImage());
}

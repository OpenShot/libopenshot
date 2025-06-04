/**
* @file
 * @brief Source file for Godot wrapper
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2025 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "osg_timeline.h"
#include "FFmpegReader.h"
#include "Profiles.h"
#include "Timeline.h"

#include "godot_cpp/classes/image.hpp"
#include "godot_cpp/classes/image_texture.hpp"
#include "godot_cpp/variant/utility_functions.hpp"
#include "godot_cpp/core/class_db.hpp"

using namespace godot;

void ExampleClass::_bind_methods() {
    ClassDB::bind_method(D_METHOD("load_file", "path"), &ExampleClass::load_file);
    ClassDB::bind_method(D_METHOD("print_type", "variant"), &ExampleClass::print_type);
    ClassDB::bind_method(D_METHOD("print_json"), &ExampleClass::print_json);
    ClassDB::bind_method(D_METHOD("get_image", "frame_number"), &ExampleClass::get_image);
}

ExampleClass::ExampleClass() {
    constructor_called = true;
    print_line("Constructor called!");

    // Create example timeline
    timeline = new openshot::Timeline(
         1920, 1080,
         openshot::Fraction(30, 1),
         44100, 2,
         openshot::LAYOUT_STEREO);

    print_line("Timeline instantiated!");
}

ExampleClass::~ExampleClass() {
    print_line("Destructor called!");
    delete timeline;
    timeline = nullptr;
    delete reader;
    reader = nullptr;
}

void ExampleClass::load_file(const String path) {
    if (reader == nullptr)
    {
        // Create example reader
        reader = new openshot::FFmpegReader(path.utf8().get_data(), true);
        reader->Open();
    }
}

void ExampleClass::print_type(const Variant &p_variant) const {
    print_line(vformat("Type: %d", p_variant.get_type()));
}

void ExampleClass::print_json(const Variant &p_variant) {
    print_line("print_json!");
    openshot::Profile p("/home/jonathan/apps/openshot-qt/src/profiles/01920x1080p2997_16-09");
    std::string s = timeline->Json();
    String output = "OpenShot Profile JSON: " + String(s.c_str());
    UtilityFunctions::print(output);
}

Ref<Image> ExampleClass::get_image(const int64_t frame_number) {
    if (reader && reader->IsOpen())
    {
        // Load video frame
        auto frame = reader->GetFrame(frame_number);
        std::shared_ptr<QImage> qimg = frame->GetImage();

        // Convert ARGB32_Premultiplied to RGBA8888, keeping premultiplied alpha
        QImage rgba_image = qimg->convertToFormat(QImage::Format_RGBA8888);

        // Copy pixel data
        int width = rgba_image.width();
        int height = rgba_image.height();
        PackedByteArray buffer;
        buffer.resize(width * height * 4);
        memcpy(buffer.ptrw(), rgba_image.constBits(), buffer.size());

        // Create Godot Image
        Ref<Image> image = Image::create(width, height, false, Image::FORMAT_RGBA8);
        image->set_data(width, height, false, Image::FORMAT_RGBA8, buffer);

        print_line(vformat("✅ Image created: %dx%d (premultiplied alpha)", width, height));
        return image;
    }

    // Empty image
    return Ref<Image>();
}



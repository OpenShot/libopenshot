/**
 * @file
 * @brief Benchmark executable for core libopenshot operations
 * @author Jonathan Thomas <jonathan@openshot.org>
 * @ref License
 */
// Copyright (c) 2025 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <chrono>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include "Clip.h"
#include "FFmpegReader.h"
#include "FFmpegWriter.h"
#include "Fraction.h"
#include "FrameMapper.h"
#ifdef USE_IMAGEMAGICK
#include "ImageReader.h"
#else
#include "QtImageReader.h"
#endif
#include "ReaderBase.h"
#include "Timeline.h"
#include "effects/Brightness.h"
#include "effects/Crop.h"
#include "effects/Mask.h"
#include "effects/Saturation.h"

using namespace openshot;
using namespace std;

using Clock = chrono::steady_clock;

template <typename Func> double time_trial(const string &name, Func func) {
	auto start = Clock::now();
	func();
	auto elapsed =
			chrono::duration_cast<chrono::milliseconds>(Clock::now() - start).count();
	cout << name << "," << elapsed << "\n";
	return static_cast<double>(elapsed);
}

void read_forward_backward(ReaderBase &reader) {
	int64_t len = reader.info.video_length;
	for (int64_t i = 1; i <= len; ++i)
		reader.GetFrame(i);
	for (int64_t i = len; i >= 1; --i)
		reader.GetFrame(i);
}

int main() {
	cout << "Trial,Milliseconds\n";
	double total = 0.0;
	const string base = TEST_MEDIA_PATH;
	const string video = base + "sintel_trailer-720p.mp4";
	const string mask_img = base + "mask.png";
	const string overlay = base + "front3.png";

	total += time_trial("FFmpegReader", [&]() {
		FFmpegReader r(video);
		r.Open();
		read_forward_backward(r);
		r.Close();
	});

	total += time_trial("FFmpegWriter", [&]() {
		FFmpegReader r(video);
		r.Open();
		FFmpegWriter w("benchmark_output.mp4");
		w.SetAudioOptions("aac", r.info.sample_rate, 192000);
		w.SetVideoOptions("libx264", r.info.width, r.info.height, r.info.fps,
											5000000);
		w.Open();
		for (int64_t i = 1; i <= r.info.video_length; ++i)
			w.WriteFrame(r.GetFrame(i));
		w.Close();
		r.Close();
	});

	total += time_trial("FrameMapper", [&]() {
		vector<Fraction> rates = {Fraction(24, 1), Fraction(30, 1), Fraction(60, 1),
															Fraction(30000, 1001), Fraction(60000, 1001)};
		for (auto &fps : rates) {
			FFmpegReader r(video);
			r.Open();
			FrameMapper map(&r, fps, PULLDOWN_NONE, r.info.sample_rate,
											r.info.channels, r.info.channel_layout);
			map.Open();
			for (int64_t i = 1; i <= map.info.video_length; ++i)
				map.GetFrame(i);
			map.Close();
			r.Close();
		}
	});

	total += time_trial("Clip", [&]() {
		Clip c(video);
		c.Open();
		read_forward_backward(c);
		c.Close();
	});

	total += time_trial("Timeline", [&]() {
		Timeline t(1920, 1080, Fraction(24, 1), 44100, 2, LAYOUT_STEREO);
		Clip video_clip(video);
		video_clip.Layer(0);
		video_clip.Start(0.0);
		video_clip.End(video_clip.Reader()->info.duration);
		video_clip.Open();
		Clip overlay1(overlay);
		overlay1.Layer(1);
		overlay1.Start(0.0);
		overlay1.End(video_clip.Reader()->info.duration);
		overlay1.Open();
		Clip overlay2(overlay);
		overlay2.Layer(2);
		overlay2.Start(0.0);
		overlay2.End(video_clip.Reader()->info.duration);
		overlay2.Open();
		t.AddClip(&video_clip);
		t.AddClip(&overlay1);
		t.AddClip(&overlay2);
		t.Open();
		t.info.video_length = t.GetMaxFrame();
		read_forward_backward(t);
		t.Close();
	});

	total += time_trial("Timeline (with transforms)", [&]() {
		Timeline t(1920, 1080, Fraction(24, 1), 44100, 2, LAYOUT_STEREO);
		Clip video_clip(video);
		int64_t last = video_clip.Reader()->info.video_length;
		video_clip.Layer(0);
		video_clip.Start(0.0);
		video_clip.End(video_clip.Reader()->info.duration);
		video_clip.alpha.AddPoint(1, 1.0);
        video_clip.alpha.AddPoint(last, 0.0);
		video_clip.Open();
		Clip overlay1(overlay);
		overlay1.Layer(1);
		overlay1.Start(0.0);
		overlay1.End(video_clip.Reader()->info.duration);
		overlay1.Open();
		overlay1.scale_x.AddPoint(1, 1.0);
		overlay1.scale_x.AddPoint(last, 0.25);
		overlay1.scale_y.AddPoint(1, 1.0);
		overlay1.scale_y.AddPoint(last, 0.25);
		Clip overlay2(overlay);
		overlay2.Layer(2);
		overlay2.Start(0.0);
		overlay2.End(video_clip.Reader()->info.duration);
		overlay2.Open();
		overlay2.rotation.AddPoint(1, 90.0);
		t.AddClip(&video_clip);
		t.AddClip(&overlay1);
		t.AddClip(&overlay2);
		t.Open();
		t.info.video_length = t.GetMaxFrame();
		read_forward_backward(t);
		t.Close();
	});

        total += time_trial("Effect_Mask", [&]() {
                FFmpegReader r(video);
                r.Open();
#ifdef USE_IMAGEMAGICK
                ImageReader mask_reader(mask_img);
#else
                QtImageReader mask_reader(mask_img);
#endif
                mask_reader.Open();
                Clip clip(&r);
                clip.Open();
                Mask m(&mask_reader, Keyframe(0.0), Keyframe(0.5));
                clip.AddEffect(&m);
                read_forward_backward(clip);
                mask_reader.Close();
                clip.Close();
                r.Close();
        });

	total += time_trial("Effect_Brightness", [&]() {
		FFmpegReader r(video);
		r.Open();
		Clip clip(&r);
		clip.Open();
		Brightness b(Keyframe(0.5), Keyframe(1.0));
		clip.AddEffect(&b);
		read_forward_backward(clip);
		clip.Close();
		r.Close();
	});

	total += time_trial("Effect_Crop", [&]() {
		FFmpegReader r(video);
		r.Open();
		Clip clip(&r);
		clip.Open();
		Crop c(Keyframe(0.25), Keyframe(0.25), Keyframe(0.25), Keyframe(0.25));
		clip.AddEffect(&c);
		read_forward_backward(clip);
		clip.Close();
		r.Close();
	});

	total += time_trial("Effect_Saturation", [&]() {
		FFmpegReader r(video);
		r.Open();
		Clip clip(&r);
		clip.Open();
		Saturation s(Keyframe(0.25), Keyframe(0.25), Keyframe(0.25),
								 Keyframe(0.25));
		clip.AddEffect(&s);
		read_forward_backward(clip);
		clip.Close();
		r.Close();
	});

	cout << "Overall," << total << "\n";
	return 0;
}

// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <array>
#include <cmath>
#include <cstdlib>
#include <iterator>
#include <memory>
#include <string>
#include <QByteArray>
#include <QTemporaryDir>
#include <QtGlobal>

#include "openshot_catch.h"

#include "ColorManagement.h"
#include "FFmpegReader.h"
#include "FFmpegWriter.h"
#include "Frame.h"
#include "Settings.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
}

using namespace openshot;

namespace {

// Patches of examples/color-chart-*.mp4 (see tests/generate-color-charts.py):
// SDR code values, then 600 and 1000 nit highlights. 4x4 grid of 160x96 patches.
struct Patch { const char* name; std::array<int, 3> code; double nits; };
const Patch kPatches[] = {
	{"reference white", {255, 255, 255}, 0}, {"18% grey", {118, 118, 118}, 0},
	{"light skin", {224, 172, 140}, 0}, {"dark skin", {141, 85, 62}, 0},
	{"sky blue", {98, 155, 210}, 0}, {"foliage", {87, 122, 59}, 0},
	{"red", {220, 30, 30}, 0}, {"green", {40, 200, 40}, 0}, {"blue", {30, 40, 220}, 0},
	{"yellow", {240, 220, 40}, 0}, {"near black", {20, 20, 20}, 0},
	{"600 nit highlight", {}, 600}, {"1000 nit highlight", {}, 1000},
};

// The SDR value OpenShot should produce: BT.2408 placement inverted, with
// the shared tone curve for HDR charts (relative_peak > 1).
std::array<int, 3> Expected(const Patch& patch, float relative_peak) {
	float rgb[3];
	for (int c = 0; c < 3; ++c)
		rgb[c] = patch.nits > 0 ? float(patch.nits / color::kReferenceWhiteNits)
		                        : float(std::pow(patch.code[c] / 255.0, 2.4));
	const float brightest = std::max(rgb[0], std::max(rgb[1], rgb[2]));
	const float scale = relative_peak > 1.0f ? color::ToneMap(brightest, relative_peak) / brightest
	                                        : std::min(brightest, 1.0f) / brightest;
	std::array<int, 3> result;
	for (int c = 0; c < 3; ++c)
		result[c] = int(std::lround(255.0 * std::pow(std::min(rgb[c] * scale, 1.0f), 1.0 / 2.4)));
	return result;
}

std::array<int, 3> PatchColor(std::shared_ptr<Frame> frame, int index) {
	const int x0 = (index % 4) * 160 + 70, y0 = (index / 4) * 96 + 42;
	long sum[3] = {0, 0, 0};
	for (int y = y0; y < y0 + 12; ++y) {
		const unsigned char* row = frame->GetPixels(y);
		for (int x = x0; x < x0 + 20; ++x)
			for (int c = 0; c < 3; ++c) sum[c] += row[x * 4 + c];
	}
	return {int(sum[0] / 240), int(sum[1] / 240), int(sum[2] / 240)};
}

void CheckChart(const std::string& file, float relative_peak, int tolerance) {
	FFmpegReader reader(TEST_MEDIA_PATH + file);
	reader.Open();
	auto frame = reader.GetFrame(2);
	REQUIRE(frame->GetWidth() == 640);
	REQUIRE(frame->GetHeight() == 384);
	for (int i = 0; i < int(std::size(kPatches)); ++i) {
		const auto expected = Expected(kPatches[i], relative_peak);
		const auto actual = PatchColor(frame, i);
		INFO(file << " " << kPatches[i].name << " expected (" << expected[0] << "," << expected[1] << ","
		     << expected[2] << ") got (" << actual[0] << "," << actual[1] << "," << actual[2] << ")");
		for (int c = 0; c < 3; ++c) CHECK(std::abs(actual[c] - expected[c]) <= tolerance);
	}
	reader.Close();
}

// Decode and convert on the CPU. Vulkan builds ignore HARDWARE_DECODER and
// select the CPU pipeline with OPENSHOT_RENDER_BACKEND=cpu.
struct SoftwareDecode {
	int previous = Settings::Instance()->HARDWARE_DECODER;
#ifdef OPENSHOT_ENABLE_GPU_FRAME
	QByteArray backend;
	bool had_backend = false;
#endif
	SoftwareDecode() {
		Settings::Instance()->HARDWARE_DECODER = 0;
#ifdef OPENSHOT_ENABLE_GPU_FRAME
		had_backend = qEnvironmentVariableIsSet("OPENSHOT_RENDER_BACKEND");
		backend = qgetenv("OPENSHOT_RENDER_BACKEND");
		qputenv("OPENSHOT_RENDER_BACKEND", "cpu");
#endif
	}
	~SoftwareDecode() {
		Settings::Instance()->HARDWARE_DECODER = previous;
#ifdef OPENSHOT_ENABLE_GPU_FRAME
		if (had_backend) qputenv("OPENSHOT_RENDER_BACKEND", backend);
		else qunsetenv("OPENSHOT_RENDER_BACKEND");
#endif
	}
};

} // namespace

TEST_CASE("YUV matrices for decoding and encoding", "[libopenshot][color]") {
	CHECK(color::DecodeMatrix(AVCOL_SPC_BT709, 640, 480) == AVCOL_SPC_BT709);
	CHECK(color::DecodeMatrix(AVCOL_SPC_BT2020_NCL, 3840, 2160) == AVCOL_SPC_BT2020_NCL);
	// Untagged files keep BT.601, as earlier OpenShot exports were written.
	CHECK(color::DecodeMatrix(AVCOL_SPC_UNSPECIFIED, 1920, 1080) == AVCOL_SPC_SMPTE170M);
	CHECK(color::EncodeMatrix(1920, 1080) == AVCOL_SPC_BT709);
	CHECK(color::EncodeMatrix(1280, 720) == AVCOL_SPC_BT709);
	CHECK(color::EncodeMatrix(720, 576) == AVCOL_SPC_SMPTE170M);
	CHECK(color::EncodeMatrix(640, 480) == AVCOL_SPC_SMPTE170M);
}

TEST_CASE("Frames describe HDR transfer and gamut", "[libopenshot][color]") {
	std::unique_ptr<AVFrame, void(*)(AVFrame*)> frame(av_frame_alloc(), [](AVFrame* f) { av_frame_free(&f); });
	REQUIRE(frame);
	CHECK_FALSE(color::DescribeFrame(frame.get(), 1920, 1080).NeedsConversion());

	frame->color_trc = AVCOL_TRC_ARIB_STD_B67;
	frame->color_primaries = AVCOL_PRI_BT2020;
	frame->colorspace = AVCOL_SPC_BT2020_NCL;
	auto hlg = color::DescribeFrame(frame.get(), 1920, 1080);
	CHECK(hlg.transfer == color::Transfer::HLG);
	CHECK(hlg.bt2020);
	CHECK(hlg.matrix == AVCOL_SPC_BT2020_NCL);
	CHECK(hlg.RelativePeak() == Approx(1000.0 / 203.0));

	frame->color_trc = AVCOL_TRC_SMPTE2084;
	auto pq = color::DescribeFrame(frame.get(), 1920, 1080, 4000.0);
	CHECK(pq.transfer == color::Transfer::PQ);
	CHECK(pq.RelativePeak() == Approx(4000.0 / 203.0));

	// BT.2020 SDR still needs gamut conversion; unspecified primaries follow the matrix.
	frame->color_trc = AVCOL_TRC_BT2020_10;
	frame->color_primaries = AVCOL_PRI_UNSPECIFIED;
	auto wide = color::DescribeFrame(frame.get(), 1920, 1080);
	CHECK(wide.transfer == color::Transfer::Sdr);
	CHECK(wide.bt2020);
	CHECK(wide.NeedsConversion());
}

TEST_CASE("Tone curve keeps SDR levels and compresses highlights", "[libopenshot][color]") {
	const float peak = 1000.0f / 203.0f;
	CHECK(color::ToneMap(0.5f, peak) == 0.5f);
	CHECK(color::ToneMap(color::kToneKnee, peak) == Approx(color::kToneKnee));
	CHECK(color::ToneMap(peak, peak) == Approx(1.0f));
	CHECK(color::ToneMap(2.0f * peak, peak) == 1.0f);
	CHECK(color::ToneMap(3.0f, 1.0f) == 1.0f);
	float previous = 0.0f;
	for (float value = 0.0f; value <= peak; value += 0.01f) {
		const float mapped = color::ToneMap(value, peak);
		CHECK(mapped >= previous);
		previous = mapped;
	}
	// Reference white stays bright, leaving headroom for highlights.
	CHECK(color::ToneMap(1.0f, peak) > 0.85f);
	CHECK(color::ToneMap(1.0f, peak) < 1.0f);
}

TEST_CASE("CPU decoding converts SDR, HLG and PQ charts to SDR BT.709", "[libopenshot][color][ffmpegreader]") {
	SoftwareDecode software;
	const float hdr_peak = float(color::kDefaultPeakNits / color::kReferenceWhiteNits);
	CheckChart("color-chart-sdr-bt709.mp4", 1.0f, 4);
	CheckChart("color-chart-hlg.mp4", hdr_peak, 4);
	CheckChart("color-chart-pq.mp4", hdr_peak, 4);
}

TEST_CASE("Exports are tagged and round trip their colors", "[libopenshot][color][ffmpegwriter]") {
	if (!avcodec_find_encoder_by_name("libx264")) {
		WARN("libx264 unavailable; skipping");
		return;
	}
	SoftwareDecode software;
	struct Size { int width, height; AVColorSpace matrix; };
	for (const Size size : {Size{1280, 720, AVCOL_SPC_BT709}, Size{640, 480, AVCOL_SPC_SMPTE170M}}) {
		QTemporaryDir directory;
		REQUIRE(directory.isValid());
		const std::string path = directory.filePath("color.mp4").toStdString();
		const char* colors[] = {"#dc1e1e", "#28c828", "#1e28dc", "#e0ac8c"};
		{
			FFmpegWriter writer(path);
			writer.SetVideoOptions(true, "libx264", Fraction(30, 1), size.width, size.height,
			                       Fraction(1, 1), false, false, 20000000);
			writer.Open();
			for (int number = 1; number <= 4; ++number)
				writer.WriteFrame(std::make_shared<Frame>(number, size.width, size.height, colors[number - 1]));
			writer.Close();
		}

		AVFormatContext* input = nullptr;
		REQUIRE(avformat_open_input(&input, path.c_str(), nullptr, nullptr) == 0);
		REQUIRE(avformat_find_stream_info(input, nullptr) >= 0);
		const int index = av_find_best_stream(input, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
		REQUIRE(index >= 0);
		const AVCodecParameters* parameters = input->streams[index]->codecpar;
		CHECK(parameters->color_space == size.matrix);
		CHECK(parameters->color_primaries == AVCOL_PRI_BT709);
		CHECK(parameters->color_trc == AVCOL_TRC_BT709);
		CHECK(parameters->color_range == AVCOL_RANGE_MPEG);
		avformat_close_input(&input);

		FFmpegReader reader(path);
		reader.Open();
		for (int number = 1; number <= 4; ++number) {
			const QColor expected(colors[number - 1]);
			const QColor actual = QColor::fromRgba(reader.GetFrame(number)->GetImage()->pixel(size.width / 2, size.height / 2));
			INFO(size.width << "x" << size.height << " " << colors[number - 1] << " got " << actual.name().toStdString());
			CHECK(std::abs(actual.red() - expected.red()) <= 4);
			CHECK(std::abs(actual.green() - expected.green()) <= 4);
			CHECK(std::abs(actual.blue() - expected.blue()) <= 4);
		}
		reader.Close();
	}
}

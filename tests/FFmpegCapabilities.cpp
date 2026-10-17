// SPDX-FileCopyrightText: 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <memory>
#include <QTemporaryDir>

#include "openshot_catch.h"
#include "FFmpegUtilities.h"
#include "FFmpegWriter.h"

using namespace openshot;

TEST_CASE("Audio capability validation preserves supported layouts and rejects invalid options",
          "[libopenshot][ffmpegwriter][codec-capabilities]")
{
    AV_REGISTER_ALL
    REQUIRE(avcodec_find_encoder_by_name("mp2") != nullptr);
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    FFmpegWriter writer(directory.filePath("audio.mp2").toStdString());

    SECTION("Mono is supported") {
        writer.SetAudioOptions(true, "mp2", 48000, 1, LAYOUT_MONO, 128000);
        CHECK_NOTHROW(writer.PrepareStreams());
    }
    SECTION("Stereo is supported") {
        writer.SetAudioOptions(true, "mp2", 48000, 2, LAYOUT_STEREO, 128000);
        CHECK_NOTHROW(writer.PrepareStreams());
    }
    SECTION("Surround is rejected by the mono/stereo encoder") {
        writer.SetAudioOptions(true, "mp2", 48000, 6, LAYOUT_5POINT1, 128000);
        CHECK_THROWS_AS(writer.PrepareStreams(), InvalidChannels);
    }
    SECTION("An unsupported sample rate is rejected") {
        writer.SetAudioOptions(true, "mp2", 12345, 2, LAYOUT_STEREO, 128000);
        CHECK_THROWS_AS(writer.PrepareStreams(), InvalidSampleRate);
    }
}

TEST_CASE("Codec capability helpers distinguish unrestricted support from query errors",
          "[libopenshot][codec-capabilities]")
{
    AV_REGISTER_ALL
    const AVCodec* codec = avcodec_find_encoder_by_name("pcm_s16le");
    REQUIRE(codec != nullptr);
    std::unique_ptr<AVCodecContext, void(*)(AVCodecContext*)> context(
        avcodec_alloc_context3(codec),
        [](AVCodecContext* value) { avcodec_free_context(&value); });
    REQUIRE(context != nullptr);
    // PCM accepts arbitrary sample rates and layouts, unlike MP2 above.
    CHECK(ffmpeg_codec_sample_rates(codec, context.get()) == nullptr);
    const auto* formats = ffmpeg_codec_sample_fmts(codec, context.get());
    REQUIRE(formats != nullptr);
    CHECK(formats[0] == AV_SAMPLE_FMT_S16);
#if HAVE_CH_LAYOUT
    CHECK(ffmpeg_codec_ch_layouts(codec, context.get()) == nullptr);
#endif
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(61, 13, 100)
    // Asking an audio codec for video formats fails; it is not an unrestricted list.
    CHECK_THROWS_AS(ffmpeg_codec_pix_fmts(codec, context.get()), InvalidCodec);
#endif
}

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(61, 13, 100)
TEST_CASE("Codec capability queries honor encoder compliance settings",
          "[libopenshot][codec-capabilities]")
{
    const AVCodec* codec = avcodec_find_encoder_by_name("mjpeg");
    REQUIRE(codec != nullptr);
    std::unique_ptr<AVCodecContext, void(*)(AVCodecContext*)> context(
        avcodec_alloc_context3(codec),
        [](AVCodecContext* value) { avcodec_free_context(&value); });
    REQUIRE(context != nullptr);
    context->strict_std_compliance = FF_COMPLIANCE_NORMAL;
    const auto* ranges = ffmpeg_codec_config<AVColorRange>(
        codec, context.get(), AV_CODEC_CONFIG_COLOR_RANGE);
    REQUIRE(ranges != nullptr);
    CHECK(ranges[0] == AVCOL_RANGE_JPEG);

    context->strict_std_compliance = FF_COMPLIANCE_UNOFFICIAL;
    ranges = ffmpeg_codec_config<AVColorRange>(
        codec, context.get(), AV_CODEC_CONFIG_COLOR_RANGE);
    REQUIRE(ranges != nullptr);
    CHECK(ranges[0] == AVCOL_RANGE_MPEG);
}
#endif

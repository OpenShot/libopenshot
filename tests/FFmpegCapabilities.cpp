// SPDX-FileCopyrightText: 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <memory>
#include <QFileInfo>
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

TEST_CASE("Encoder info reports supported audio options as JSON",
          "[libopenshot][ffmpegwriter][codec-capabilities]")
{
    const Json::Value mp2 = openshot::stringToJson(FFmpegWriter::GetEncoderInfo("mp2"));
    CHECK(mp2["found"].asBool());
    CHECK(mp2["name"].asString() == "mp2");
    CHECK(mp2["type"].asString() == "audio");
    CHECK(mp2["lossy"].asBool());
    REQUIRE(mp2["sample_rates"].isArray());
    bool has_48k = false;
    bool has_odd_rate = false;
    for (const auto& rate : mp2["sample_rates"]) {
        has_48k = has_48k || rate.asInt() == 48000;
        has_odd_rate = has_odd_rate || rate.asInt() == 12345;
    }
    CHECK(has_48k);
    CHECK_FALSE(has_odd_rate);
    REQUIRE(mp2["channel_layouts"].isArray());
    bool has_surround = false;
    for (const auto& layout : mp2["channel_layouts"])
        has_surround = has_surround || layout.asUInt64() == LAYOUT_5POINT1;
    CHECK_FALSE(has_surround);

    // PCM accepts any sample rate, and has no use for a bit rate
    const Json::Value pcm = openshot::stringToJson(FFmpegWriter::GetEncoderInfo("pcm_s16le"));
    CHECK(pcm["found"].asBool());
    CHECK(pcm["sample_rates"].isNull());
    CHECK(pcm["lossless"].asBool());
    CHECK_FALSE(pcm["lossy"].asBool());

    const Json::Value missing = openshot::stringToJson(FFmpegWriter::GetEncoderInfo("not-a-codec"));
    CHECK_FALSE(missing["found"].asBool());
}

TEST_CASE("Encoder info reports video pixel formats",
          "[libopenshot][ffmpegwriter][codec-capabilities]")
{
    const Json::Value mpeg4 = openshot::stringToJson(FFmpegWriter::GetEncoderInfo("mpeg4"));
    REQUIRE(mpeg4["found"].asBool());
    CHECK(mpeg4["type"].asString() == "video");
    REQUIRE(mpeg4["pixel_formats"].isArray());
    CHECK(mpeg4["pixel_formats"][0].asString() == "yuv420p");
    CHECK(mpeg4["sample_rates"].isNull());
    CHECK(mpeg4["codec"].asString() == "mpeg4");
    CHECK_FALSE(mpeg4["intra_only"].asBool());

    // MJPEG stores every frame as a picture
    const Json::Value mjpeg = openshot::stringToJson(FFmpegWriter::GetEncoderInfo("mjpeg"));
    REQUIRE(mjpeg["found"].asBool());
    CHECK(mjpeg["intra_only"].asBool());
    CHECK(mjpeg["image"].asBool());
    CHECK_FALSE(mpeg4["image"].asBool());
}

TEST_CASE("Format info matches formats by extension and by name",
          "[libopenshot][ffmpegwriter][codec-capabilities]")
{
    const Json::Value mkv = openshot::stringToJson(FFmpegWriter::GetFormatInfo("mkv"));
    CHECK(mkv["found"].asBool());
    CHECK(mkv["name"].asString() == "matroska");
    const Json::Value matroska = openshot::stringToJson(FFmpegWriter::GetFormatInfo("matroska"));
    CHECK(matroska["name"].asString() == "matroska");
    const Json::Value dotted = openshot::stringToJson(FFmpegWriter::GetFormatInfo(".wav"));
    CHECK(dotted["name"].asString() == "wav");
    CHECK(dotted["default_audio_codec"].asString() == "pcm_s16le");
    const Json::Value missing = openshot::stringToJson(FFmpegWriter::GetFormatInfo("not-a-format"));
    CHECK_FALSE(missing["found"].asBool());
}

TEST_CASE("Formats list the extensions the writer can export to",
          "[libopenshot][ffmpegwriter][codec-capabilities]")
{
    const Json::Value formats = openshot::stringToJson(FFmpegWriter::GetFormats());
    REQUIRE(formats.isArray());
    bool has_mkv = false;
    bool has_wav = false;
    for (const auto& format : formats) {
        if (format["extension"].asString() == "mkv") {
            has_mkv = true;
            CHECK(format["name"].asString() == "matroska");
        }
        if (format["extension"].asString() == "wav") {
            has_wav = true;
            CHECK(format["default_audio_codec"].asString() == "pcm_s16le");
            CHECK(format["default_video_codec"].asString().empty());
        }
        // Each listed extension picks its own format, as the writer would
        const Json::Value info = openshot::stringToJson(
            FFmpegWriter::GetFormatInfo(format["extension"].asString()));
        CHECK(info["name"].asString() == format["name"].asString());
    }
    CHECK(has_mkv);
    CHECK(has_wav);
}

TEST_CASE("Formats report which codecs they can store",
          "[libopenshot][ffmpegwriter][codec-capabilities]")
{
    CHECK(FFmpegWriter::IsCodecSupportedByFormat("mkv", "mp2") == 1);
    CHECK(FFmpegWriter::IsCodecSupportedByFormat("wav", "mpeg4") == 0);
    CHECK(FFmpegWriter::IsCodecSupportedByFormat("mkv", "not-a-codec") == 0);
    CHECK(FFmpegWriter::IsCodecSupportedByFormat("not-a-format", "mp2") == 0);

    const Json::Value audio = openshot::stringToJson(FFmpegWriter::GetEncoders("audio"));
    REQUIRE(audio.isArray());
    bool has_mp2 = false;
    bool has_video = false;
    for (const auto& encoder : audio) {
        has_mp2 = has_mp2 || encoder["name"].asString() == "mp2";
        has_video = has_video || encoder["name"].asString() == "mpeg4";
    }
    CHECK(has_mp2);
    CHECK_FALSE(has_video);

    // WAV can't hold MPEG-4 video, so no video encoders survive the filter
    const Json::Value wav_video = openshot::stringToJson(FFmpegWriter::GetEncoders("video", "wav"));
    for (const auto& encoder : wav_video)
        CHECK(encoder["name"].asString() != "mpeg4");
    CHECK(openshot::stringToJson(FFmpegWriter::GetEncoders("audio", "not-a-format")).empty());
}

TEST_CASE("Testing settings opens the encoders without writing a file",
          "[libopenshot][ffmpegwriter][codec-capabilities]")
{
    AV_REGISTER_ALL
    REQUIRE(avcodec_find_encoder_by_name("mp2") != nullptr);
    QTemporaryDir directory;
    REQUIRE(directory.isValid());

    SECTION("A format accepts a codec it can hold") {
        const std::string path = directory.filePath("audio.mkv").toStdString();
        FFmpegWriter writer(path);
        writer.SetAudioOptions(true, "mp2", 48000, 2, LAYOUT_STEREO, 128000);
        writer.PrepareStreams();
        const Json::Value result = openshot::stringToJson(writer.Test());
        CHECK(result["ok"].asBool());
        CHECK(result["stage"].asString().empty());
        CHECK(result["elapsed_ms"].asInt64() >= 0);
        CHECK_FALSE(QFileInfo::exists(QString::fromStdString(path)));
    }
    SECTION("A format rejects a codec with FFmpeg's explanation") {
        const std::string path = directory.filePath("audio.mp3").toStdString();
        FFmpegWriter writer(path);
        writer.SetAudioOptions(true, "mp2", 48000, 2, LAYOUT_STEREO, 128000);
        writer.PrepareStreams();
        const Json::Value result = openshot::stringToJson(writer.Test());
        CHECK_FALSE(result["ok"].asBool());
        CHECK(result["stage"].asString() == "header");
        CHECK(result["message"].asString().find("MP3") != std::string::npos);
        CHECK(result["log"].size() >= 1);
        CHECK_FALSE(QFileInfo::exists(QString::fromStdString(path)));
    }
}

TEST_CASE("Closing after a failed header does not write a trailer",
          "[libopenshot][ffmpegwriter][codec-capabilities]")
{
    AV_REGISTER_ALL
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    FFmpegWriter writer(directory.filePath("audio.mp3").toStdString());
    writer.SetAudioOptions(true, "mp2", 48000, 2, LAYOUT_STEREO, 128000);
    CHECK_THROWS_AS(writer.Open(), InvalidFile);
    CHECK_NOTHROW(writer.Close());
}

TEST_CASE("AMD AMF encoder names are identified independently of hardware",
          "[libopenshot][codec-capabilities][hardware-encoders]")
{
    CHECK(ffmpeg_is_amf_encoder("h264_amf"));
    CHECK(ffmpeg_is_amf_encoder("hevc_amf"));
    CHECK(ffmpeg_is_amf_encoder("av1_amf"));

    CHECK_FALSE(ffmpeg_is_amf_encoder(nullptr));
    CHECK_FALSE(ffmpeg_is_amf_encoder(""));
    CHECK_FALSE(ffmpeg_is_amf_encoder("h264_vaapi"));
    CHECK_FALSE(ffmpeg_is_amf_encoder("h264_nvenc"));
    CHECK_FALSE(ffmpeg_is_amf_encoder("libx264"));
    CHECK_FALSE(ffmpeg_is_amf_encoder("H264_AMF"));
    CHECK_FALSE(ffmpeg_is_amf_encoder("not_h264_amf"));
}

TEST_CASE("AMD AMF capability detection requires FFmpeg registration",
          "[libopenshot][codec-capabilities][hardware-encoders]")
{
    const char* names[] = {"h264_amf", "hevc_amf", "av1_amf"};

    for (const char* name : names) {
        const bool registered =
            avcodec_find_encoder_by_name(name) != nullptr;

        CHECK(ffmpeg_amf_encoder_is_compiled(name) == registered);
    }

    CHECK_FALSE(ffmpeg_amf_encoder_is_compiled(nullptr));
    CHECK_FALSE(ffmpeg_amf_encoder_is_compiled(""));
    CHECK_FALSE(ffmpeg_amf_encoder_is_compiled("h264_vaapi"));
    CHECK_FALSE(ffmpeg_amf_encoder_is_compiled("h264_nvenc"));
    CHECK_FALSE(ffmpeg_amf_encoder_is_compiled("pcm_s16le"));
    CHECK_FALSE(ffmpeg_amf_encoder_is_compiled(
        "openshot_nonexistent_encoder"));
}

// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "ColorManagement.h"

#include <algorithm>
#include <cmath>
#include <cstring>

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/mastering_display_metadata.h>
#include <libswscale/swscale.h>
}

namespace openshot {
namespace color {
namespace {

// BT.2100 PQ (SMPTE ST 2084)
constexpr double kPqM1 = 2610.0 / 16384.0, kPqM2 = 2523.0 / 4096.0 * 128.0;
constexpr double kPqC1 = 3424.0 / 4096.0, kPqC2 = 2413.0 / 4096.0 * 32.0, kPqC3 = 2392.0 / 4096.0 * 32.0;
// BT.2100 HLG
constexpr double kHlgA = 0.17883277, kHlgB = 0.28466892, kHlgC = 0.55991073;
constexpr double kHlgGamma = 1.2; // system gamma at the nominal 1000 nit peak
// SDR display EOTF (BT.1886), as BT.2408 uses to place SDR within HDR.
constexpr double kSdrGamma = 2.4;

// Linear BT.2020 RGB to linear BT.709 RGB (BT.2087).
constexpr float kBt2020To709[9] = {
	 1.6604910f, -0.5876411f, -0.0728499f,
	-0.1245505f,  1.1328999f, -0.0083494f,
	-0.0181508f, -0.1005789f,  1.1187297f};
constexpr float kLuma2020[3] = {0.2627f, 0.6780f, 0.0593f};
constexpr float kLuma709[3] = {0.2126f, 0.7152f, 0.0722f};

// Lookups stay small enough for L1 cache. 12-bit signal indices exceed the
// 10-bit sources' precision (and PQ's 12-bit visibility threshold).
constexpr int kSignalBits = 12;
constexpr int kSignalSize = 1 << kSignalBits;
// Encode lookup indexed by the float's exponent and top 10 mantissa bits,
// covering linear 2^-24 (below half an 8-bit code) to 1.0.
constexpr uint32_t kEncodeShift = 13;
constexpr uint32_t kEncodeBase = (127u - 24u) << (23 - kEncodeShift);
constexpr int kEncodeSize = int((127u << (23 - kEncodeShift)) - kEncodeBase) + 1;

// Tone scale lookup indexed by the brightest channel's exponent and top 7
// mantissa bits (under 0.4% apart), from the knee to 64x reference white.
constexpr uint32_t kToneShift = 16;
constexpr uint32_t kToneBase = (127u - 1u) << (23 - kToneShift);
constexpr int kToneSize = int(((127u + 6u) << (23 - kToneShift)) - kToneBase);

inline uint32_t FloatBits(float value) { uint32_t bits; std::memcpy(&bits, &value, sizeof(bits)); return bits; }
inline int ToneIndex(float brightest) {
	const int index = int(FloatBits(brightest) >> kToneShift) - int(kToneBase);
	return std::clamp(index, 0, kToneSize - 1);
}
inline int EncodeIndex(float linear) {
	const int index = int(FloatBits(linear) >> kEncodeShift) - int(kEncodeBase);
	return std::clamp(index, 0, kEncodeSize - 1);
}

double PqToNits(double e) {
	const double p = std::pow(std::clamp(e, 0.0, 1.0), 1.0 / kPqM2);
	return 10000.0 * std::pow(std::max(p - kPqC1, 0.0) / (kPqC2 - kPqC3 * p), 1.0 / kPqM1);
}
double HlgToScene(double e) {
	e = std::clamp(e, 0.0, 1.0);
	return e <= 0.5 ? e * e / 3.0 : (std::exp((e - kHlgC) / kHlgA) + kHlgB) / 12.0;
}
bool IsBt2020Matrix(AVColorSpace matrix) {
	return matrix == AVCOL_SPC_BT2020_NCL || matrix == AVCOL_SPC_BT2020_CL;
}
} // namespace

AVColorSpace DecodeMatrix(AVColorSpace tagged, int width, int height) {
	switch (tagged) {
		case AVCOL_SPC_BT709:
		case AVCOL_SPC_BT470BG:
		case AVCOL_SPC_SMPTE170M:
		case AVCOL_SPC_SMPTE240M:
		case AVCOL_SPC_FCC:
		case AVCOL_SPC_BT2020_NCL:
		case AVCOL_SPC_BT2020_CL:
			return tagged;
		default:
			// Untagged files keep BT.601 at every size: earlier OpenShot exports
			// and older FFmpeg encodes used it without tagging, and
			// cameras and phones tag their files.
			(void) width; (void) height;
			return AVCOL_SPC_SMPTE170M;
	}
}

AVColorSpace EncodeMatrix(int width, int height) {
	// Tagged; this is also what players assume for untagged files.
	return (width >= 1280 || height > 576) ? AVCOL_SPC_BT709 : AVCOL_SPC_SMPTE170M;
}

int SwsColorspace(AVColorSpace matrix) {
	switch (matrix) {
		case AVCOL_SPC_BT709: return SWS_CS_ITU709;
		case AVCOL_SPC_FCC: return SWS_CS_FCC;
		case AVCOL_SPC_SMPTE240M: return SWS_CS_SMPTE240M;
		case AVCOL_SPC_BT2020_NCL:
		case AVCOL_SPC_BT2020_CL: return SWS_CS_BT2020;
		default: return SWS_CS_ITU601;
	}
}

float SourceColor::RelativePeak() const {
	if (transfer == Transfer::Sdr) return 1.0f;
	const double nits = transfer == Transfer::HLG ? kDefaultPeakNits : peak_nits;
	return float(std::clamp(nits / kReferenceWhiteNits, 1.0, 10000.0 / kReferenceWhiteNits));
}

double FramePeakNits(const AVFrame* frame) {
	if (!frame) return 0.0;
	if (const AVFrameSideData* light = av_frame_get_side_data(frame, AV_FRAME_DATA_CONTENT_LIGHT_LEVEL)) {
		const auto* cll = reinterpret_cast<const AVContentLightMetadata*>(light->data);
		if (cll->MaxCLL > 0) return double(cll->MaxCLL);
	}
	if (const AVFrameSideData* display = av_frame_get_side_data(frame, AV_FRAME_DATA_MASTERING_DISPLAY_METADATA)) {
		const auto* mastering = reinterpret_cast<const AVMasteringDisplayMetadata*>(display->data);
		if (mastering->has_luminance && mastering->max_luminance.den > 0) {
			const double nits = av_q2d(mastering->max_luminance);
			if (nits > 0.0) return nits;
		}
	}
	return 0.0;
}

SourceColor DescribeFrame(const AVFrame* frame, int width, int height, double peak_nits) {
	SourceColor result;
	const AVColorSpace tagged = frame ? frame->colorspace : AVCOL_SPC_UNSPECIFIED;
	result.matrix = DecodeMatrix(tagged, width, height);
	result.full_range = frame && frame->color_range == AVCOL_RANGE_JPEG;
	if (frame) {
		if (frame->color_trc == AVCOL_TRC_SMPTE2084) result.transfer = Transfer::PQ;
		else if (frame->color_trc == AVCOL_TRC_ARIB_STD_B67) result.transfer = Transfer::HLG;
		// Unspecified primaries follow a BT.2020 matrix, as players do.
		result.bt2020 = frame->color_primaries == AVCOL_PRI_BT2020 ||
			(frame->color_primaries == AVCOL_PRI_UNSPECIFIED && IsBt2020Matrix(tagged));
	}
	if (peak_nits > 0.0) result.peak_nits = peak_nits;
	return result;
}

float ToneMap(float value, float relative_peak) {
	if (value <= kToneKnee || relative_peak <= 1.0f) return std::min(value, 1.0f);
	// Linear below the knee; above it an extended-Reinhard shoulder with unit
	// slope at the knee that reaches 1.0 exactly at the content peak.
	const float span = 1.0f - kToneKnee;
	const float t = (value - kToneKnee) / span;
	const float w = std::max((relative_peak - kToneKnee) / span, 1.0f);
	const float shoulder = t * (1.0f + t / (w * w)) / (1.0f + t);
	return std::min(kToneKnee + span * shoulder, 1.0f);
}

SdrConverter::SdrConverter(const SourceColor& source_color)
	: source(source_color), peak(source_color.RelativePeak()), linearize(kSignalSize), encode(kEncodeSize) {
	for (int i = 0; i < kSignalSize; ++i) {
		// Bucket centre of the 16-bit codes sharing this index.
		const double e = (i + 0.5) / kSignalSize;
		double value;
		switch (source.transfer) {
			case Transfer::PQ: value = PqToNits(e) / kReferenceWhiteNits; break;
			case Transfer::HLG: value = HlgToScene(e); break;
			default: value = std::pow(e, kSdrGamma); break;
		}
		linearize[i] = float(value);
	}
	if (source.transfer == Transfer::HLG) {
		// OOTF: display = peak * Ys^(gamma-1) * scene, relative to reference white.
		hlg_gain.resize(kSignalSize + 1);
		for (int i = 0; i <= kSignalSize; ++i)
			hlg_gain[i] = float(kDefaultPeakNits / kReferenceWhiteNits * std::pow(double(i) / kSignalSize, kHlgGamma - 1.0));
	}
	tone_scale.resize(kToneSize);
	for (int i = 0; i < kToneSize; ++i) {
		const uint32_t bits = ((uint32_t(i) + kToneBase) << kToneShift) | (1u << (kToneShift - 1));
		float brightest;
		std::memcpy(&brightest, &bits, sizeof(brightest));
		tone_scale[i] = brightest > kToneKnee ? ToneMap(brightest, peak) / brightest : 1.0f;
	}
	for (int i = 0; i < kEncodeSize; ++i) {
		// Middle of the floats sharing this index.
		const uint32_t bits = ((uint32_t(i) + kEncodeBase) << kEncodeShift) | (1u << (kEncodeShift - 1));
		float linear;
		std::memcpy(&linear, &bits, sizeof(linear));
		encode[i] = uint8_t(std::lround(255.0 * std::pow(std::min(double(linear), 1.0), 1.0 / kSdrGamma)));
	}
}

void SdrConverter::ToSdr(float rgb[3]) const {
	if (source.bt2020) {
		const float r = rgb[0], g = rgb[1], b = rgb[2];
		rgb[0] = kBt2020To709[0] * r + kBt2020To709[1] * g + kBt2020To709[2] * b;
		rgb[1] = kBt2020To709[3] * r + kBt2020To709[4] * g + kBt2020To709[5] * b;
		rgb[2] = kBt2020To709[6] * r + kBt2020To709[7] * g + kBt2020To709[8] * b;
	}
	// Out-of-gamut colors clip per channel.
	for (int c = 0; c < 3; ++c) rgb[c] = std::max(rgb[c], 0.0f);
	const float brightest = std::max(rgb[0], std::max(rgb[1], rgb[2]));
	if (brightest > kToneKnee) {
		// Scale all channels by the max-channel curve, preserving hue.
		const float scale = ToneMap(brightest, peak) / brightest;
		for (int c = 0; c < 3; ++c) rgb[c] *= scale;
	}
	for (int c = 0; c < 3; ++c) rgb[c] = std::min(rgb[c], 1.0f);
}

template <bool Hlg, bool Gamut>
void SdrConverter::ConvertRows(const uint16_t* src, int src_stride_bytes, uint8_t* dst, int dst_stride_bytes,
                               int width, int height) const {
	const float* luma = Gamut ? kLuma2020 : kLuma709;
	constexpr int shift = 16 - kSignalBits;
	#pragma omp parallel for if(int64_t(width) * height >= 65536) schedule(static)
	for (int y = 0; y < height; ++y) {
		const uint16_t* in = reinterpret_cast<const uint16_t*>(reinterpret_cast<const uint8_t*>(src) + int64_t(y) * src_stride_bytes);
		uint8_t* out = dst + int64_t(y) * dst_stride_bytes;
		for (int x = 0; x < width; ++x, in += 4, out += 4) {
			float r = linearize[in[0] >> shift], g = linearize[in[1] >> shift], b = linearize[in[2] >> shift];
			if (Hlg) {
				const float scene_luma = luma[0] * r + luma[1] * g + luma[2] * b;
				const float gain = hlg_gain[std::min(kSignalSize, int(scene_luma * kSignalSize + 0.5f))];
				r *= gain; g *= gain; b *= gain;
			}
			if (Gamut) {
				const float r2 = kBt2020To709[0] * r + kBt2020To709[1] * g + kBt2020To709[2] * b;
				const float g2 = kBt2020To709[3] * r + kBt2020To709[4] * g + kBt2020To709[5] * b;
				const float b2 = kBt2020To709[6] * r + kBt2020To709[7] * g + kBt2020To709[8] * b;
				r = r2; g = g2; b = b2;
			}
			// Clip out-of-gamut colors, then scale by the max-channel tone curve.
			r = std::max(r, 0.0f); g = std::max(g, 0.0f); b = std::max(b, 0.0f);
			const float scale = tone_scale[ToneIndex(std::max(r, std::max(g, b)))];
			out[0] = encode[EncodeIndex(r * scale)];
			out[1] = encode[EncodeIndex(g * scale)];
			out[2] = encode[EncodeIndex(b * scale)];
			out[3] = uint8_t(in[3] >> 8);
		}
	}
}

void SdrConverter::Convert(const uint16_t* src, int src_stride_bytes, uint8_t* dst, int dst_stride_bytes,
                           int width, int height) const {
	const bool hlg = source.transfer == Transfer::HLG;
	if (hlg && source.bt2020) ConvertRows<true, true>(src, src_stride_bytes, dst, dst_stride_bytes, width, height);
	else if (hlg) ConvertRows<true, false>(src, src_stride_bytes, dst, dst_stride_bytes, width, height);
	else if (source.bt2020) ConvertRows<false, true>(src, src_stride_bytes, dst, dst_stride_bytes, width, height);
	else ConvertRows<false, false>(src, src_stride_bytes, dst, dst_stride_bytes, width, height);
}

} // namespace color
} // namespace openshot

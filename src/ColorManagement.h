// Copyright (c) 2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later
#ifndef OPENSHOT_COLOR_MANAGEMENT_H
#define OPENSHOT_COLOR_MANAGEMENT_H

#include <cstdint>
#include <vector>

extern "C" {
#include <libavutil/pixfmt.h>
}
struct AVFrame;

namespace openshot {
namespace color {

/// SDR graphics/reference white in HDR signals (ITU-R BT.2408). HDR sources
/// map this luminance to SDR 100%.
constexpr double kReferenceWhiteNits = 203.0;
/// Nominal HLG display peak (BT.2100) and the PQ peak assumed without metadata.
constexpr double kDefaultPeakNits = 1000.0;
/// Linear SDR level (1.0 = reference white) above which highlights compress.
/// Must match TONE_KNEE in src/vulkan/affine.comp.
constexpr float kToneKnee = 0.8f;

enum class Transfer : int { Sdr = 0, PQ = 1, HLG = 2 };

/// The YUV matrix to decode with: the tagged matrix, or BT.601 when untagged
/// (how OpenShot exports before 2026 and older FFmpeg encodes were written).
AVColorSpace DecodeMatrix(AVColorSpace tagged, int width, int height);
/// The YUV matrix OpenShot encodes with (and tags) for an output size:
/// BT.709 for HD and larger, BT.601 for SD.
AVColorSpace EncodeMatrix(int width, int height);
/// The swscale coefficient table index (SWS_CS_*) for a matrix.
int SwsColorspace(AVColorSpace matrix);

/// How a decoded source must be converted into OpenShot's SDR BT.709 RGBA.
struct SourceColor {
	AVColorSpace matrix = AVCOL_SPC_BT709;
	bool full_range = false;
	Transfer transfer = Transfer::Sdr;
	bool bt2020 = false;          ///< BT.2020 primaries: convert gamut to BT.709
	double peak_nits = kDefaultPeakNits; ///< HDR content peak, for tone mapping

	/// True when transfer or gamut conversion is required (HDR or wide gamut).
	bool NeedsConversion() const { return transfer != Transfer::Sdr || bt2020; }
	/// Peak in units of reference white (>= 1); 1 means no tone mapping.
	float RelativePeak() const;
};

/// Describe a frame's color. peak_nits is the content peak from stream
/// metadata (0 = unknown); it is only used for PQ.
SourceColor DescribeFrame(const AVFrame* frame, int width, int height, double peak_nits = 0.0);
/// Content peak from a frame's HDR10 static metadata (MaxCLL, then mastering
/// display maximum), or 0 when absent.
double FramePeakNits(const AVFrame* frame);

/// CPU conversion of nonlinear R'G'B'A (16-bit, native endian, full range, in
/// the source's primaries and transfer) into SDR BT.709 RGBA8. Lookup tables
/// are built once per SourceColor; Convert is thread-safe.
class SdrConverter {
public:
	explicit SdrConverter(const SourceColor& source);
	const SourceColor& Source() const { return source; }
	void Convert(const uint16_t* src, int src_stride_bytes, uint8_t* dst, int dst_stride_bytes,
	             int width, int height) const;
	/// Convert one linear-light RGB value (1.0 = reference white, source
	/// primaries) to SDR display RGB in [0,1]. Exposed for tests.
	void ToSdr(float rgb[3]) const;
private:
	SourceColor source;
	float peak;
	std::vector<float> linearize;  ///< 12-bit signal -> linear (relative to reference white)
	std::vector<float> hlg_gain;   ///< HLG OOTF gain by scene luminance
	std::vector<uint8_t> encode;   ///< linear (by float exponent/mantissa) -> BT.1886 8-bit code
	std::vector<float> tone_scale; ///< brightest channel -> tone curve scale
	template <bool Hlg, bool Gamut>
	void ConvertRows(const uint16_t* src, int src_stride_bytes, uint8_t* dst, int dst_stride_bytes,
	                 int width, int height) const;
};

/// Tone-map one linear SDR-relative value (max channel) to [0,1].
float ToneMap(float value, float relative_peak);

} // namespace color
} // namespace openshot
#endif

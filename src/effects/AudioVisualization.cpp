/**
 * @file
 * @brief Source file for AudioVisualization effect class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "AudioVisualization.h"
#include "Exceptions.h"
#include "Timeline.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <vector>

#include <QBrush>
#include <QColor>
#include <QConicalGradient>
#include <QLinearGradient>
#include <QLineF>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPolygonF>
#include <QRadialGradient>

#include <AppConfig.h>
#include <juce_audio_basics/juce_audio_basics.h>

using namespace openshot;

namespace {
	constexpr double PI = 3.14159265358979323846;

	float clampf(float value, float min_value, float max_value) {
		return std::max(min_value, std::min(max_value, value));
	}

	int clampi(int value, int min_value, int max_value) {
		return std::max(min_value, std::min(max_value, value));
	}

	QColor color_at(const Color& color, int64_t frame_number) {
		return QColor(
			clampi(color.red.GetInt(frame_number), 0, 255),
			clampi(color.green.GetInt(frame_number), 0, 255),
			clampi(color.blue.GetInt(frame_number), 0, 255),
			clampi(color.alpha.GetInt(frame_number), 0, 255));
	}

	QColor mix_color(const QColor& a, const QColor& b, float amount) {
		amount = clampf(amount, 0.0f, 1.0f);
		const float inv = 1.0f - amount;
		return QColor(
			clampi(std::lround(a.red() * inv + b.red() * amount), 0, 255),
			clampi(std::lround(a.green() * inv + b.green() * amount), 0, 255),
			clampi(std::lround(a.blue() * inv + b.blue() * amount), 0, 255),
			clampi(std::lround(a.alpha() * inv + b.alpha() * amount), 0, 255));
	}

	QColor alpha_color(QColor color, float alpha_scale) {
		color.setAlpha(clampi(std::lround(color.alpha() * alpha_scale), 0, 255));
		return color;
	}

	QColor hue_shift(QColor color, int degrees) {
		int h = 0;
		int s = 0;
		int v = 0;
		int a = color.alpha();
		color.getHsv(&h, &s, &v, &a);
		if (h < 0)
			h = 0;
		return QColor::fromHsv((h + degrees + 360) % 360, s, v, a);
	}

	QColor rainbow_color(const QColor& seed, float position, float spread) {
		int h = 0;
		int s = 0;
		int v = 0;
		int a = seed.alpha();
		seed.getHsv(&h, &s, &v, &a);
		if (h < 0)
			h = 0;
		const int hue = (h + static_cast<int>(std::lround(clampf(position, 0.0f, 1.0f) * 360.0f))) % 360;
		const int saturation = clampi(std::lround(s * (0.65f + clampf(spread, 0.0f, 1.0f) * 0.35f)), 0, 255);
		const int value = clampi(std::lround(v * (0.88f + clampf(spread, 0.0f, 1.0f) * 0.12f)), 0, 255);
		return QColor::fromHsv(hue, saturation, value, a);
	}

	template<typename Gradient>
	void set_rainbow_stops(Gradient& gradient, const QColor& seed, float spread, float alpha_scale = 1.0f) {
		const float stops[] = {0.0f, 0.16f, 0.33f, 0.50f, 0.66f, 0.83f, 1.0f};
		for (float stop : stops)
			gradient.setColorAt(stop, alpha_color(rainbow_color(seed, stop, spread), alpha_scale));
	}

	struct Palette {
		QColor base;
		QColor dark;
		QColor light;
		QColor accent;
		QColor glow;
	};

	Palette make_palette(const QColor& base, float glow_amount, int style) {
		Palette palette;
		palette.base = base;
		palette.dark = mix_color(base, QColor(0, 0, 0, base.alpha()), 0.62f);
		palette.light = mix_color(base, QColor(255, 255, 255, base.alpha()), 0.28f);
		palette.accent = mix_color(base, palette.light, 0.35f);
		if (style == AUDIO_VISUALIZATION_STYLE_MINIMAL) {
			palette.light = base;
			palette.accent = base;
		} else if (style == AUDIO_VISUALIZATION_STYLE_SOFT) {
			palette.light = mix_color(base, QColor(255, 255, 255, base.alpha()), 0.42f);
			palette.accent = palette.light;
		} else if (style == AUDIO_VISUALIZATION_STYLE_NEON) {
			palette.light = mix_color(base, QColor(255, 255, 255, base.alpha()), 0.18f);
			palette.accent = palette.light;
		}
		palette.glow = alpha_color(palette.light, 0.18f + glow_amount * 0.65f);
		return palette;
	}

	Palette make_palette(const QColor& base, float glow_amount, int style, float color_spread) {
		Palette palette = make_palette(base, glow_amount, style);
		color_spread = clampf(color_spread, 0.0f, 1.0f);
		palette.dark = mix_color(base, palette.dark, color_spread);
		palette.light = mix_color(base, palette.light, color_spread);
		palette.accent = mix_color(base, palette.accent, color_spread);
		palette.glow = alpha_color(mix_color(base, palette.glow, color_spread), 0.18f + glow_amount * 0.65f);
		return palette;
	}

	float style_glow_amount(int style, float glow) {
		if (style == AUDIO_VISUALIZATION_STYLE_NEON)
			return clampf(std::max(glow, 0.45f), 0.0f, 1.0f);
		if (style == AUDIO_VISUALIZATION_STYLE_SOFT)
			return clampf(std::max(glow, 0.22f), 0.0f, 1.0f);
		if (style == AUDIO_VISUALIZATION_STYLE_MINIMAL)
			return glow * 0.2f;
		return glow;
	}

	float style_stroke_width(int style, float glow) {
		if (style == AUDIO_VISUALIZATION_STYLE_MINIMAL)
			return 1.0f;
		if (style == AUDIO_VISUALIZATION_STYLE_SOFT)
			return 2.4f + glow * 1.4f;
		if (style == AUDIO_VISUALIZATION_STYLE_NEON)
			return 1.8f + glow * 1.2f;
		return 1.5f + glow * 1.6f;
	}

	float style_fill_alpha(int style) {
		if (style == AUDIO_VISUALIZATION_STYLE_MINIMAL)
			return 1.0f;
		if (style == AUDIO_VISUALIZATION_STYLE_NEON)
			return 0.82f;
		if (style == AUDIO_VISUALIZATION_STYLE_SOFT)
			return 0.74f;
		return 0.72f;
	}

	QBrush vertical_style_fill(float x0, float y0, float x1, float y1, const Palette& palette, int style, float alpha_scale = 1.0f) {
		if (style == AUDIO_VISUALIZATION_STYLE_MINIMAL)
			return QBrush(alpha_color(palette.base, style_fill_alpha(style) * alpha_scale));

		QLinearGradient grad(x0, y0, x1, y1);
		if (style == AUDIO_VISUALIZATION_STYLE_NEON) {
			grad.setColorAt(0.0, alpha_color(palette.light, 0.92f * alpha_scale));
			grad.setColorAt(0.5, alpha_color(palette.base, 0.70f * alpha_scale));
			grad.setColorAt(1.0, alpha_color(palette.base, 0.28f * alpha_scale));
		} else if (style == AUDIO_VISUALIZATION_STYLE_SOFT) {
			grad.setColorAt(0.0, alpha_color(palette.light, 0.48f * alpha_scale));
			grad.setColorAt(0.5, alpha_color(palette.base, 0.78f * alpha_scale));
			grad.setColorAt(1.0, alpha_color(palette.base, 0.48f * alpha_scale));
		} else {
			grad.setColorAt(0.0, alpha_color(palette.light, 0.72f * alpha_scale));
			grad.setColorAt(0.62, alpha_color(palette.base, 0.72f * alpha_scale));
			grad.setColorAt(1.0, alpha_color(palette.base, 0.38f * alpha_scale));
		}
		return QBrush(grad);
	}

	float normalized_frequency_to_hz(float value, bool high_frequency) {
		if (value > 1.0f)
			return value;

		const float min_hz = 20.0f;
		const float max_hz = 20000.0f;
		const float normalized = clampf(value, 0.0f, 1.0f);
		if (high_frequency && normalized <= 0.0f)
			return min_hz + 1.0f;
		return min_hz * std::pow(max_hz / min_hz, normalized);
	}

	float hz_to_normalized_frequency(float value) {
		if (value >= 0.0f && value <= 1.0f)
			return value;

		const float min_hz = 20.0f;
		const float max_hz = 20000.0f;
		const float hz = clampf(value, min_hz, max_hz);
		return clampf(std::log(hz / min_hz) / std::log(max_hz / min_hz), 0.0f, 1.0f);
	}

	std::vector<float> channel_samples(const std::shared_ptr<Frame>& frame, int channel, int wanted_points, float gain, float smooth) {
		std::vector<float> values(std::max(2, wanted_points), 0.0f);
		const int samples = frame->GetAudioSamplesCount();
		const int channels = frame->GetAudioChannelsCount();
		if (samples <= 0 || channels <= 0)
			return values;

		const int safe_channel = clampi(channel, 0, channels - 1);
		const float *audio = frame->GetAudioSampleBuffer()->getReadPointer(safe_channel);
		float previous = 0.0f;
		for (int i = 0; i < static_cast<int>(values.size()); ++i) {
			const int start = (i * samples) / values.size();
			const int end = std::max(start + 1, ((i + 1) * samples) / static_cast<int>(values.size()));
			float peak = 0.0f;
			float peak_magnitude = 0.0f;
			for (int sample = start; sample < end && sample < samples; ++sample) {
				const float value = audio[sample];
				const float magnitude = std::fabs(value);
				if (magnitude > peak_magnitude) {
					peak = value;
					peak_magnitude = magnitude;
				}
			}
			const float scaled = clampf(peak * gain, -1.0f, 1.0f);
			values[i] = previous * smooth + scaled * (1.0f - smooth);
			previous = values[i];
		}
		return values;
	}

	std::vector<float> combined_samples(const std::shared_ptr<Frame>& frame, int wanted_points, float gain, float smooth) {
		std::vector<float> values(std::max(2, wanted_points), 0.0f);
		const int samples = frame->GetAudioSamplesCount();
		const int channels = frame->GetAudioChannelsCount();
		if (samples <= 0 || channels <= 0)
			return values;

		std::vector<const float*> channel_data;
		channel_data.reserve(channels);
		for (int channel = 0; channel < channels; ++channel)
			channel_data.push_back(frame->GetAudioSampleBuffer()->getReadPointer(channel));

		float previous = 0.0f;
		for (int i = 0; i < static_cast<int>(values.size()); ++i) {
			const int start = (i * samples) / values.size();
			const int end = std::max(start + 1, ((i + 1) * samples) / static_cast<int>(values.size()));
			float peak = 0.0f;
			float peak_magnitude = 0.0f;
			for (int sample = start; sample < end && sample < samples; ++sample) {
				float mixed = std::accumulate(channel_data.begin(), channel_data.end(), 0.0f,
					[sample](float total, const float* channel) {
						return total + channel[sample];
					});
				mixed /= channels;
				const float magnitude = std::fabs(mixed);
				if (magnitude > peak_magnitude) {
					peak = mixed;
					peak_magnitude = magnitude;
				}
			}
			const float scaled = clampf(peak * gain, -1.0f, 1.0f);
			values[i] = previous * smooth + scaled * (1.0f - smooth);
			previous = values[i];
		}
		return values;
	}

	float reactive_level(const std::shared_ptr<Frame>& frame, int channel, float gain) {
		const int samples = frame->GetAudioSamplesCount();
		const int channels = frame->GetAudioChannelsCount();
		if (samples <= 0 || channels <= 0)
			return 0.0f;

		double total = 0.0;
		float peak = 0.0f;
		const int first_channel = channel >= 0 ? clampi(channel, 0, channels - 1) : 0;
		const int last_channel = channel >= 0 ? first_channel + 1 : channels;
		for (int c = first_channel; c < last_channel; ++c) {
			const float *audio = frame->GetAudioSampleBuffer()->getReadPointer(c);
			for (int i = 0; i < samples; ++i) {
				const float magnitude = std::fabs(audio[i]);
				total += magnitude * magnitude;
				peak = std::max(peak, magnitude);
			}
		}

		const float rms = std::sqrt(total / (samples * (last_channel - first_channel)));
		const float mixed = std::max(rms * 3.6f, peak * 1.15f);
		return std::pow(clampf(mixed * gain, 0.0f, 1.0f), 0.55f);
	}

	float band_level(const std::vector<float>& bins, float start, float end) {
		if (bins.empty())
			return 0.0f;

		const int first = clampi(std::lround(start * bins.size()), 0, static_cast<int>(bins.size()) - 1);
		const int last = clampi(std::lround(end * bins.size()), first + 1, static_cast<int>(bins.size()));
		const float total = std::accumulate(bins.begin() + first, bins.begin() + last, 0.0f);
		return clampf(total / (last - first), 0.0f, 1.0f);
	}

	std::vector<float> spectrum_bins(const std::shared_ptr<Frame>& frame, int bins, float gain, float smooth, float low_hz, float high_hz) {
		std::vector<float> result(std::max(2, bins), 0.0f);
		const int samples = frame->GetAudioSamplesCount();
		const int channels = frame->GetAudioChannelsCount();
		const int sample_rate = std::max(1, frame->SampleRate());
		if (samples <= 4 || channels <= 0)
			return result;

		const int max_source_samples = std::min(samples, 2048);
		const float nyquist = sample_rate * 0.5f;
		low_hz = clampf(low_hz, 1.0f, nyquist);
		high_hz = clampf(high_hz, low_hz + 1.0f, nyquist);

		std::vector<const float*> channel_data;
		channel_data.reserve(channels);
		for (int channel = 0; channel < channels; ++channel)
			channel_data.push_back(frame->GetAudioSampleBuffer()->getReadPointer(channel));

		std::vector<double> mixed_windowed(max_source_samples, 0.0);
		for (int sample = 0; sample < max_source_samples; ++sample) {
			float mixed = 0.0f;
			for (int channel = 0; channel < channels; ++channel)
				mixed += channel_data[channel][sample];
			mixed /= channels;
			const double window = 0.5 - 0.5 * std::cos((2.0 * PI * sample) / (max_source_samples - 1));
			mixed_windowed[sample] = mixed * window;
		}

		std::vector<float> magnitudes(result.size(), 0.0f);
		const int result_size = static_cast<int>(result.size());
		#pragma omp parallel for if(result_size * max_source_samples >= 32768) schedule(static)
		for (int bin = 0; bin < result_size; ++bin) {
			const float t = (bin + 0.5f) / result_size;
			const float hz = low_hz * std::pow(high_hz / low_hz, t);
			const double phase_delta = 2.0 * PI * hz / sample_rate;
			const double step_real = std::cos(phase_delta);
			const double step_imag = std::sin(phase_delta);
			double phase_real = 1.0;
			double phase_imag = 0.0;
			double real = 0.0;
			double imag = 0.0;

			for (int sample = 0; sample < max_source_samples; ++sample) {
				const double value = mixed_windowed[sample];
				real += value * phase_real;
				imag -= value * phase_imag;

				const double next_real = phase_real * step_real - phase_imag * step_imag;
				phase_imag = phase_real * step_imag + phase_imag * step_real;
				phase_real = next_real;
			}

			magnitudes[bin] = clampf(std::sqrt(real * real + imag * imag) / max_source_samples * gain * 8.0f, 0.0f, 1.0f);
		}

		float previous = 0.0f;
		for (int bin = 0; bin < result_size; ++bin) {
			result[bin] = previous * smooth + magnitudes[bin] * (1.0f - smooth);
			previous = result[bin];
		}
		return result;
	}

	void draw_glow_path(QPainter& painter, const QPainterPath& path, const Palette& palette, float width, float glow) {
		if (glow > 0.01f) {
			QPen glow_pen(palette.glow, width + glow * 18.0f, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
			painter.setPen(glow_pen);
			painter.drawPath(path);
		}
		QPen pen(palette.base, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
		painter.setPen(pen);
		painter.drawPath(path);
	}

	void draw_glow_polyline(QPainter& painter, const QPolygonF& points, const Palette& palette, float width, float glow) {
		if (points.size() < 2)
			return;
		if (glow > 0.01f) {
			QPen glow_pen(palette.glow, width + glow * 14.0f, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
			painter.setPen(glow_pen);
			painter.drawPolyline(points);
		}
		QPen pen(palette.base, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
		painter.setPen(pen);
		painter.drawPolyline(points);
	}
}

AudioVisualization::AudioVisualization() :
	visualization_type(AUDIO_VISUALIZATION_WAVEFORM),
	style(AUDIO_VISUALIZATION_STYLE_CLEAN),
	color((unsigned char)68, (unsigned char)170, (unsigned char)255, (unsigned char)255),
	intensity(1.0),
	smoothing(0.35),
	detail(0.75),
	glow(0.25),
	color_spread(0.6),
	color_mode(AUDIO_VISUALIZATION_COLOR_SEED),
	channel_layout(AUDIO_VISUALIZATION_CHANNEL_AUTO),
	frequency_low(0.0),
	frequency_high(1.0),
	background(AUDIO_VISUALIZATION_BACKGROUND_SOURCE)
{
	init_effect_details();
}

AudioVisualization::AudioVisualization(int visualization_type, Color color) :
	AudioVisualization()
{
	this->visualization_type = visualization_type;
	this->color = color;
}

void AudioVisualization::init_effect_details()
{
	InitEffectInfo();
	info.class_name = "AudioVisualization";
	info.name = "Audio Visualization";
	info.description = "Render waveform, spectrum, and other transparent audio visualizations.";
	info.has_audio = false;
	info.has_video = true;
}

std::shared_ptr<openshot::Frame> AudioVisualization::GetFrame(std::shared_ptr<openshot::Frame> frame, int64_t frame_number)
{
	const std::shared_ptr<QImage> frame_image = frame->GetImage();
	int width = frame_image ? std::max(1, frame_image->width()) : 1;
	int height = frame_image ? std::max(1, frame_image->height()) : 1;
	if ((width <= 1 || height <= 1) && ParentTimeline()) {
		if (Timeline* timeline = dynamic_cast<Timeline*>(ParentTimeline())) {
			if (timeline->info.width > 1 && timeline->info.height > 1) {
				width = timeline->info.width;
				height = timeline->info.height;
			}
		}
	}
	const float intensity_value = clampf(intensity.GetValue(frame_number), 0.0f, 10.0f);
	const float smoothing_value = clampf(smoothing.GetValue(frame_number), 0.0f, 1.0f);
	const float detail_value = clampf(detail.GetValue(frame_number), 0.0f, 1.0f);
	const float glow_value = clampf(glow.GetValue(frame_number), 0.0f, 1.0f);
	const float color_spread_value = clampf(color_spread.GetValue(frame_number), 0.0f, 1.0f);
	const int mode = clampi(visualization_type, 0, AUDIO_VISUALIZATION_RADIAL_BARS);
	const bool uses_frequency = mode == AUDIO_VISUALIZATION_BARS ||
		mode == AUDIO_VISUALIZATION_SPECTRUM ||
		mode == AUDIO_VISUALIZATION_RADIAL ||
		mode == AUDIO_VISUALIZATION_RADIAL_BARS ||
		mode == AUDIO_VISUALIZATION_PARTICLES;
	const float low_hz = uses_frequency ? normalized_frequency_to_hz(frequency_low.GetValue(frame_number), false) : 20.0f;
	const float high_hz = uses_frequency
		? std::max(low_hz + 1.0f, normalized_frequency_to_hz(frequency_high.GetValue(frame_number), true))
		: 20000.0f;
	const int channels = std::max(1, frame->GetAudioChannelsCount());
	const bool split = channel_layout == AUDIO_VISUALIZATION_CHANNEL_SPLIT ||
		(channel_layout == AUDIO_VISUALIZATION_CHANNEL_AUTO && channels > 1 &&
		 (mode == AUDIO_VISUALIZATION_WAVEFORM || mode == AUDIO_VISUALIZATION_FILLED_WAVEFORM || mode == AUDIO_VISUALIZATION_VU_METER));
	const bool overlay = channel_layout == AUDIO_VISUALIZATION_CHANNEL_OVERLAY;

	const bool use_filled_waveform_argb = mode == AUDIO_VISUALIZATION_FILLED_WAVEFORM;
	auto visual = std::make_shared<QImage>(width, height,
		use_filled_waveform_argb ? QImage::Format_ARGB32_Premultiplied : QImage::Format_RGBA8888_Premultiplied);
	if (background == AUDIO_VISUALIZATION_BACKGROUND_SOURCE && frame_image && !frame_image->isNull()) {
		if (frame_image->width() == width && frame_image->height() == height)
			*visual = frame_image->copy();
		else
			*visual = frame_image->scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	} else {
		visual->fill(Qt::transparent);
	}

	QPainter painter(visual.get());
	if (background == AUDIO_VISUALIZATION_BACKGROUND_SOURCE && frame_image &&
		(frame_image->width() != width || frame_image->height() != height)) {
		painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
	}

	const QColor base = color_at(color, frame_number);
	const float styled_glow = style_glow_amount(style, glow_value);
	const Palette palette = make_palette(base, styled_glow, style, color_spread_value);

	if (background == AUDIO_VISUALIZATION_BACKGROUND_SOLID) {
		painter.fillRect(visual->rect(), palette.dark);
	} else if (background == AUDIO_VISUALIZATION_BACKGROUND_FADE) {
		QLinearGradient grad(0, 0, 0, height);
		grad.setColorAt(0.0, alpha_color(palette.dark, 0.0f));
		grad.setColorAt(1.0, alpha_color(palette.dark, 0.55f));
		painter.fillRect(visual->rect(), grad);
	} else if (background == AUDIO_VISUALIZATION_BACKGROUND_GRADIENT) {
		QLinearGradient grad(0, 0, width, height);
		grad.setColorAt(0.0, alpha_color(palette.dark, 0.55f));
		grad.setColorAt(1.0, alpha_color(palette.accent, 0.35f));
		painter.fillRect(visual->rect(), grad);
	}

	if (frame->GetAudioSamplesCount() <= 0) {
		painter.end();
		frame->AddImage(visual);
		return frame;
	}

	const float gain = std::max(0.01f, intensity_value);
	const float stroke = style_stroke_width(style, styled_glow);
	const bool is_waveform_mode = mode == AUDIO_VISUALIZATION_WAVEFORM || mode == AUDIO_VISUALIZATION_FILLED_WAVEFORM;
	painter.setRenderHint(QPainter::Antialiasing, !is_waveform_mode || styled_glow > 0.01f || stroke > 1.05f);

	if (is_waveform_mode) {
		painter.setRenderHint(QPainter::Antialiasing, false);
		const int lanes = split ? std::min(channels, 8) : (overlay ? std::min(channels, 8) : 1);
		const int columns = clampi(std::lround(width * (0.75f + detail_value * 0.25f)), 16, std::max(16, width));
		for (int lane = 0; lane < lanes; ++lane) {
			const std::vector<float> values = (split || overlay) ? channel_samples(frame, lane, columns, gain, smoothing_value)
																 : combined_samples(frame, columns, gain, smoothing_value);
			const float lane_top = split ? height * lane / static_cast<float>(lanes) : 0.0f;
			const float lane_height = split ? height / static_cast<float>(lanes) : static_cast<float>(height);
			const float center_y = lane_top + lane_height * 0.5f;
			const float amplitude = lane_height * 0.44f;
			const QColor lane_base = color_mode == AUDIO_VISUALIZATION_COLOR_RAINBOW
				? rainbow_color(base, lane / static_cast<float>(std::max(1, lanes)), color_spread_value)
				: (overlay ? mix_color(base, hue_shift(base, lane * 18), color_spread_value * 0.18f) : (lane % 2 ? palette.accent : base));
			const Palette lane_palette = make_palette(lane_base, styled_glow, style, color_spread_value);

			QPolygonF top_edge;
			top_edge.reserve(static_cast<int>(values.size()));
			for (int i = 0; i < static_cast<int>(values.size()); ++i) {
				const float x = (values.size() <= 1) ? 0.0f : (width - 1) * i / static_cast<float>(values.size() - 1);
				top_edge.append(QPointF(x, center_y - values[i] * amplitude));
			}

			const float alpha_scale = overlay ? 0.48f : 1.0f;
			const bool rainbow = color_mode == AUDIO_VISUALIZATION_COLOR_RAINBOW;
			if (mode == AUDIO_VISUALIZATION_FILLED_WAVEFORM) {
				painter.setPen(Qt::NoPen);
				if (styled_glow > 0.01f) {
					if (rainbow) {
						QLinearGradient glow_grad(0, 0, width, 0);
						set_rainbow_stops(glow_grad, base, color_spread_value, 0.78f * alpha_scale);
						painter.setBrush(QBrush(glow_grad));
					} else {
						painter.setBrush(alpha_color(lane_palette.glow, 0.78f * alpha_scale));
					}
					const int glow_step = 4;
					const int glow_pad = std::max(1, static_cast<int>(std::ceil(styled_glow * 5.0f)));
					for (int i = 0; i < static_cast<int>(values.size()); i += glow_step) {
						const int x0 = (i * width) / static_cast<int>(values.size());
						const int x1 = ((std::min<int>(i + glow_step, values.size())) * width) / static_cast<int>(values.size());
						float envelope = 0.0f;
						for (int j = i; j < std::min<int>(i + glow_step, values.size()); ++j) {
							const float magnitude = std::fabs(values[j]) * amplitude;
							envelope = std::max(envelope, values[j] == 0.0f ? 0.0f : std::max(1.0f, magnitude));
						}
						const int y0 = clampi(static_cast<int>(std::floor(center_y - envelope)) - glow_pad, 0, height);
						const int y1 = clampi(static_cast<int>(std::ceil(center_y + envelope)) + glow_pad, y0 + 1, height);
						painter.drawRect(QRect(x0, y0, std::max(1, x1 - x0), y1 - y0));
					}
				}

				if (rainbow) {
					QLinearGradient fill_grad(0, 0, width, 0);
					set_rainbow_stops(fill_grad, base, color_spread_value, style_fill_alpha(style) * alpha_scale);
					painter.setBrush(QBrush(fill_grad));
				} else {
					painter.setBrush(vertical_style_fill(0, lane_top, 0, lane_top + lane_height, lane_palette, style, alpha_scale));
				}
				for (int i = 0; i < static_cast<int>(values.size()); ++i) {
					const int x0 = (i * width) / static_cast<int>(values.size());
					const int x1 = ((i + 1) * width) / static_cast<int>(values.size());
					const float magnitude = std::fabs(values[i]) * amplitude;
					const float envelope = values[i] == 0.0f ? 0.0f : std::max(1.0f, magnitude);
					const int y0 = clampi(static_cast<int>(std::floor(center_y - envelope)), 0, height);
					const int y1 = clampi(static_cast<int>(std::ceil(center_y + envelope)), y0 + 1, height);
					painter.drawRect(QRect(x0, y0, std::max(1, x1 - x0), y1 - y0));
				}
			} else {
				const float line_width = std::max(1.0f, stroke);
				if (rainbow) {
					if (styled_glow > 0.01f) {
						QLinearGradient glow_grad(0, 0, width, 0);
						set_rainbow_stops(glow_grad, base, color_spread_value, 0.88f * alpha_scale);
						painter.setPen(QPen(QBrush(glow_grad), line_width + styled_glow * 16.0f, Qt::SolidLine, Qt::FlatCap));
						painter.drawPolyline(top_edge);
					}
					QLinearGradient line_grad(0, 0, width, 0);
					set_rainbow_stops(line_grad, base, color_spread_value, alpha_scale);
					painter.setPen(QPen(QBrush(line_grad), line_width, Qt::SolidLine, Qt::FlatCap));
					painter.drawPolyline(top_edge);
				} else {
					if (styled_glow > 0.01f) {
						painter.setPen(QPen(alpha_color(lane_palette.glow, 0.88f * alpha_scale),
							line_width + styled_glow * 16.0f, Qt::SolidLine, Qt::FlatCap));
						painter.drawPolyline(top_edge);
					}
					painter.setPen(QPen(alpha_color(lane_palette.base, alpha_scale),
						line_width, Qt::SolidLine, Qt::FlatCap));
					painter.drawPolyline(top_edge);
				}
			}
		}
	} else if (mode == AUDIO_VISUALIZATION_BARS) {
		const int bars = clampi(std::lround(16 + detail_value * 112), 8, std::max(8, width / 3));
		const std::vector<float> bins = spectrum_bins(frame, bars, gain, smoothing_value, low_hz, high_hz);
		const float gap = std::max(1.0f, width / static_cast<float>(bars) * 0.18f);
		const float bar_width = std::max(1.0f, width / static_cast<float>(bars) - gap);
		for (int i = 0; i < bars; ++i) {
			const float x = i * (bar_width + gap);
			const float h = std::max(1.0f, bins[i] * height * 0.88f);
			QRectF rect(x, height - h, bar_width, h);
			const Palette bar_palette = color_mode == AUDIO_VISUALIZATION_COLOR_RAINBOW
				? make_palette(rainbow_color(base, i / static_cast<float>(std::max(1, bars - 1)), color_spread_value), styled_glow, style, color_spread_value)
				: palette;
			if (styled_glow > 0.01f)
				painter.fillRect(rect.adjusted(-styled_glow * 2.0f, -styled_glow * 5.0f, styled_glow * 2.0f, 0), alpha_color(bar_palette.glow, 0.55f));
			painter.fillRect(rect, vertical_style_fill(0, rect.top(), 0, height, bar_palette, style));
		}
	} else if (mode == AUDIO_VISUALIZATION_SPECTRUM) {
		const int bins_count = clampi(std::lround(56 + detail_value * 220), 40, std::max(40, width / 2));
		const std::vector<float> bins = spectrum_bins(frame, bins_count, gain, smoothing_value, low_hz, high_hz);
		std::vector<QPointF> points;
		points.reserve(bins_count);
		for (int i = 0; i < bins_count; ++i) {
			const float x = i * width / static_cast<float>(std::max(1, bins_count - 1));
			const float h = std::max(1.0f, bins[i] * height * 0.84f);
			points.emplace_back(x, height - h);
		}

		QPainterPath ridge;
		if (!points.empty()) {
			ridge.moveTo(points.front());
			for (size_t i = 0; i + 1 < points.size(); ++i) {
				const QPointF& p0 = points[i == 0 ? i : i - 1];
				const QPointF& p1 = points[i];
				const QPointF& p2 = points[i + 1];
				const QPointF& p3 = points[std::min(i + 2, points.size() - 1)];
				const QPointF c1 = p1 + (p2 - p0) / 6.0;
				const QPointF c2 = p2 - (p3 - p1) / 6.0;
				ridge.cubicTo(c1, c2, p2);
			}
		}

		QBrush terrain_brush;
		if (color_mode == AUDIO_VISUALIZATION_COLOR_RAINBOW) {
			QLinearGradient rainbow_fill(0, 0, width, 0);
			set_rainbow_stops(rainbow_fill, base, color_spread_value, style_fill_alpha(style));
			terrain_brush = QBrush(rainbow_fill);
		} else {
			terrain_brush = vertical_style_fill(0, 0, 0, height, palette, style);
		}
		if (points.size() > 1) {
			QImage fill_layer(width, height, QImage::Format_RGBA8888_Premultiplied);
			fill_layer.fill(Qt::transparent);
			QPainter fill_painter(&fill_layer);
			fill_painter.fillRect(fill_layer.rect(), terrain_brush);
			fill_painter.end();

			QImage mask(width, height, QImage::Format_RGBA8888_Premultiplied);
			mask.fill(Qt::transparent);
			QPainter mask_painter(&mask);
			mask_painter.setRenderHint(QPainter::Antialiasing, false);
			mask_painter.setPen(QPen(Qt::white, 1.0f));
			for (int x = 0; x < width; ++x) {
				const float position = x * (points.size() - 1) / static_cast<float>(std::max(1, width - 1));
				const int index = clampi(static_cast<int>(std::floor(position)), 0, static_cast<int>(points.size()) - 2);
				const float mix = position - index;
				const float y = points[index].y() * (1.0f - mix) + points[index + 1].y() * mix;
				mask_painter.drawLine(QPointF(x + 0.5f, y), QPointF(x + 0.5f, height));
			}
			mask_painter.end();

			fill_painter.begin(&fill_layer);
			fill_painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
			fill_painter.drawImage(0, 0, mask);
			fill_painter.end();
			painter.drawImage(0, 0, fill_layer);
		}
		const float ridge_width = std::max(1.0f, stroke * 0.75f);
		if (color_mode == AUDIO_VISUALIZATION_COLOR_RAINBOW) {
			QLinearGradient glow_grad(0, 0, width, 0);
			set_rainbow_stops(glow_grad, base, color_spread_value, 0.45f);
			if (styled_glow > 0.01f) {
				painter.setPen(QPen(QBrush(glow_grad), ridge_width + styled_glow * 10.0f, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
				painter.drawPath(ridge);
			}
			QLinearGradient ridge_grad(0, 0, width, 0);
			set_rainbow_stops(ridge_grad, base, color_spread_value);
			painter.setPen(QPen(QBrush(ridge_grad), ridge_width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
			painter.drawPath(ridge);
		} else {
			draw_glow_path(painter, ridge, palette, ridge_width, styled_glow * 0.55f);
		}
	} else if (mode == AUDIO_VISUALIZATION_RADIAL) {
		const int segments = clampi(std::lround(48 + detail_value * 192), 24, 256);
		const std::vector<float> bins = spectrum_bins(frame, segments, gain, smoothing_value, low_hz, high_hz);
		const QPointF center(width * 0.5, height * 0.5);
		const float radius = std::min(width, height) * 0.24f;
		const float spike = std::min(width, height) * 0.24f;
		QPainterPath ring;
		for (int i = 0; i <= segments; ++i) {
			const int idx = i % segments;
			const double angle = -PI * 0.5 + (2.0 * PI * i / segments);
			const float r = radius + bins[idx] * spike;
			const QPointF p(center.x() + std::cos(angle) * r, center.y() + std::sin(angle) * r);
			if (i == 0)
				ring.moveTo(p);
			else
				ring.lineTo(p);
		}
		QConicalGradient grad(center, -90);
		if (color_mode == AUDIO_VISUALIZATION_COLOR_RAINBOW) {
			set_rainbow_stops(grad, base, color_spread_value);
		} else {
			grad.setColorAt(0.0, palette.base);
			grad.setColorAt(0.45, palette.light);
			grad.setColorAt(1.0, palette.accent);
		}
		if (styled_glow > 0.01f) {
			QBrush glow_brush(palette.glow);
			if (color_mode == AUDIO_VISUALIZATION_COLOR_RAINBOW) {
				QConicalGradient glow_grad(center, -90);
				set_rainbow_stops(glow_grad, base, color_spread_value, 0.48f);
				glow_brush = QBrush(glow_grad);
			}
			QPen glow_pen(glow_brush, stroke + styled_glow * 16.0f, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
			painter.setPen(glow_pen);
			painter.drawPath(ring);
		}
		painter.setPen(QPen(QBrush(grad), stroke + 1.0f, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
		painter.drawPath(ring);
	} else if (mode == AUDIO_VISUALIZATION_RADIAL_BARS) {
		const int bars = clampi(std::lround(36 + detail_value * 156), 24, 220);
		const std::vector<float> bins = spectrum_bins(frame, bars, gain, smoothing_value, low_hz, high_hz);
		const QPointF center(width * 0.5, height * 0.5);
		const float min_dimension = std::min(width, height);
		const float inner_radius = min_dimension * 0.22f;
		const float max_length = min_dimension * 0.28f;
		const float bar_width = std::max(1.0f, static_cast<float>((2.0 * PI * inner_radius / bars) * 0.55));
		for (int i = 0; i < bars; ++i) {
			const double angle = -PI * 0.5 + (2.0 * PI * i / bars);
			const float length = std::max(min_dimension * 0.012f, bins[i] * max_length);
			const QPointF start(center.x() + std::cos(angle) * inner_radius, center.y() + std::sin(angle) * inner_radius);
			const QPointF end(center.x() + std::cos(angle) * (inner_radius + length), center.y() + std::sin(angle) * (inner_radius + length));
			QColor c = color_mode == AUDIO_VISUALIZATION_COLOR_RAINBOW
				? rainbow_color(base, i / static_cast<float>(std::max(1, bars - 1)), color_spread_value)
				: mix_color(palette.base, palette.light, bins[i] * 0.35f);
			if (styled_glow > 0.01f) {
				const QColor glow_color = color_mode == AUDIO_VISUALIZATION_COLOR_RAINBOW ? alpha_color(c, 0.58f) : alpha_color(palette.glow, 0.65f);
				painter.setPen(QPen(glow_color, bar_width + styled_glow * 8.0f, Qt::SolidLine, Qt::RoundCap));
				painter.drawLine(start, end);
			}
			painter.setPen(QPen(c, bar_width, Qt::SolidLine, Qt::RoundCap));
			painter.drawLine(start, end);
		}
	} else if (mode == AUDIO_VISUALIZATION_PHASE_SCOPE) {
		if (channels < 2) {
			visualization_type = AUDIO_VISUALIZATION_WAVEFORM;
			auto result = GetFrame(frame, frame_number);
			visualization_type = mode;
			return result;
		}
		painter.setRenderHint(QPainter::Antialiasing, false);
		const int count = clampi(std::lround(96 + detail_value * 224), 64, 320);
		const int samples = frame->GetAudioSamplesCount();
		const float *left = frame->GetAudioSampleBuffer()->getReadPointer(0);
		const float *right = frame->GetAudioSampleBuffer()->getReadPointer(1);
		QPolygonF trace;
		trace.reserve(count);
		float previous_x = width * 0.5f;
		float previous_y = height * 0.5f;
		for (int i = 0; i < count; ++i) {
			const int sample = clampi((i * samples) / count, 0, samples - 1);
			const float raw_x = width * 0.5f + clampf((left[sample] - right[sample]) * gain * 0.75f, -1.0f, 1.0f) * width * 0.38f;
			const float raw_y = height * 0.5f - clampf((left[sample] + right[sample]) * gain * 0.38f, -1.0f, 1.0f) * height * 0.42f;
			const float x = previous_x * 0.72f + raw_x * 0.28f;
			const float y = previous_y * 0.72f + raw_y * 0.28f;
			trace.append(QPointF(x, y));
			previous_x = x;
			previous_y = y;
		}
		painter.setPen(QPen(alpha_color(palette.dark, 0.35f), 1.0f));
		painter.drawLine(QPointF(width * 0.5f, height * 0.12f), QPointF(width * 0.5f, height * 0.88f));
		painter.drawLine(QPointF(width * 0.12f, height * 0.5f), QPointF(width * 0.88f, height * 0.5f));
		const float trace_width = std::max(1.0f, stroke);
		if (color_mode == AUDIO_VISUALIZATION_COLOR_RAINBOW) {
			QRectF rainbow_bounds = trace.boundingRect();
			const qreal min_span = std::min(width, height) * 0.22;
			if (rainbow_bounds.width() < min_span)
				rainbow_bounds.adjust((rainbow_bounds.width() - min_span) * 0.5, 0.0, (min_span - rainbow_bounds.width()) * 0.5, 0.0);
			if (rainbow_bounds.height() < min_span)
				rainbow_bounds.adjust(0.0, (rainbow_bounds.height() - min_span) * 0.5, 0.0, (min_span - rainbow_bounds.height()) * 0.5);
			rainbow_bounds = rainbow_bounds.intersected(QRectF(0, 0, width, height));
			if (styled_glow > 0.01f) {
				QLinearGradient glow_grad(rainbow_bounds.left(), rainbow_bounds.center().y(), rainbow_bounds.right(), rainbow_bounds.center().y());
				set_rainbow_stops(glow_grad, base, color_spread_value, 0.42f);
				painter.setPen(QPen(QBrush(glow_grad), trace_width + styled_glow * 10.0f, Qt::SolidLine, Qt::FlatCap, Qt::BevelJoin));
				painter.drawPolyline(trace);
			}
			QLinearGradient trace_grad(rainbow_bounds.left(), rainbow_bounds.center().y(), rainbow_bounds.right(), rainbow_bounds.center().y());
			set_rainbow_stops(trace_grad, base, color_spread_value);
			painter.setPen(QPen(QBrush(trace_grad), trace_width, Qt::SolidLine, Qt::FlatCap, Qt::BevelJoin));
			painter.drawPolyline(trace);
		} else {
			if (styled_glow > 0.01f) {
				QPen glow_pen(palette.glow, trace_width + styled_glow * 10.0f, Qt::SolidLine, Qt::FlatCap, Qt::BevelJoin);
				painter.setPen(glow_pen);
				painter.drawPolyline(trace);
			}
			QPen pen(palette.base, trace_width, Qt::SolidLine, Qt::FlatCap, Qt::BevelJoin);
			painter.setPen(pen);
			painter.drawPolyline(trace);
		}
	} else if (mode == AUDIO_VISUALIZATION_PARTICLES) {
		const int count = clampi(std::lround(220 + detail_value * 920), 160, 1280);
		const float level = reactive_level(frame, -1, gain);
		const std::vector<float> bands = spectrum_bins(frame, 36, gain, smoothing_value, low_hz, high_hz);
		const float low = band_level(bands, 0.0f, 0.18f);
		const float mid = band_level(bands, 0.18f, 0.56f);
		const float high = band_level(bands, 0.56f, 1.0f);
		const float time = frame_number * (0.028f + level * 0.075f);
		const QPointF center(width * 0.5f, height * (0.55f - level * 0.08f));
		const float max_radius = std::sqrt(width * width + height * height) * 0.42f;
		const float swirl = 0.65f + low * 2.4f;
		const float twist = 0.45f + mid * 2.1f;
		const float sparkle = 0.2f + high * 1.8f;
		std::uniform_real_distribution<float> dist(0.0f, 1.0f);
		for (int i = 0; i < count; ++i) {
			std::mt19937 rng(static_cast<uint32_t>((i + 1) * 747796405U));
			const float seed = dist(rng);
			const float arm = std::floor(dist(rng) * 5.0f);
			const float age = std::fmod(seed + time * (0.18f + dist(rng) * 0.62f + level * 0.55f), 1.0f);
			const float eased_age = std::pow(age, 0.58f);
			const float base_angle = (arm / 5.0f) * 2.0f * PI + dist(rng) * 0.42f;
			const float orbit = base_angle + eased_age * swirl * PI + std::sin(time + seed * 12.0f) * twist * 0.22f;
			const float wave = std::sin(eased_age * PI * (2.0f + sparkle) + seed * 10.0f);
			const float radius = (0.06f + eased_age * (0.88f + low * 0.35f)) * max_radius * (0.62f + level * 0.74f);
			const float ribbon = wave * (height * 0.018f + mid * height * 0.055f);
			const QPointF direction(std::cos(orbit), std::sin(orbit));
			const QPointF normal(-direction.y(), direction.x());
			const QPointF p(
				center.x() + direction.x() * radius + normal.x() * ribbon,
				center.y() + direction.y() * radius + normal.y() * ribbon);
			const float trail_length = 16.0f + low * 58.0f + level * 64.0f;
			const QPointF tail(
				p.x() - direction.x() * trail_length - normal.x() * ribbon * 0.35f,
				p.y() - direction.y() * trail_length - normal.y() * ribbon * 0.35f);
			const float fade = std::sin(age * PI);
			const float size = 0.42f + fade * (0.95f + high * 2.25f + level * 2.0f);
			QColor c = color_mode == AUDIO_VISUALIZATION_COLOR_RAINBOW
				? rainbow_color(base, std::fmod(age + arm / 5.0f, 1.0f), color_spread_value)
				: mix_color(palette.base, palette.light, fade * (0.25f + color_spread_value * 0.35f));
			c = alpha_color(c, (0.12f + level * 0.52f + high * 0.22f) * fade);
			QPen trail_pen(alpha_color(c, 0.26f + mid * 0.16f), std::max(0.45f, size * (0.28f + low * 0.12f)), Qt::SolidLine, Qt::RoundCap);
			painter.setPen(trail_pen);
			painter.drawLine(tail, p);
			if (style == AUDIO_VISUALIZATION_STYLE_MINIMAL) {
				painter.setBrush(alpha_color(c, 1.0f));
			} else {
				QRadialGradient grad(p, size * (2.2f + styled_glow * 4.0f));
				grad.setColorAt(0.0, c);
				grad.setColorAt(1.0, alpha_color(c, 0.0f));
				painter.setBrush(grad);
			}
			painter.setPen(Qt::NoPen);
			painter.drawEllipse(p, size * 2.0f, size * 2.0f);
		}
	} else if (mode == AUDIO_VISUALIZATION_VU_METER) {
		const int meters = split ? std::min(channels, 8) : 1;
		const int segments = clampi(std::lround(8 + detail_value * 32), 6, 48);
		for (int meter = 0; meter < meters; ++meter) {
			const float level = reactive_level(frame, split ? meter : -1, gain);
			const float lane_width = width / static_cast<float>(meters);
			const float x0 = meter * lane_width + lane_width * 0.08f;
			const float seg_gap = std::max(1.0f, height * 0.01f);
			const float seg_h = (height * 0.88f - seg_gap * (segments - 1)) / segments;
			for (int segment = 0; segment < segments; ++segment) {
				const float t = (segment + 1) / static_cast<float>(segments);
				const bool on = t <= level;
				QColor c;
				if (style == AUDIO_VISUALIZATION_STYLE_MINIMAL) {
					c = palette.base;
				} else {
					c = t > 0.82f ? QColor(255, 86, 68, base.alpha()) : (t > 0.55f ? mix_color(palette.accent, QColor(255, 214, 90, base.alpha()), (t - 0.55f) / 0.27f) : mix_color(palette.base, palette.light, t * 1.5f));
				}
				if (!on)
					c = alpha_color(palette.dark, 0.25f);
				const float y = height * 0.94f - (segment + 1) * seg_h - segment * seg_gap;
				painter.fillRect(QRectF(x0, y, lane_width * 0.84f, seg_h), c);
			}
		}
	}

	painter.end();
	frame->AddImage(visual);
	return frame;
}

std::string AudioVisualization::Json() const {
	return JsonValue().toStyledString();
}

Json::Value AudioVisualization::JsonValue() const {
	Json::Value root = EffectBase::JsonValue();
	root["type"] = info.class_name;
	root["visualization_type"] = visualization_type;
	root["style"] = style;
	root["color"] = color.JsonValue();
	root["intensity"] = intensity.JsonValue();
	root["smoothing"] = smoothing.JsonValue();
	root["detail"] = detail.JsonValue();
	root["glow"] = glow.JsonValue();
	root["color_spread"] = color_spread.JsonValue();
	root["color_mode"] = color_mode;
	root["channel_layout"] = channel_layout;
	root["frequency_low"] = frequency_low.JsonValue();
	root["frequency_high"] = frequency_high.JsonValue();
	root["background"] = background;
	return root;
}

void AudioVisualization::SetJson(const std::string value) {
	try
	{
		const Json::Value root = openshot::stringToJson(value);
		SetJsonValue(root);
	}
	catch (const std::exception& e)
	{
		throw InvalidJSON("JSON is invalid (missing keys or invalid data types)");
	}
}

void AudioVisualization::SetJsonValue(const Json::Value root) {
	EffectBase::SetJsonValue(root);
	if (!root["visualization_type"].isNull())
		visualization_type = root["visualization_type"].asInt();
	if (!root["style"].isNull())
		style = root["style"].asInt();
	if (!root["color"].isNull())
		color.SetJsonValue(root["color"]);
	if (!root["intensity"].isNull())
		intensity.SetJsonValue(root["intensity"]);
	if (!root["smoothing"].isNull())
		smoothing.SetJsonValue(root["smoothing"]);
	if (!root["detail"].isNull())
		detail.SetJsonValue(root["detail"]);
	if (!root["glow"].isNull())
		glow.SetJsonValue(root["glow"]);
	if (!root["color_spread"].isNull())
		color_spread.SetJsonValue(root["color_spread"]);
	if (!root["color_mode"].isNull())
		color_mode = root["color_mode"].asInt();
	if (!root["channel_layout"].isNull())
		channel_layout = root["channel_layout"].asInt();
	if (!root["frequency_low"].isNull())
		frequency_low.SetJsonValue(root["frequency_low"]);
	if (!root["frequency_high"].isNull())
		frequency_high.SetJsonValue(root["frequency_high"]);
	if (!root["background"].isNull())
		background = root["background"].asInt();
}

std::string AudioVisualization::PropertiesJSON(int64_t requested_frame) const {
	Json::Value root = BasePropertiesJSON(requested_frame);

	root["visualization_type"] = add_property_json("Visualization", visualization_type, "int", "", NULL, 0, AUDIO_VISUALIZATION_RADIAL_BARS, false, requested_frame);
	root["visualization_type"]["choices"].append(add_property_choice_json("Waveform", AUDIO_VISUALIZATION_WAVEFORM, visualization_type));
	root["visualization_type"]["choices"].append(add_property_choice_json("Filled Waveform", AUDIO_VISUALIZATION_FILLED_WAVEFORM, visualization_type));
	root["visualization_type"]["choices"].append(add_property_choice_json("Bars", AUDIO_VISUALIZATION_BARS, visualization_type));
	root["visualization_type"]["choices"].append(add_property_choice_json("Radial", AUDIO_VISUALIZATION_RADIAL, visualization_type));
	root["visualization_type"]["choices"].append(add_property_choice_json("Radial Bars", AUDIO_VISUALIZATION_RADIAL_BARS, visualization_type));
	root["visualization_type"]["choices"].append(add_property_choice_json("Spectrum", AUDIO_VISUALIZATION_SPECTRUM, visualization_type));
	root["visualization_type"]["choices"].append(add_property_choice_json("Phase Scope", AUDIO_VISUALIZATION_PHASE_SCOPE, visualization_type));
	root["visualization_type"]["choices"].append(add_property_choice_json("Particles", AUDIO_VISUALIZATION_PARTICLES, visualization_type));
	root["visualization_type"]["choices"].append(add_property_choice_json("VU Meter", AUDIO_VISUALIZATION_VU_METER, visualization_type));

	root["style"] = add_property_json("Style", style, "int", "", NULL, 0, AUDIO_VISUALIZATION_STYLE_MINIMAL, false, requested_frame);
	root["style"]["choices"].append(add_property_choice_json("Clean", AUDIO_VISUALIZATION_STYLE_CLEAN, style));
	root["style"]["choices"].append(add_property_choice_json("Soft", AUDIO_VISUALIZATION_STYLE_SOFT, style));
	root["style"]["choices"].append(add_property_choice_json("Neon", AUDIO_VISUALIZATION_STYLE_NEON, style));
	root["style"]["choices"].append(add_property_choice_json("Minimal", AUDIO_VISUALIZATION_STYLE_MINIMAL, style));

	root["color"] = add_property_json("Color", 0.0, "color", "", &color.red, 0, 255, false, requested_frame);
	root["color"]["red"] = add_property_json("Red", color.red.GetValue(requested_frame), "float", "", &color.red, 0, 255, false, requested_frame);
	root["color"]["blue"] = add_property_json("Blue", color.blue.GetValue(requested_frame), "float", "", &color.blue, 0, 255, false, requested_frame);
	root["color"]["green"] = add_property_json("Green", color.green.GetValue(requested_frame), "float", "", &color.green, 0, 255, false, requested_frame);
	root["color"]["alpha"] = add_property_json("Alpha", color.alpha.GetValue(requested_frame), "float", "", &color.alpha, 0, 255, false, requested_frame);

	root["intensity"] = add_property_json("Intensity", intensity.GetValue(requested_frame), "float", "", &intensity, 0.0, 10.0, false, requested_frame);
	root["smoothing"] = add_property_json("Smoothing", smoothing.GetValue(requested_frame), "float", "", &smoothing, 0.0, 1.0, false, requested_frame);
	root["detail"] = add_property_json("Detail", detail.GetValue(requested_frame), "float", "", &detail, 0.0, 1.0, false, requested_frame);
	root["glow"] = add_property_json("Glow", glow.GetValue(requested_frame), "float", "", &glow, 0.0, 1.0, false, requested_frame);
	root["color_spread"] = add_property_json("Color Spread", color_spread.GetValue(requested_frame), "float", "", &color_spread, 0.0, 1.0, false, requested_frame);

	root["color_mode"] = add_property_json("Color Mode", color_mode, "int", "", NULL, 0, AUDIO_VISUALIZATION_COLOR_RAINBOW, false, requested_frame);
	root["color_mode"]["choices"].append(add_property_choice_json("Seed", AUDIO_VISUALIZATION_COLOR_SEED, color_mode));
	root["color_mode"]["choices"].append(add_property_choice_json("Rainbow", AUDIO_VISUALIZATION_COLOR_RAINBOW, color_mode));

	root["channel_layout"] = add_property_json("Channel Layout", channel_layout, "int", "", NULL, 0, AUDIO_VISUALIZATION_CHANNEL_OVERLAY, false, requested_frame);
	root["channel_layout"]["choices"].append(add_property_choice_json("Auto", AUDIO_VISUALIZATION_CHANNEL_AUTO, channel_layout));
	root["channel_layout"]["choices"].append(add_property_choice_json("Combined", AUDIO_VISUALIZATION_CHANNEL_COMBINED, channel_layout));
	root["channel_layout"]["choices"].append(add_property_choice_json("Split", AUDIO_VISUALIZATION_CHANNEL_SPLIT, channel_layout));
	root["channel_layout"]["choices"].append(add_property_choice_json("Overlay", AUDIO_VISUALIZATION_CHANNEL_OVERLAY, channel_layout));

	root["frequency_low"] = add_property_json("Low Frequency", hz_to_normalized_frequency(frequency_low.GetValue(requested_frame)), "float", "Normalized frequency floor: 0 = 20 Hz, 1 = 20 kHz", &frequency_low, 0.0, 1.0, false, requested_frame);
	root["frequency_high"] = add_property_json("High Frequency", hz_to_normalized_frequency(frequency_high.GetValue(requested_frame)), "float", "Normalized frequency ceiling: 0 = 20 Hz, 1 = 20 kHz", &frequency_high, 0.0, 1.0, false, requested_frame);

	root["background"] = add_property_json("Background", background, "int", "", NULL, 0, AUDIO_VISUALIZATION_BACKGROUND_SOURCE, false, requested_frame);
	root["background"]["choices"].append(add_property_choice_json("Transparent", AUDIO_VISUALIZATION_BACKGROUND_TRANSPARENT, background));
	root["background"]["choices"].append(add_property_choice_json("Solid", AUDIO_VISUALIZATION_BACKGROUND_SOLID, background));
	root["background"]["choices"].append(add_property_choice_json("Fade", AUDIO_VISUALIZATION_BACKGROUND_FADE, background));
	root["background"]["choices"].append(add_property_choice_json("Gradient", AUDIO_VISUALIZATION_BACKGROUND_GRADIENT, background));
	root["background"]["choices"].append(add_property_choice_json("Source", AUDIO_VISUALIZATION_BACKGROUND_SOURCE, background));

	return root.toStyledString();
}

/**
 * @file
 * @brief Header file for FrameScope class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_FRAMESCOPE_H
#define OPENSHOT_FRAMESCOPE_H

#include "Frame.h"
#include "Json.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace openshot {

	/**
	 * @brief Analyze a single Frame and expose scope-friendly JSON.
	 *
	 * FrameScope provides a lightweight analysis layer for the current preview
	 * frame. It intentionally focuses on broadly useful data for grading and
	 * editorial decisions, such as histograms, a luma waveform, audio envelope
	 * buckets, and simple clipping / peak summaries.
	 *
	 * Pixel format: libopenshot frames are always stored as
	 * QImage::Format_RGBA8888_Premultiplied (see Frame::AddImage). The
	 * in-memory byte order per pixel is [R=0, G=1, B=2, A=3].
	 */
	class FrameScope {
	private:
		std::shared_ptr<Frame> frame;
		int waveform_columns;
		int audio_buckets;
		int vectorscope_size;
		bool roi_enabled;
		float roi_x;
		float roi_y;
		float roi_width;
		float roi_height;

		bool video_present;
		int video_width;
		int video_height;
		int waveform_bins;
		std::vector<int> waveform_column_map;
		std::vector<size_t> waveform_offset_map;  // pre-multiplied: waveform_column_map[x] * waveform_bins
		int waveform_column_map_width;
		int waveform_column_map_columns;
		double avg_luma;
		int clipped_shadows;
		int clipped_highlights;
		int clipped_red;
		int clipped_green;
		int clipped_blue;
		std::vector<uint32_t> histogram_luma;
		std::vector<uint32_t> histogram_red;
		std::vector<uint32_t> histogram_green;
		std::vector<uint32_t> histogram_blue;
		std::vector<uint32_t> waveform_luma;
		std::vector<uint32_t> waveform_red;
		std::vector<uint32_t> waveform_green;
		std::vector<uint32_t> waveform_blue;
		std::vector<uint32_t> vectorscope;

		bool audio_present;
		int audio_channels;
		int audio_samples;
		int audio_sample_rate;
		std::vector<float> audio_peak;
		std::vector<float> audio_rms;
		std::vector<uint32_t> audio_clipped_samples;
		std::vector<std::vector<float>> audio_waveform_min;
		std::vector<std::vector<float>> audio_waveform_max;
		mutable Json::Value scope_data;
		mutable bool json_dirty;

		void reset();
		void reset_video();
		void reset_audio();
		void ensure_video_buffers();
		void ensure_audio_buffers();
		void rebuild_waveform_column_map(int width);
		void analyze_video();
		void analyze_audio();
		void analyze();
		void rebuild_json() const;
		static std::vector<int> copy_to_int_vector(const std::vector<uint32_t>& values);

	public:
		/// Create an empty scope analyzer with default bucket sizes.
		FrameScope();

		/// Construct and immediately analyze a frame.
		FrameScope(std::shared_ptr<Frame> frame, int waveform_columns = 256, int audio_buckets = 256, int vectorscope_size = 256);

		/// Replace the current frame and recompute the scope data.
		void SetFrame(std::shared_ptr<Frame> new_frame);

		/// Return the currently analyzed frame.
		std::shared_ptr<Frame> GetFrame() const { return frame; }

		/// Set the number of horizontal waveform columns and re-analyze.
		void SetWaveformColumns(int columns);

		/// Set the number of audio buckets and re-analyze.
		void SetAudioBuckets(int buckets);

		/// Set the vectorscope plane edge length and re-analyze video.
		void SetVectorscopeSize(int size);

		/// Set a normalized ROI for video analysis and re-analyze video.
		void SetVideoRegionNormalized(float x, float y, float width, float height);

		/// Clear any video ROI and re-analyze the full frame.
		void ClearVideoRegion();

		/// Return whether a video ROI is enabled.
		bool HasVideoRegion() const { return roi_enabled; }

		/// Return whether the current frame has analyzable video data.
		bool HasVideo() const { return video_present; }

		/// Return whether the current frame has analyzable audio data.
		bool HasAudio() const { return audio_present; }

		/// Return the analyzed video width.
		int GetVideoWidth() const { return video_width; }

		/// Return the analyzed video height.
		int GetVideoHeight() const { return video_height; }

		/// Return the number of waveform columns.
		int GetWaveformColumns() const { return waveform_columns; }

		/// Return the number of vertical waveform bins.
		int GetWaveformBins() const { return waveform_bins; }

		/// Return the vectorscope plane edge length.
		int GetVectorscopeSize() const { return vectorscope_size; }

		/// Return the luma histogram bins by reference.
		const std::vector<uint32_t>& GetVideoHistogramLumaBins() const { return histogram_luma; }

		/// Return the red histogram bins by reference.
		const std::vector<uint32_t>& GetVideoHistogramRedBins() const { return histogram_red; }

		/// Return the green histogram bins by reference.
		const std::vector<uint32_t>& GetVideoHistogramGreenBins() const { return histogram_green; }

		/// Return the blue histogram bins by reference.
		const std::vector<uint32_t>& GetVideoHistogramBlueBins() const { return histogram_blue; }

		/// Return the flattened luma waveform bins by reference.
		const std::vector<uint32_t>& GetVideoWaveformLumaBins() const { return waveform_luma; }

		/// Return the flattened red waveform bins by reference.
		const std::vector<uint32_t>& GetVideoWaveformRedBins() const { return waveform_red; }

		/// Return the flattened green waveform bins by reference.
		const std::vector<uint32_t>& GetVideoWaveformGreenBins() const { return waveform_green; }

		/// Return the flattened blue waveform bins by reference.
		const std::vector<uint32_t>& GetVideoWaveformBlueBins() const { return waveform_blue; }

		/// Return the flattened vectorscope density plane by reference.
		const std::vector<uint32_t>& GetVideoVectorscopeBins() const { return vectorscope; }

		/// Return the flattened vectorscope density plane.
		std::vector<int> GetVideoVectorscope() const { return copy_to_int_vector(vectorscope); }

		/// Return the luma histogram bins.
		std::vector<int> GetVideoHistogramLuma() const { return copy_to_int_vector(histogram_luma); }

		/// Return the red histogram bins.
		std::vector<int> GetVideoHistogramRed() const { return copy_to_int_vector(histogram_red); }

		/// Return the green histogram bins.
		std::vector<int> GetVideoHistogramGreen() const { return copy_to_int_vector(histogram_green); }

		/// Return the blue histogram bins.
		std::vector<int> GetVideoHistogramBlue() const { return copy_to_int_vector(histogram_blue); }

		/// Return the flattened luma waveform bins.
		std::vector<int> GetVideoWaveformLuma() const { return copy_to_int_vector(waveform_luma); }

		/// Return the flattened red waveform bins.
		std::vector<int> GetVideoWaveformRed() const { return copy_to_int_vector(waveform_red); }

		/// Return the flattened green waveform bins.
		std::vector<int> GetVideoWaveformGreen() const { return copy_to_int_vector(waveform_green); }

		/// Return the flattened blue waveform bins.
		std::vector<int> GetVideoWaveformBlue() const { return copy_to_int_vector(waveform_blue); }

		/// Return the average luma of the analyzed frame.
		double GetVideoAverageLuma() const { return avg_luma; }

		/// Return the clipped shadow pixel count.
		int GetVideoClippedShadows() const { return clipped_shadows; }

		/// Return the clipped highlight pixel count.
		int GetVideoClippedHighlights() const { return clipped_highlights; }

		/// Return the clipped red-channel pixel count.
		int GetVideoClippedRed() const { return clipped_red; }

		/// Return the clipped green-channel pixel count.
		int GetVideoClippedGreen() const { return clipped_green; }

		/// Return the clipped blue-channel pixel count.
		int GetVideoClippedBlue() const { return clipped_blue; }

		/// Return the number of analyzed audio channels.
		int GetAudioChannels() const { return audio_channels; }

		/// Return the number of analyzed audio samples.
		int GetAudioSamples() const { return audio_samples; }

		/// Return the analyzed audio sample rate.
		int GetAudioSampleRate() const { return audio_sample_rate; }

		/// Return the number of audio waveform buckets.
		int GetAudioBuckets() const { return audio_buckets; }

		/// Return per-channel peak levels.
		const std::vector<float>& GetAudioPeakLevelsRef() const { return audio_peak; }

		/// Return per-channel RMS levels.
		const std::vector<float>& GetAudioRmsLevelsRef() const { return audio_rms; }

		/// Return per-channel clipped sample counts.
		const std::vector<uint32_t>& GetAudioClippedSamplesRef() const { return audio_clipped_samples; }

		/// Return per-channel peak levels.
		std::vector<float> GetAudioPeakLevels() const { return audio_peak; }

		/// Return per-channel RMS levels.
		std::vector<float> GetAudioRmsLevels() const { return audio_rms; }

		/// Return per-channel clipped sample counts.
		std::vector<int> GetAudioClippedSamples() const { return copy_to_int_vector(audio_clipped_samples); }

		/// Return one channel of audio waveform minimum values.
		std::vector<float> GetAudioWaveformMin(int channel) const;

		/// Return one channel of audio waveform maximum values.
		std::vector<float> GetAudioWaveformMax(int channel) const;

		/// Return the current scope payload as a Json::Value tree.
		Json::Value JsonValue() const;

		/// Return the current scope payload as a JSON string.
		std::string Json() const;
	};

}

#endif

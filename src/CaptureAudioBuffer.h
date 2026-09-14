/** @file @brief Timestamp-positioned audio for live capture. */
// Copyright (c) 2008-2026 OpenShot Studios, LLC
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_CAPTURE_AUDIO_BUFFER_H
#define OPENSHOT_CAPTURE_AUDIO_BUFFER_H

#include <algorithm>
#include <cstdint>
#include <deque>
#include <utility>
#include <vector>

extern "C" {
#include <libavutil/mathematics.h>
}

namespace openshot {
// Positions are sample frames relative to recording start. Callers synchronize
// access. Delivery delays and dropped packets must never shift later samples.
class CaptureAudioBuffer {
public:
	void Reset(int channels, int64_t capacity)
	{
		channel_count = channels;
		max_samples = capacity;
		position = 0;
		queued_samples = 0;
		timestamp_started = false;
		next_timestamp_sample = 0;
		blocks.clear();
	}

	void Push(int64_t start, std::vector<std::vector<float>> samples)
	{
		if (samples.size() != static_cast<size_t>(channel_count) || samples.empty()) return;
		const int64_t count = samples.front().size();
		for (const auto& channel : samples) {
			if (channel.size() != static_cast<size_t>(count)) return;
		}
		if (!count || start + count <= position) return;
		// Trim samples from before recording start, already-written intervals,
		// or a single packet larger than the bounded capture buffer.
		const int64_t trim = std::max<int64_t>({0, position - start, count - max_samples});
		for (auto& channel : samples) channel.erase(channel.begin(), channel.begin() + trim);
		start += trim;
		queued_samples += count - trim;
		const auto insertion = std::upper_bound(blocks.begin(), blocks.end(), start,
			[](int64_t value, const Block& block) { return value < block.start; });
		blocks.insert(insertion, Block{start, std::move(samples)});
		while (queued_samples > max_samples && !blocks.empty()) {
			queued_samples -= blocks.front().samples.front().size();
			blocks.pop_front();
		}
	}

	void PushTimestamp(int64_t timestamp, int64_t epoch, AVRational time_base,
		int sample_rate, std::vector<std::vector<float>> samples)
	{
		if (samples.empty() || samples.front().empty()) return;
		int64_t start = av_rescale_q(timestamp - epoch, time_base, AVRational{1, sample_rate});
		// Capture timestamps include clock-estimation jitter. Placing every
		// packet independently can insert silence or discard PCM at each seam.
		// Keep adjacent packets sample-contiguous within one millisecond of
		// their absolute capture position. Compare against the accumulated end,
		// not the previous timestamp, so tolerance cannot accumulate into drift.
		const int64_t tolerance = std::max(1, sample_rate / 1000);
		if (timestamp_started && start >= next_timestamp_sample - tolerance
			&& start <= next_timestamp_sample + tolerance) {
			start = next_timestamp_sample;
		}
		next_timestamp_sample = start + static_cast<int64_t>(samples.front().size());
		timestamp_started = true;
		Push(start, std::move(samples));
	}

	bool Covers(int count) const
	{
		return std::any_of(blocks.begin(), blocks.end(), [&](const Block& block) {
			return block.start + static_cast<int64_t>(block.samples.front().size()) >= position + count;
		});
	}

	std::vector<std::vector<float>> Read(int count)
	{
		std::vector<std::vector<float>> result(channel_count, std::vector<float>(count, 0.0f));
		const int64_t end = position + count;
		for (const auto& block : blocks) {
			const int64_t first = std::max(position, block.start);
			const int64_t last = std::min(end, block.start + static_cast<int64_t>(block.samples.front().size()));
			if (last <= first) continue;
			for (int channel = 0; channel < channel_count; ++channel) {
				std::copy_n(block.samples[channel].begin() + (first - block.start), last - first,
					result[channel].begin() + (first - position));
			}
		}
		position = end;
		for (auto it = blocks.begin(); it != blocks.end();) {
			if (it->start + static_cast<int64_t>(it->samples.front().size()) <= position) {
				queued_samples -= it->samples.front().size();
				it = blocks.erase(it);
			} else ++it;
		}
		return result;
	}

private:
	struct Block {
		int64_t start;
		std::vector<std::vector<float>> samples;
	};
	std::deque<Block> blocks;
	int channel_count = 0;
	int64_t max_samples = 0;
	int64_t position = 0;
	int64_t queued_samples = 0;
	bool timestamp_started = false;
	int64_t next_timestamp_sample = 0;
};
}
#endif

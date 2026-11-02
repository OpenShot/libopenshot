#ifndef OPENSHOT_PITCH_PROCESSOR_H
#define OPENSHOT_PITCH_PROCESSOR_H

#include <cstddef>
#include <memory>
#include <vector>

#ifdef OPENSHOT_HAVE_RUBBERBAND
#include <rubberband/RubberBandStretcher.h>
#endif

namespace openshot {

class PitchProcessor {
public:
    PitchProcessor(int sample_rate, int channels);
    ~PitchProcessor();

    PitchProcessor(const PitchProcessor&) = delete;
    PitchProcessor& operator=(const PitchProcessor&) = delete;

    // Reset the processor after a seek or playback discontinuity.
    void Reset();

    // Configure the output duration and pitch independently.
    // time_ratio = output duration / input duration.
    // pitch_semitones = pitch adjustment relative to input.
    void Configure(double time_ratio, double pitch_semitones);

    // Feed planar floating-point audio samples.
    void Process(const float* const* input,
                 std::size_t sample_count,
                 bool final_block = false);

    // Number of processed samples ready for retrieval.
    std::size_t Available() const;

    // Retrieve up to sample_count samples per channel.
    std::size_t Retrieve(float* const* output,
                         std::size_t sample_count);

    // Input samples recommended before the next processing cycle.
    std::size_t SamplesRequired() const;

    // Real-time startup padding and output delay, in sample frames.
    // Query after Configure() and before processing begins.
    std::size_t PreferredStartPad() const;
    std::size_t StartDelay() const;

    bool IsAvailable() const;

private:
    int sample_rate_;
    int channels_;

#ifdef OPENSHOT_HAVE_RUBBERBAND
    std::unique_ptr<RubberBand::RubberBandStretcher> stretcher_;
#endif
};

} // namespace openshot

#endif

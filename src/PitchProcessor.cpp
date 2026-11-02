#include "PitchProcessor.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace openshot {

PitchProcessor::PitchProcessor(int sample_rate, int channels)
    : sample_rate_(sample_rate),
      channels_(channels)
{
    if (sample_rate_ <= 0 || channels_ <= 0) {
        throw std::invalid_argument(
            "PitchProcessor requires a valid sample rate and channel count");
    }

#ifdef OPENSHOT_HAVE_RUBBERBAND
    using RB = RubberBand::RubberBandStretcher;

    stretcher_ = std::make_unique<RB>(
        static_cast<std::size_t>(sample_rate_),
        static_cast<std::size_t>(channels_),
        RB::OptionProcessRealTime |
        RB::OptionEngineFiner |
        RB::OptionPitchHighQuality,
        1.0,
        1.0
    );
#endif
}

PitchProcessor::~PitchProcessor() = default;

void PitchProcessor::Reset()
{
#ifdef OPENSHOT_HAVE_RUBBERBAND
    stretcher_->reset();
#endif
}

void PitchProcessor::Configure(double time_ratio,
                               double pitch_semitones)
{
    if (!std::isfinite(time_ratio) || time_ratio <= 0.0 ||
        !std::isfinite(pitch_semitones)) {
        throw std::invalid_argument(
            "Invalid pitch or time-stretch parameters");
    }

#ifdef OPENSHOT_HAVE_RUBBERBAND
    const double pitch_scale =
        std::pow(2.0, pitch_semitones / 12.0);

    stretcher_->setTimeRatio(time_ratio);
    stretcher_->setPitchScale(pitch_scale);
#else
    (void)time_ratio;
    (void)pitch_semitones;
#endif
}

void PitchProcessor::Process(const float* const* input,
                             std::size_t sample_count,
                             bool final_block)
{
#ifdef OPENSHOT_HAVE_RUBBERBAND
    if (sample_count > 0 && input == nullptr) {
        throw std::invalid_argument("Null audio input");
    }

    stretcher_->process(input, sample_count, final_block);
#else
    (void)input;
    (void)sample_count;
    (void)final_block;
#endif
}

std::size_t PitchProcessor::Available() const
{
#ifdef OPENSHOT_HAVE_RUBBERBAND
    const int available = stretcher_->available();
    return available > 0
        ? static_cast<std::size_t>(available)
        : 0;
#else
    return 0;
#endif
}

std::size_t PitchProcessor::Retrieve(float* const* output,
                                     std::size_t sample_count)
{
#ifdef OPENSHOT_HAVE_RUBBERBAND
    if (sample_count > 0 && output == nullptr) {
        throw std::invalid_argument("Null audio output");
    }

    return stretcher_->retrieve(output, sample_count);
#else
    (void)output;
    (void)sample_count;
    return 0;
#endif
}

std::size_t PitchProcessor::SamplesRequired() const
{
#ifdef OPENSHOT_HAVE_RUBBERBAND
    return stretcher_->getSamplesRequired();
#else
    return 0;
#endif
}

std::size_t PitchProcessor::PreferredStartPad() const
{
#ifdef OPENSHOT_HAVE_RUBBERBAND
    return stretcher_->getPreferredStartPad();
#else
    return 0;
#endif
}

std::size_t PitchProcessor::StartDelay() const
{
#ifdef OPENSHOT_HAVE_RUBBERBAND
    return stretcher_->getStartDelay();
#else
    return 0;
#endif
}

bool PitchProcessor::IsAvailable() const
{
#ifdef OPENSHOT_HAVE_RUBBERBAND
    return static_cast<bool>(stretcher_);
#else
    return false;
#endif
}

} // namespace openshot

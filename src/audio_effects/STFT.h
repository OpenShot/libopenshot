// © OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#ifndef OPENSHOT_STFT_AUDIO_EFFECT_H
#define OPENSHOT_STFT_AUDIO_EFFECT_H
#define _USE_MATH_DEFINES

#include "EffectBase.h"
#include "Enums.h"

#include <AppConfig.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

namespace openshot
{

    class STFT
    {
    public:
        STFT() : num_channels (1) { }

        virtual ~STFT() { }

        void setup(const int num_input_channels);

        void process(juce::AudioBuffer<float> &block);

        void updateParameters(const int new_fft_size, const int new_overlap, const int new_window_type);

        virtual void updateFftSize(const int new_fft_size);

        virtual void updateHopSize(const int new_overlap);

        virtual void updateWindow(const int new_window_type);

    private:

        virtual void modification(const int channel);

        virtual void analysis(const int channel);

        virtual void synthesis(const int channel);

    protected:
        int num_channels;
        int num_samples;

        int fft_size;
        std::unique_ptr<juce::dsp::FFT> fft;

        int input_buffer_length;
        juce::AudioBuffer<float> input_buffer;

        int output_buffer_length;
        juce::AudioBuffer<float> output_buffer;

        juce::HeapBlock<float> fft_window;
        juce::HeapBlock<juce::dsp::Complex<float>> time_domain_buffer;
        juce::HeapBlock<juce::dsp::Complex<float>> frequency_domain_buffer;

        int overlap;
        int hop_size;
        int window_type;
        float window_scale_factor;

        int input_buffer_write_position;
        int output_buffer_write_position;
        int output_buffer_read_position;
        int samples_since_last_FFT;

        int current_input_buffer_write_position;
        int current_output_buffer_write_position;
        int current_output_buffer_read_position;
        int current_samples_since_last_FFT;
    };
}

#endif

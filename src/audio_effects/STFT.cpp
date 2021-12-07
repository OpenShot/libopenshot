// © OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "STFT.h"

using namespace openshot;

void STFT::setup(const int num_input_channels)
{
    num_channels = (num_input_channels > 0) ? num_input_channels : 1;
}

void STFT::updateParameters(const int new_fft_size, const int new_overlap, const int new_window_type)
{
    updateFftSize(new_fft_size);
    updateHopSize(new_overlap);
    updateWindow(new_window_type);
}

void STFT::process(juce::AudioBuffer<float> &block)
{
    num_samples = block.getNumSamples();

    for (int channel = 0; channel < num_channels; ++channel) {
        float *channel_data = block.getWritePointer(channel);

        current_input_buffer_write_position = input_buffer_write_position;
        current_output_buffer_write_position = output_buffer_write_position;
        current_output_buffer_read_position = output_buffer_read_position;
        current_samples_since_last_FFT = samples_since_last_FFT;

        for (int sample = 0; sample < num_samples; ++sample) {
            const float input_sample = channel_data[sample];

            input_buffer.setSample(channel, current_input_buffer_write_position, input_sample);
            if (++current_input_buffer_write_position >= input_buffer_length)
                current_input_buffer_write_position = 0;
            // diff
            channel_data[sample] = output_buffer.getSample(channel, current_output_buffer_read_position);

            output_buffer.setSample(channel, current_output_buffer_read_position, 0.0f);
            if (++current_output_buffer_read_position >= output_buffer_length)
                current_output_buffer_read_position = 0;

            if (++current_samples_since_last_FFT >= hop_size) {
                current_samples_since_last_FFT = 0;
                analysis(channel);
                modification(channel);
                synthesis(channel);
            }
        }
    }

    input_buffer_write_position = current_input_buffer_write_position;
    output_buffer_write_position = current_output_buffer_write_position;
    output_buffer_read_position = current_output_buffer_read_position;
    samples_since_last_FFT = current_samples_since_last_FFT;
}


void STFT::updateFftSize(const int new_fft_size)
{
    if (new_fft_size != fft_size)
    {
        fft_size = new_fft_size;
        fft = std::make_unique<juce::dsp::FFT>(log2(fft_size));

        input_buffer_length = fft_size;
        input_buffer.clear();
        input_buffer.setSize(num_channels, input_buffer_length);

        output_buffer_length = fft_size;
        output_buffer.clear();
        output_buffer.setSize(num_channels, output_buffer_length);

        fft_window.realloc(fft_size);
        fft_window.clear(fft_size);

        time_domain_buffer.realloc(fft_size);
        time_domain_buffer.clear(fft_size);

        frequency_domain_buffer.realloc(fft_size);
        frequency_domain_buffer.clear(fft_size);

        input_buffer_write_position = 0;
        output_buffer_write_position = 0;
        output_buffer_read_position = 0;
        samples_since_last_FFT = 0;
    }
}

void STFT::updateHopSize(const int new_overlap)
{
    if (new_overlap != overlap)
    {
        overlap = new_overlap;

        if (overlap != 0) {
            hop_size = fft_size / overlap;
            output_buffer_write_position = hop_size % output_buffer_length;
        }
    }
}


void STFT::updateWindow(const int new_window_type)
{
    window_type = new_window_type;

    switch (window_type) {
        case RECTANGULAR: {
            for (int sample = 0; sample < fft_size; ++sample)
                fft_window[sample] = 1.0f;
            break;
        }
        case BART_LETT: {
            for (int sample = 0; sample < fft_size; ++sample)
                fft_window[sample] = 1.0f - fabs (2.0f * (float)sample / (float)(fft_size - 1) - 1.0f);
            break;
        }
        case HANN: {
            for (int sample = 0; sample < fft_size; ++sample)
                fft_window[sample] = 0.5f - 0.5f * cosf (2.0f * M_PI * (float)sample / (float)(fft_size - 1));
            break;
        }
        case HAMMING: {
            for (int sample = 0; sample < fft_size; ++sample)
                fft_window[sample] = 0.54f - 0.46f * cosf (2.0f * M_PI * (float)sample / (float)(fft_size - 1));
            break;
        }
    }

    float window_sum = 0.0f;
    for (int sample = 0; sample < fft_size; ++sample)
        window_sum += fft_window[sample];

    window_scale_factor = 0.0f;
    if (overlap != 0 && window_sum != 0.0f)
        window_scale_factor = 1.0f / (float)overlap / window_sum * (float)fft_size;
}



void STFT::analysis(const int channel)
{
    int input_buffer_index = current_input_buffer_write_position;
    for (int index = 0; index < fft_size; ++index) {
        time_domain_buffer[index].real(fft_window[index] * input_buffer.getSample(channel, input_buffer_index));
        time_domain_buffer[index].imag(0.0f);

        if (++input_buffer_index >= input_buffer_length)
            input_buffer_index = 0;
    }
}

void STFT::modification(const int channel)
{
    fft->perform(time_domain_buffer, frequency_domain_buffer, false);

    for (int index = 0; index < fft_size / 2 + 1; ++index) {
        float magnitude = abs(frequency_domain_buffer[index]);
        float phase = arg(frequency_domain_buffer[index]);

        frequency_domain_buffer[index].real(magnitude * cosf (phase));
        frequency_domain_buffer[index].imag(magnitude * sinf (phase));

        if (index > 0 && index < fft_size / 2) {
            frequency_domain_buffer[fft_size - index].real(magnitude * cosf (phase));
            frequency_domain_buffer[fft_size - index].imag(magnitude * sinf (-phase));
        }
    }

    fft->perform(frequency_domain_buffer, time_domain_buffer, true);
}

void STFT::synthesis(const int channel)
{
    int output_buffer_index = current_output_buffer_write_position;
    for (int index = 0; index < fft_size; ++index) {
        float output_sample = output_buffer.getSample(channel, output_buffer_index);
        output_sample += time_domain_buffer[index].real() * window_scale_factor;
        output_buffer.setSample(channel, output_buffer_index, output_sample);

        if (++output_buffer_index >= output_buffer_length)
            output_buffer_index = 0;
    }

    current_output_buffer_write_position += hop_size;
    if (current_output_buffer_write_position >= output_buffer_length)
        current_output_buffer_write_position = 0;
}

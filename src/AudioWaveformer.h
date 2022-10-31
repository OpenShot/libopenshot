/**
 * @file
 * @brief Header file for AudioWaveformer class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2022 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_WAVEFORMER_H
#define OPENSHOT_WAVEFORMER_H

#include "ReaderBase.h"
#include "Frame.h"
#include <vector>


namespace openshot {

	/**
	 * @brief This class is used to extra audio data used for generating waveforms.
	 *
	 * Pass in a ReaderBase* with audio data, and this class will iterate the reader,
	 * and sample down the dataset to a much smaller set - more useful for generating
	 * waveforms. For example, take 44100 samples per second, and reduce it to 20
	 * "average" samples per second - much easier to graph.
	 */
	class AudioWaveformer {
	private:
        ReaderBase* reader;

	public:
		/// Default constructor
        AudioWaveformer(ReaderBase* reader);

        /// @brief Extract audio samples from any ReaderBase class
        /// @param channel Which audio channel should we extract data from
        /// @param num_per_second How many samples per second to return
        /// @param normalize Should we scale the data range so the largest value is 1.0
		std::vector<float> ExtractSamples(int channel, int num_per_second, bool normalize);

		/// Destructor
		~AudioWaveformer();
	};

}

#endif

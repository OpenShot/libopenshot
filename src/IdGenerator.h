/*
 * @file
 * @brief Header file for generating random identifier strings
 */

// Copyright (c) 2008-2025 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_ID_GENERATOR_H
#define OPENSHOT_ID_GENERATOR_H

#include <random>
#include <string>

namespace openshot {

	class IdGenerator {
	public:
		static inline std::string Generate(int length = 8) {
		static const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
			std::random_device rd;
			std::mt19937 gen(rd());
			std::uniform_int_distribution<> dist(0, static_cast<int>(sizeof(charset) - 2));

			std::string result;
			result.reserve(length);
			for (int i = 0; i < length; ++i)
			result += charset[dist(gen)];
			return result;
}
};

} // namespace openshot

#endif // OPENSHOT_ID_GENERATOR_H

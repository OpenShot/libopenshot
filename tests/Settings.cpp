/**
 * @file
 * @brief Unit tests for openshot::Color
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "openshot_catch.h"

#include "Settings.h"
#include <omp.h>


using namespace openshot;

TEST_CASE( "Constructor", "[libopenshot][settings]" )
{
	// Get system cpu count
	int cpu_count = omp_get_num_procs();

	// Create an empty color
	Settings *s = Settings::Instance();

	CHECK(s->OMP_THREADS == cpu_count);
	CHECK(s->FF_THREADS == cpu_count);
	CHECK_FALSE(s->HIGH_QUALITY_SCALING);
}

TEST_CASE( "Change settings", "[libopenshot][settings]" )
{
	// Create an empty color
	Settings *s = Settings::Instance();
	s->OMP_THREADS = 13;
	s->HIGH_QUALITY_SCALING = true;

	CHECK(s->OMP_THREADS == 13);
	CHECK(s->HIGH_QUALITY_SCALING == true);

	CHECK(Settings::Instance()->OMP_THREADS == 13);
	CHECK(Settings::Instance()->HIGH_QUALITY_SCALING == true);
}

TEST_CASE( "Debug logging", "[libopenshot][settings][environment]")
{
	// Check the environment
	auto envvar = std::getenv("LIBOPENSHOT_DEBUG");
	const auto is_enabled = bool(envvar != nullptr);

	CHECK(Settings::Instance()->DEBUG_TO_STDERR == is_enabled);
}

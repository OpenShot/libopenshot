/**
 * @file
 * @brief Source file for Timeline class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "TimelineBase.h"

using namespace openshot;

/// Constructor for the base timeline
TimelineBase::TimelineBase()
{
	// Init preview size (default)
	preview_width = 1920;
	preview_height = 1080;
}

/* This function will be overloaded in the Timeline class passing no arguments
* so we'll be able to access the Timeline::Clips() function from a pointer object of
* the TimelineBase class
*/
void TimelineBase::Clips(int test){
	return;
}
/**
 * @file
 * @brief Source file for RendererBase class
 * @author Duzy Chan <code@duzy.info>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "RendererBase.h"
using namespace openshot;

RendererBase::RendererBase()
{
}

RendererBase::~RendererBase()
{
}

void RendererBase::paint(const std::shared_ptr<Frame> & frame)
{
	if (frame)
		this->render(frame->GetImage());
}

/**
 * @file
 * @brief Header file for RendererBase class
 * @author Duzy Chan <code@duzy.info>
 *
 * @ref License
 */

// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#ifndef OPENSHOT_RENDERER_BASE_H
#define OPENSHOT_RENDERER_BASE_H

#include "Frame.h"
#include <cstdlib> // for realloc
#include <memory>

namespace openshot
{
    class Frame;

    /**
     * @brief This is the base class of all Renderers in libopenshot.
     *
     * Renderers are responsible for rendering images of a video onto a
     * display device.
     */
    class RendererBase
    {
    public:

	/// Paint(render) a video Frame.
	void paint(const std::shared_ptr<openshot::Frame> & frame);

	/// Allow manual override of the QWidget that is used to display
	virtual void OverrideWidget(int64_t qwidget_address) = 0;

    protected:
	RendererBase();
	virtual ~RendererBase();

	virtual void render(std::shared_ptr<QImage> image) = 0;
    };

}

#endif

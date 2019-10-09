/**
 * @file
 * @brief Unit tests for openshot::Timeline
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

/* LICENSE
 *
 * Copyright (c) 2008-2019 OpenShot Studios, LLC
 * <http://www.openshotstudios.com/>. This file is part of
 * OpenShot Library (libopenshot), an open-source project dedicated to
 * delivering high quality video editing and animation solutions to the
 * world. For more information visit <http://www.openshot.org/>.
 *
 * OpenShot Library (libopenshot) is free software: you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * OpenShot Library (libopenshot) is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with OpenShot Library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "UnitTest++.h"
#include "../include/OpenShot.h"

#ifdef TEST_PLUGINS_PATH
#define TEST_PLUGINS_PATH_LIB TEST_PLUGINS_PATH "liblibeffectsuperblur.so"
#else
#define TEST_PLUGINS_PATH_LIB "liblibeffectsuperblur.so"
#endif

using namespace std;
using namespace openshot;


TEST(DynamicEffect_Loader)
{
    auto effect_info = EffectInfo();
    effect_info.UnloadDynamicEffects();

    auto effect = effect_info.LoadEffect(TEST_PLUGINS_PATH_LIB);

    CHECK(effect != nullptr);

    delete effect;
}


TEST(DynamicEffect_DoubleLoader)
{
    auto effect_info = EffectInfo();
    effect_info.UnloadDynamicEffects();

    auto effect = effect_info.LoadEffect(TEST_PLUGINS_PATH_LIB);

    CHECK(effect != nullptr);

    delete effect;

    effect = effect_info.LoadEffect(TEST_PLUGINS_PATH_LIB);

    CHECK(effect != nullptr);

    delete effect;
}


TEST(DynamicEffect_ReachByName)
{
    auto effect_info = EffectInfo();
    effect_info.UnloadDynamicEffects();

    auto effect = effect_info.LoadEffect(TEST_PLUGINS_PATH_LIB);

    CHECK(effect != nullptr);

    delete effect;

    effect = effect_info.CreateEffect("SuperBlur");

    CHECK(effect != nullptr);

    delete effect;
}


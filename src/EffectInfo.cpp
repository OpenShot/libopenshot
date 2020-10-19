/**
 * @file
 * @brief Source file for EffectInfo class
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

#include "EffectInfo.h"

using namespace openshot;

static std::map<std::string, EffectBase*(*)(uint16_t)> m_loadedDynamicEffects;
static std::vector<void*> m_loadedDynamicHandles;

// Generate JSON string of this object
std::string EffectInfo::Json() {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Create a new effect instance
EffectBase* EffectInfo::CreateEffect(std::string effect_type) {
    // Try to find dynamically loaded effect
    auto effect_factory = m_loadedDynamicEffects.find(effect_type);
    if (effect_factory != m_loadedDynamicEffects.end())
        return effect_factory->second(OPENSHOT_PLUGIN_API_VERSION);

	// Init the matching effect object
	if (effect_type == "Bars")
		return new Bars();

	if (effect_type == "Blur")
		return new Blur();

	else if (effect_type == "Brightness")
		return new Brightness();

	else if (effect_type == "ChromaKey")
		return new ChromaKey();

	else if (effect_type == "ColorShift")
		return new ColorShift();

	else if (effect_type == "Crop")
		return new Crop();

	else if (effect_type == "Deinterlace")
		return new Deinterlace();

	else if (effect_type == "Hue")
		return new Hue();

	else if (effect_type == "Mask")
		return new Mask();

	else if (effect_type == "Negate")
		return new Negate();

	else if (effect_type == "Pixelate")
		return new Pixelate();

	else if (effect_type == "Saturation")
		return new Saturation();

	else if (effect_type == "Shift")
		return new Shift();

	else if (effect_type == "Wave")
		return new Wave();
	return NULL;
}

#if defined(__linux__)
EffectBase* EffectInfo::LoadEffect(std::string location){
    pthread_mutex_lock(&m_mutex);

    void * file = dlopen(location.c_str(), RTLD_NOW);
    if (file == nullptr){
        pthread_mutex_unlock(&m_mutex);
        throw InvalidFile("Can not open file", location.c_str());
    }
    void * factory_ptr = dlsym(file, "factory");

    if (factory_ptr == nullptr){
        pthread_mutex_unlock(&m_mutex);
        dlclose(file);
        throw InvalidFile("Can not find requested plugin API in file", location.c_str());
    }

    EffectBase * (*factory)(uint16_t);
    factory = (EffectBase* (*)(uint16_t)) factory_ptr;

    EffectBase* instance = factory(OPENSHOT_PLUGIN_API_VERSION);

    if (instance == nullptr){
        pthread_mutex_unlock(&m_mutex);
        dlclose(file);
        throw InvalidFile("Plugin does not support current version of openshot", location.c_str());
    }

    m_loadedDynamicEffects.insert(make_pair(std::string(instance->info.name), factory));
    m_loadedDynamicHandles.insert(m_loadedDynamicHandles.end(), file);

    pthread_mutex_unlock(&m_mutex);
    return instance;
}

void EffectInfo::UnloadDynamicEffects(){
    pthread_mutex_lock(&m_mutex);

    for (auto & handle : m_loadedDynamicHandles){
        dlclose(handle);
    }

    m_loadedDynamicEffects.clear();
    m_loadedDynamicHandles.clear();
    pthread_mutex_unlock(&m_mutex);
}
#else
EffectBase* EffectInfo::LoadEffect(std::string location){
    return NULL;
}

void EffectInfo::UnloadDynamicEffects(){

}
#endif

// Generate Json::Value for this object
Json::Value EffectInfo::JsonValue() {

	// Create root json object
	Json::Value root;

	// Append info JSON from each supported effect
	root.append(Bars().JsonInfo());
	root.append(Blur().JsonInfo());
	root.append(Brightness().JsonInfo());
	root.append(ChromaKey().JsonInfo());
	root.append(ColorShift().JsonInfo());
	root.append(Crop().JsonInfo());
	root.append(Deinterlace().JsonInfo());
	root.append(Hue().JsonInfo());
	root.append(Mask().JsonInfo());
	root.append(Negate().JsonInfo());
	root.append(Pixelate().JsonInfo());
	root.append(Saturation().JsonInfo());
	root.append(Shift().JsonInfo());
	root.append(Wave().JsonInfo());

	for (auto & effect : m_loadedDynamicEffects){
	    auto instance = effect.second(OPENSHOT_PLUGIN_API_VERSION);
	    root.append(instance->JsonInfo());
	    delete instance;
	}

	// return JsonValue
	return root;

}

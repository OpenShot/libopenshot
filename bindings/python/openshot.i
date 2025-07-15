/**
 * @file
 * @brief SWIG configuration for libopenshot (to generate Python SWIG bindings)
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @section LICENSE
*/
// Copyright (c) 2008-2019 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

%module("threads"=1) openshot

/* Suppress warnings about ignored operator= */
%warnfilter(362);

/* Don't generate multiple wrappers for functions with default args */
%feature("compactdefaultargs", "1");

/* Enable inline documentation */
%feature("autodoc", "1");

/* Include various SWIG helpers */
%include "typemaps.i"
%include "std_string.i"
%include "std_list.i"
%include "std_vector.i"
%include "std_map.i"
%include <stdint.i>

/* Unhandled STL Exception Handling */
%include <std_except.i>

/* Include shared pointer code */
%include <std_shared_ptr.i>

/* Mark these classes as shared_ptr classes */
#ifdef USE_IMAGEMAGICK
    %shared_ptr(Magick::Image)
#endif
%shared_ptr(juce::AudioBuffer<float>)
%shared_ptr(openshot::Frame)

/* Rename operators to avoid wrapping name collisions */
%rename(__eq__) operator==;
%rename(__lt__) operator<;
%rename(__gt__) operator>;

/* Instantiate the required template specializations */
%template() std::map<std::string, int>;
%template() std::pair<int, int>;
%template() std::vector<int>;
%template() std::vector<float>;
%template() std::pair<double, double>;
%template() std::pair<float, float>;
%template() std::pair<std::string, std::string>;
%template() std::vector<std::pair<std::string, std::string>>;
%template() std::vector<std::vector<float>>;

%{
#include "OpenShotVersion.h"
#include "ReaderBase.h"
#include "WriterBase.h"
#include "AudioDevices.h"
#include "AudioWaveformer.h"
#include "CacheBase.h"
#include "CacheDisk.h"
#include "CacheMemory.h"
#include "ChannelLayouts.h"
#include "ChunkReader.h"
#include "ChunkWriter.h"
#include "ClipBase.h"
#include "Clip.h"
#include "Coordinate.h"
#include "Color.h"
#include "DummyReader.h"
#include "EffectBase.h"
#include "Effects.h"
#include "EffectInfo.h"
#include "Enums.h"
#include "Exceptions.h"
#include "FFmpegReader.h"
#include "FFmpegWriter.h"
#include "Fraction.h"
#include "Frame.h"
#include "FrameMapper.h"
#include "PlayerBase.h"
#include "Point.h"
#include "Profiles.h"
#include "QtHtmlReader.h"
#include "QtImageReader.h"
#include "QtPlayer.h"
#include "QtTextReader.h"
#include "KeyFrame.h"
#include "RendererBase.h"
#include "Settings.h"
#include "TimelineBase.h"
#include "Timeline.h"
#include "Qt/VideoCacheThread.h"
#include "ZmqLogger.h"

%}

// Prevent SWIG from ever generating a wrapper for juce::Thread’s constructor (or run())
%ignore juce::Thread::Thread;

#ifdef USE_IMAGEMAGICK
    %{
        #include "ImageReader.h"
        #include "ImageWriter.h"
        #include "TextReader.h"
    %}
#endif

#ifdef USE_OPENCV
    %{
        #include "ClipProcessingJobs.h"
        #include "effects/Stabilizer.h"
        #include "effects/Tracker.h"
        #include "effects/ObjectDetection.h"
        #include "effects/Outline.h"
        #include "TrackedObjectBase.h"
        #include "TrackedObjectBBox.h"
    %}
#endif

/* Generic language independent exception handler. */
%include "exception.i"
%exception {
    try {
        $action
    }
    catch (openshot::ExceptionBase &e) {
        SWIG_exception_fail(SWIG_RuntimeError, e.py_message().c_str());
    }
    catch (std::exception &e) {
        SWIG_exception_fail(SWIG_RuntimeError, e.what());
    }
}



/* Wrap std templates (list, vector, etc...) */
%template(ClipList) std::list<openshot::Clip *>;
%template(EffectBaseList) std::list<openshot::EffectBase *>;
%template(CoordinateVector) std::vector<openshot::Coordinate>;
%template(PointsVector) std::vector<openshot::Point>;
%template(FieldVector) std::vector<openshot::Field>;
%template(MappedFrameVector) std::vector<openshot::MappedFrame>;
%template(MetadataMap) std::map<std::string, std::string>;

/* Deprecated */
%template(AudioDeviceInfoVector) std::vector<openshot::AudioDeviceInfo>;

/* Make openshot.Fraction more Pythonic */
%extend openshot::Fraction {
%{
    #include <sstream>
    #include <map>
    #include <vector>

    static std::vector<std::string> _keys{"num", "den"};
    static int fracError = 0;
%}
    double __float__() {
        return $self->ToDouble();
    }
    int __int__() {
        return $self->ToInt();
    }
    /* Dictionary-type methods */
    int __len__() {
        return _keys.size();
    }
    %exception __getitem__ {
        $action
        if (fracError == 1) {
            fracError = 0;  // Clear flag for reuse
            PyErr_SetString(PyExc_KeyError, "Key not found");
            SWIG_fail;
        }
    }
    const std::string __getitem__(int index) {
        if (index < static_cast<int>(_keys.size())) {
            return _keys[index];
        }
        /* Otherwise, raise an exception */
        fracError = 1;
        return "";
    }
    int __getitem__(const std::string& key) {
        if (key == "num") {
            return $self->num;
        } else if (key == "den") {
            return $self->den;
        }
        /* Otherwise, raise an exception */
        fracError = 1;
        return 0;
    }
    bool __contains__(const std::string& key) {
        return bool(std::find(_keys.begin(), _keys.end(), key) != _keys.end());
    }
    std::map<std::string, int> GetMap() {
        std::map<std::string, int> map1;
        map1.insert({"num", $self->num});
        map1.insert({"den", $self->den});
        return map1;
    }
    /* Display methods */
    const std::string __str__() {
        std::ostringstream result;
        result << $self->num << ":" << $self->den;
        return result.str();
    }
    const std::string __repr__() {
        std::ostringstream result;
        result << "Fraction(" << $self->num << ", " << $self->den << ")";
        return result.str();
    }
    /* Implement dict methods in Python */
    %pythoncode %{
        def __iter__(self):
            return iter(self.GetMap())
        def keys(self):
            _items = self.GetMap()
            return _items.keys()
        def items(self):
            _items = self.GetMap()
            return _items.items()
        def values(self):
            _items = self.GetMap()
            return _items.values()
        def __mul__(self, other):
            if isinstance(other, Fraction):
                return Fraction(self.num * other.num, self.den * other.den)
            return float(self) * other
        def __rmul__(self, other):
            return other * float(self)
        def __truediv__(self, other):
            if isinstance(other, Fraction):
                return Fraction(self.num * other.den, self.den * other.num)
            return float(self) / other;
        def __rtruediv__(self, other):
            return other / float(self)
        def __format__(self, format_spec):
            integer_fmt = "bcdoxX"
            float_fmt = "eEfFgGn%"
            all_fmt = "".join(["s", integer_fmt, float_fmt])
            if not format_spec or format_spec[-1] not in all_fmt:
              value = str(self)
            elif format_spec[-1] in integer_fmt:
                value = int(self)
            elif format_spec[-1] in float_fmt:
                value = float(self)
            else:
                value = str(self)
            return "{value:{spec}}".format(value=value, spec=format_spec)
    %}
}

%extend openshot::Profile {
    bool __eq__(const openshot::Profile& other) const {
        return (*self == other);
    }
    bool __lt__(const openshot::Profile& other) const {
        return (*self < other);
    }
    bool __gt__(const openshot::Profile& other) const {
        return (*self > other);
    }
}

%extend openshot::OpenShotVersion {
        // Give the struct a string representation
    const std::string __str__() {
        return std::string(OPENSHOT_VERSION_FULL);
    }
    // And a repr for interactive use
    const std::string __repr__() {
        std::ostringstream result;
        result << "OpenShotVersion('" << OPENSHOT_VERSION_FULL << "')";
        return result.str();
    }
}

%include "OpenShotVersion.h"
%include "ReaderBase.h"
%include "WriterBase.h"
%include "AudioDevices.h"
%include "AudioWaveformer.h"
%include "CacheBase.h"
%include "CacheDisk.h"
%include "CacheMemory.h"
%include "ChannelLayouts.h"
%include "ChunkReader.h"
%include "ChunkWriter.h"
%include "ClipBase.h"
%include "Clip.h"
%include "Coordinate.h"
%include "Color.h"
%include "DummyReader.h"
%include "EffectBase.h"
%include "Effects.h"
%include "EffectInfo.h"
%include "Enums.h"
%include "Exceptions.h"
%include "FFmpegReader.h"
%include "FFmpegWriter.h"
%include "Fraction.h"
%include "Frame.h"
%include "FrameMapper.h"
%include "PlayerBase.h"
%include "Point.h"
%include "Profiles.h"
%include "QtHtmlReader.h"
%include "QtImageReader.h"
%include "QtPlayer.h"
%include "QtTextReader.h"
%include "KeyFrame.h"
%include "RendererBase.h"
%include "Settings.h"
%include "TimelineBase.h"
%include "Qt/VideoCacheThread.h"
%include "Timeline.h"
%include "ZmqLogger.h"

#ifdef USE_OPENCV
    %include "ClipProcessingJobs.h"
    %include "TrackedObjectBase.h"
    %include "TrackedObjectBBox.h"
#endif

#ifdef USE_IMAGEMAGICK
    %include "ImageReader.h"
    %include "ImageWriter.h"
    %include "TextReader.h"
#endif

/* Effects */
%include "effects/Bars.h"
%include "effects/Blur.h"
%include "effects/Brightness.h"
%include "effects/Caption.h"
%include "effects/ChromaKey.h"
%include "effects/ColorMap.h"
%include "effects/ColorShift.h"
%include "effects/Crop.h"
%include "effects/Deinterlace.h"
%include "effects/Hue.h"
%include "effects/LensFlare.h"
%include "effects/Mask.h"
%include "effects/Negate.h"
%include "effects/Pixelate.h"
%include "effects/Saturation.h"
%include "effects/Sharpen.h"
%include "effects/Shift.h"
%include "effects/SphericalProjection.cpp"
%include "effects/Wave.h"
#ifdef USE_OPENCV
    %include "effects/Stabilizer.h"
    %include "effects/Tracker.h"
    %include "effects/ObjectDetection.h"
    %include "effects/Outline.h"
#endif

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
%apply uint64_t { uintptr_t };

class QWidget;

/* Unhandled STL Exception Handling */
%include <std_except.i>

/* Include shared pointer code */
%include <std_shared_ptr.i>

%typemap(in) QWidget *;

/* Mark these classes as shared_ptr classes */
#ifdef USE_IMAGEMAGICK
    %shared_ptr(Magick::Image)
#endif
%shared_ptr(juce::AudioBuffer<float>)
%shared_ptr(openshot::Frame)

%newobject openshot::Clip::CreateReader;

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
#include <QtWidgets/QWidget>

static void *openshot_swig_pylong_as_ptr(PyObject *obj) {
    if (!obj) {
        return nullptr;
    }

    unsigned long long ull = PyLong_AsUnsignedLongLong(obj);
    if (!PyErr_Occurred()) {
        return reinterpret_cast<void*>(static_cast<uintptr_t>(ull));
    }
    PyErr_Clear();

    long long ll = PyLong_AsLongLong(obj);
    if (!PyErr_Occurred()) {
        return reinterpret_cast<void*>(static_cast<intptr_t>(ll));
    }
    PyErr_Clear();

    return nullptr;
}

static void *openshot_swig_get_qwidget_ptr(PyObject *obj) {
    if (!obj || obj == Py_None) {
        return nullptr;
    }

    if (PyLong_Check(obj)) {
        void *ptr = openshot_swig_pylong_as_ptr(obj);
        return ptr;
    }

    const char *sip_modules[] = {"sip", "PyQt6.sip", "PyQt5.sip"};
    for (size_t i = 0; i < (sizeof(sip_modules) / sizeof(sip_modules[0])); ++i) {
        PyObject *mod = PyImport_ImportModule(sip_modules[i]);
        if (!mod) {
            PyErr_Clear();
            continue;
        }
        PyObject *unwrap = PyObject_GetAttrString(mod, "unwrapinstance");
        if (unwrap && PyCallable_Check(unwrap)) {
            PyObject *addr = PyObject_CallFunctionObjArgs(unwrap, obj, NULL);
            if (addr) {
                void *ptr = openshot_swig_pylong_as_ptr(addr);
                Py_DECREF(addr);
                if (ptr) {
                    Py_DECREF(unwrap);
                    Py_DECREF(mod);
                    return ptr;
                }
            }
        }
        Py_XDECREF(unwrap);
        Py_DECREF(mod);
    }

    const char *shiboken_modules[] = {"shiboken6", "shiboken2"};
    for (size_t i = 0; i < (sizeof(shiboken_modules) / sizeof(shiboken_modules[0])); ++i) {
        PyObject *mod = PyImport_ImportModule(shiboken_modules[i]);
        if (!mod) {
            PyErr_Clear();
            continue;
        }
        PyObject *get_ptr = PyObject_GetAttrString(mod, "getCppPointer");
        if (get_ptr && PyCallable_Check(get_ptr)) {
            PyObject *ptrs = PyObject_CallFunctionObjArgs(get_ptr, obj, NULL);
            if (ptrs) {
                PyObject *addr = ptrs;
                if (PyTuple_Check(ptrs) && PyTuple_Size(ptrs) > 0) {
                    addr = PyTuple_GetItem(ptrs, 0);
                }
                void *ptr = openshot_swig_pylong_as_ptr(addr);
                Py_DECREF(ptrs);
                if (ptr) {
                    Py_DECREF(get_ptr);
                    Py_DECREF(mod);
                    return ptr;
                }
            }
        }
        Py_XDECREF(get_ptr);
        Py_DECREF(mod);
    }

    return nullptr;
}

static int openshot_swig_is_qwidget(PyObject *obj) {
    void *ptr = openshot_swig_get_qwidget_ptr(obj);
    if (ptr) {
        return 1;
    }
    return obj == Py_None ? 1 : 0;
}

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

%typemap(in) QWidget * {
    void *ptr = openshot_swig_get_qwidget_ptr($input);
    if (!ptr && $input != Py_None) {
        SWIG_exception_fail(SWIG_TypeError, "Expected QWidget or Qt binding widget");
    }
    $1 = reinterpret_cast<QWidget*>(ptr);
}

%typemap(typecheck) QWidget * {
    $1 = openshot_swig_is_qwidget($input);
}

%typemap(out) uintptr_t openshot::QtPlayer::GetRendererQObject {
    $result = PyLong_FromLongLong((long long)(intptr_t)$1);
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

%extend openshot::Frame {
    PyObject* GetPixelsBytes() {
        PyGILState_STATE gstate = PyGILState_Ensure();
        PyObject* result = NULL;

        std::shared_ptr<QImage> img = $self->GetImage();
        if (!img) {
            Py_INCREF(Py_None);
            result = Py_None;
            PyGILState_Release(gstate);
            return result;
        }

        const Py_ssize_t size =
            static_cast<Py_ssize_t>(img->bytesPerLine()) *
            static_cast<Py_ssize_t>(img->height());

        const unsigned char* p = img->constBits();
        if (!p || size <= 0) {
            Py_INCREF(Py_None);
            result = Py_None;
            PyGILState_Release(gstate);
            return result;
        }

        result = PyBytes_FromStringAndSize(reinterpret_cast<const char*>(p), size);
        PyGILState_Release(gstate);
        return result;
    }

    PyObject* GetPixelsRowBytes(int row) {
        PyGILState_STATE gstate = PyGILState_Ensure();
        PyObject* result = NULL;

        std::shared_ptr<QImage> img = $self->GetImage();
        if (!img) {
            Py_INCREF(Py_None);
            result = Py_None;
            PyGILState_Release(gstate);
            return result;
        }

        if (row < 0 || row >= img->height()) {
            PyErr_SetString(PyExc_IndexError, "row out of range");
            PyGILState_Release(gstate);
            return NULL;
        }

        const unsigned char* p = img->constScanLine(row);
        if (!p) {
            Py_INCREF(Py_None);
            result = Py_None;
            PyGILState_Release(gstate);
            return result;
        }

        const Py_ssize_t row_bytes = static_cast<Py_ssize_t>(img->bytesPerLine());
        result = PyBytes_FromStringAndSize(reinterpret_cast<const char*>(p), row_bytes);
        PyGILState_Release(gstate);
        return result;
    }

    int GetBytesPerLine() {
        std::shared_ptr<QImage> img = $self->GetImage();
        return img ? img->bytesPerLine() : 0;
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

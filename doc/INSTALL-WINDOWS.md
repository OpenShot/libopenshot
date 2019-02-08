## Getting Started

The best way to get started with libopenshot, is to learn about our build system, obtain all the 
source code, install a development IDE and tools, and better understand our dependencies. So, 
please read through the following sections, and follow the instructions. And keep in mind, 
that your computer is likely different than the one used when writing these instructions. 
Your file paths and versions of applications might be slightly different, so keep an eye out 
for subtle file path differences in the commands you type.

## Build Tools

CMake is the backbone of our build system.  It is a cross-platform build system, which 
checks for dependencies, locates header files and libraries, generates makefiles, and 
supports the cross-platform compiling of libopenshot and libopenshot-audio.  CMake uses 
an out-of-source build concept, where all temporary build files, such as makefiles, 
object files, and even the final binaries, are created outside of the source code 
folder, inside a /build/ sub-folder.  This prevents the build process from cluttering 
up the source code.  These instructions have only been tested with the GNU compiler 
(including MSYS2/MinGW for Windows).

## Dependencies

The following libraries are required to build libopenshot.  Instructions on how to 
install these dependencies vary for each operating system.  Libraries and Executables 
have been labeled in the list below to help distinguish between them.

* ### FFmpeg (libavformat, libavcodec, libavutil, libavdevice, libavresample, libswscale)
  * http://www.ffmpeg.org/ `(Library)`
  * This library is used to decode and encode video, audio, and image files.  It is also used to obtain information about media files, such as frame rate, sample rate, aspect ratio, and other common attributes.

* ### ImageMagick++ (libMagick++, libMagickWand, libMagickCore)
  * http://www.imagemagick.org/script/magick++.php `(Library)`
  * This library is **optional**, and used to decode and encode images.

* ### OpenShot Audio Library (libopenshot-audio)
  * https://github.com/OpenShot/libopenshot-audio/ `(Library)`
  * This library is used to mix, resample, host plug-ins, and play audio. It is based on the JUCE project, which is an outstanding audio library used by many different applications

* ### Qt 5 (libqt5)
  * http://www.qt.io/qt5/ `(Library)`
  * Qt5 is used to display video, store image data, composite images, apply image effects, and many other utility functions, such as file system manipulation, high resolution timers, etc...

* ### CMake (cmake)
  * http://www.cmake.org/ `(Executable)`
  * This executable is used to automate the generation of Makefiles, check for dependencies, and is the backbone of libopenshot’s cross-platform build process.

* ### SWIG (swig)
  * http://www.swig.org/ `(Executable)`
  * This executable is used to generate the Python and Ruby bindings for libopenshot. It is a simple and powerful wrapper for C++ libraries, and supports many languages.

* ### Python 3 (libpython)
  * http://www.python.org/ `(Executable and Library)`
  * This library is used by swig to create the Python (version 3+) bindings for libopenshot. This is also the official language used by OpenShot Video Editor (a graphical interface to libopenshot).

* ### Doxygen (doxygen)
  * http://www.stack.nl/~dimitri/doxygen/ `(Executable)`
  * This executable is used to auto-generate the documentation used by libopenshot.

* ### UnitTest++ (libunittest++)
  * https://github.com/unittest-cpp/ `(Library)`
  * This library is used to execute unit tests for libopenshot.  It contains many macros used to keep our unit testing code very clean and simple.

* ### ZeroMQ (libzmq)
  * http://zeromq.org/ `(Library)`
  * This library is used to communicate between libopenshot and other applications (publisher / subscriber). Primarily used to send debug data from libopenshot.

* ### OpenMP (-fopenmp)
  * http://openmp.org/wp/ `(Compiler Flag)`
  * If your compiler supports this flag (GCC, Clang, and most other compilers), it provides libopenshot with easy methods of using parallel programming techniques to improve performance and take advantage of multi-core processors.

## CMake Flags (Optional)
There are many different build flags that can be passed to cmake to adjust how libopenshot 
is compiled. Some of these flags might be required when compiling on certain OSes, just 
depending on how your build environment is setup. To add a build flag, follow this general 
syntax: `cmake -DMAGICKCORE_HDRI_ENABLE=1 -DENABLE_TESTS=1 ../`

* MAGICKCORE_HDRI_ENABLE (default 0)
* MAGICKCORE_QUANTUM_DEPTH (default 0)
* OPENSHOT_IMAGEMAGICK_COMPATIBILITY (default 0)
* DISABLE_TESTS (default 0)
* CMAKE_PREFIX_PATH (`/location/to/missing/library/`)
* PYTHON_INCLUDE_DIR (`/location/to/python/include/`)
* PYTHON_LIBRARY (`/location/to/python/lib.a`)
* PYTHON_FRAMEWORKS (`/usr/local/Cellar/python3/3.3.2/Frameworks/Python.framework/`)
* CMAKE_CXX_COMPILER (`/location/to/mingw/g++`)
* CMAKE_C_COMPILER (`/location/to/mingw/gcc`)

## Environment Variables

Many environment variables will need to be set during this Windows installation guide. 
The command line will need to be closed and re-launched after any changes to your environment 
variables. Also, dependency libraries will not be found during linking or execution without 
being found in the PATH environment variable. So, if you get errors related to missing 
commands or libraries, double check the PATH variable.

The following environment variables need to be added to your “System Variables”.  Be sure to 
check each folder path for accuracy, as your paths will likely be different than this list.

### Example Variables

* DL_DIR (`C:\libdl`)
* DXSDK_DIR (`C:\Program Files\Microsoft DirectX SDK (June 2010)\`)
* FFMPEGDIR (`C:\ffmpeg-git-95f163b-win32-dev`)
* FREETYPE_DIR (`C:\Program Files\GnuWin32`)
* HOME (`C:\msys\1.0\home`)
* LIBOPENSHOT_AUDIO_DIR (`C:\Program Files\libopenshot-audio`)
* QTDIR (`C:\qt5`)
* SNDFILE_DIR (`C:\Program Files\libsndfile`)
* UNITTEST_DIR (`C:\UnitTest++`)
* ZMQDIR (`C:\msys2\usr\local\`)
* PATH (`The following paths are an example`)
   * C:\Qt5\bin; C:\Qt5\MinGW\bin\; C:\msys\1.0\local\lib; C:\Program Files\CMake 2.8\bin; C:\UnitTest++\build; C:\libopenshot\build\src; C:\Program Files\doxygen\bin; C:\ffmpeg-git-95f163b-win32-dev\lib; C:\swigwin-2.0.4; C:\Python33; C:\Program Files\Project\lib; C:\msys2\usr\local\





## Obtaining Source Code

The first step in installing libopenshot is to obtain the most recent source code. The source code 
is available on [GitHub](https://github.com/OpenShot/libopenshot). Use the following command to 
obtain the latest libopenshot source code.

```
git clone https://github.com/OpenShot/libopenshot.git
git clone https://github.com/OpenShot/libopenshot-audio.git
```

## Folder Structure (libopenshot)

The source code is divided up into the following folders.

* ### build/
   * This folder needs to be manually created, and is used by cmake to store the temporary 
   build files, such as makefiles, as well as the final binaries (library and test executables).

* ### cmake/
   * This folder contains custom modules not included by default in cmake, used to find 
   dependency libraries and headers and determine if these libraries are installed.

* ### doc/
   * This folder contains documentation and related files, such as logos and images 
   required by the doxygen auto-generated documentation.

* ### include/
   * This folder contains all headers (*.h) used by libopenshot.

* ### src/
   * This folder contains all source code (*.cpp) used by libopenshot.

* ### tests/
   * This folder contains all unit test code.  Each class has it’s own test file (*.cpp), and 
   uses UnitTest++ macros to keep the test code simple and manageable.

* ### thirdparty/
   * This folder contains code not written by the OpenShot team. For example, jsoncpp, an 
   open-source JSON parser.

## Install MSYS2 Dependencies

Most Windows dependencies needed for libopenshot-audio, libopenshot, and openshot-qt
can be installed easily with MSYS2 and the pacman package manager. Follow these
directions to setup a Windows build environment for OpenShot.

1) Install MSYS2: http://www.msys2.org/

2) Run MSYS2 command prompt (for example: `C:\msys64\msys2_shell.cmd`)

3) Append PATH (so MSYS2 can find executables and libraries):

```
PATH=$PATH:/c/msys64/mingw64/bin:/c/msys64/mingw64/lib     (64-bit PATH)
  or 
PATH=$PATH:/c/msys32/mingw32/bin:/c/msys32/mingw32/lib     (32-bit PATH)
```

4) Update and upgrade all packages

```
pacman -Syu
```

5a) Install the following packages (**64-Bit**)

```
pacman -S --needed base-devel mingw-w64-x86_64-toolchain
pacman -S mingw64/mingw-w64-x86_64-ffmpeg
pacman -S mingw64/mingw-w64-x86_64-python3-pyqt5
pacman -S mingw64/mingw-w64-x86_64-swig
pacman -S mingw64/mingw-w64-x86_64-cmake
pacman -S mingw64/mingw-w64-x86_64-doxygen
pacman -S mingw64/mingw-w64-x86_64-python3-pip
pacman -S mingw32/mingw-w64-i686-zeromq
pacman -S mingw64/mingw-w64-x86_64-python3-pyzmq
pacman -S mingw64/mingw-w64-x86_64-python3-cx_Freeze
pacman -S git

# Install ImageMagick if needed (OPTIONAL and NOT NEEDED)
pacman -S mingw64/mingw-w64-x86_64-imagemagick
```
  
5b) **Or** Install the following packages (**32-Bit**)

```
pacman -S --needed base-devel mingw32/mingw-w64-i686-toolchain
pacman -S mingw32/mingw-w64-i686-ffmpeg
pacman -S mingw32/mingw-w64-i686-python3-pyqt5
pacman -S mingw32/mingw-w64-i686-swig
pacman -S mingw32/mingw-w64-i686-cmake
pacman -S mingw32/mingw-w64-i686-doxygen
pacman -S mingw32/mingw-w64-i686-python3-pip
pacman -S mingw32/mingw-w64-i686-zeromq
pacman -S mingw32/mingw-w64-i686-python3-pyzmq
pacman -S mingw32/mingw-w64-i686-python3-cx_Freeze
pacman -S git

# Install ImageMagick if needed (OPTIONAL and NOT NEEDED)
pacman -S mingw32/mingw-w32-x86_32-imagemagick
```

6) Install Python PIP Dependencies
 
```
pip3 install httplib2
pip3 install slacker
pip3 install tinys3
pip3 install github3.py
pip3 install requests
```  

7) Download Unittest++ (https://github.com/unittest-cpp/unittest-cpp) into /MSYS2/[USER]/unittest-cpp-master/

``` 
cmake -G "MSYS Makefiles" ../ -DCMAKE_MAKE_PROGRAM=mingw32-make -DCMAKE_INSTALL_PREFIX:PATH=/usr
mingw32-make install
```

8) ZMQ++ Header (This might not be needed anymore)
  NOTE: Download and copy zmq.hpp into the /c/msys64/mingw64/include/ folder

## Manual Dependencies

* ### DLfcn
   * https://github.com/dlfcn-win32/dlfcn-win32
   * Download and Extract the Win32 Static (.tar.bz2) archive to a local folder: C:\libdl\
   * Create an environment variable called DL_DIR and set the value to C:\libdl\. This environment variable will be used by CMake to find the binary and header file.

* ### DirectX SDK / Windows SDK
   * Windows 7: (DirectX SDK) http://www.microsoft.com/download/en/details.aspx?displaylang=en&id=6812
   * Windows 8: (Windows SDK)
   * https://msdn.microsoft.com/en-us/windows/desktop/aa904949
   * Download and Install the SDK Setup program.  This is needed for the JUCE library to play audio on Windows.
Create an environment variable called DXSDK_DIR and set the value to C:\Program Files\Microsoft DirectX SDK (June 2010)\  (your path might be different). This environment variable will be used by CMake to find the binaries and header files.

* ### libSndFile
   * http://www.mega-nerd.com/libsndfile/#Download
   * Download and Install the Win32 Setup program.
   * Create an environment variable called SNDFILE_DIR and set the value to C:\Program Files\libsndfile. This environment variable will be used by CMake to find the binary and header files.

* ### libzmq
   * http://zeromq.org/intro:get-the-software
   * Download source code (zip)
   * Follow their instructions, and build with mingw
   * Create an environment variable called ZMQDIR and set the value to C:\libzmq\build\ (the location of the compiled version). This environment variable will be used by CMake to find the binary and header files.

## Windows Build Instructions (libopenshot-audio)
In order to compile libopenshot-audio, launch a command prompt and enter the following commands. This does not require the MSYS2 prompt, but it should work in both the Windows command prompt and the MSYS2 prompt.

```
cd [libopenshot-audio repo folder]
mkdir build
cd build
cmake -G “MinGW Makefiles” ../
mingw32-make
mingw32-make install
openshot-audio-test-sound  (This should play a test sound)
```

## Windows Build Instructions (libopenshot)
Run the following commands to build libopenshot:

```
cd [libopenshot repo folder]
mkdir build
cd build
cmake -G "MinGW Makefiles" -DPYTHON_INCLUDE_DIR="C:/Python34/include/" -DPYTHON_LIBRARY="C:/Python34/libs/libpython34.a" ../
mingw32-make
```

If you are missing any dependencies for libopenshot, you will receive error messages at this point. 
Just install the missing dependencies, and run the above commands again. Repeat until no error 
messages are displayed and the build process completes.

Also, if you are having trouble building, please see the CMake Flags section above, as 
it might provide a solution for finding a missing folder path, missing Python 3 library, etc...

To run all unit tests (and verify everything is working correctly), launch a terminal, and enter:

```
mingw32-make test
```

To auto-generate the documentation for libopenshot, launch a terminal, and enter:

```
mingw32-make doc
```

This will use doxygen to generate a folder of HTML files, with all classes and methods 
documented. The folder is located at build/doc/html/. Once libopenshot has been successfully 
built, we need to install it (i.e. copy it to the correct folder, so other libraries can find it).

```
mingw32-make install
```

This should copy the binary files to C:\Program Files\openshot\lib\, and the header 
files to C:\Program Files\openshot\include\...  This is where other projects will 
look for the libopenshot files when building.. Python 3 bindings are also installed 
at this point. let's verify the python bindings work:

```
python3
>>> import openshot
```

If no errors are displayed, you have successfully compiled and installed libopenshot on 
your system. Congratulations and be sure to read our wiki on [Becoming an OpenShot Developer](https://github.com/OpenShot/openshot-qt/wiki/Become-a-Developer)! 
Welcome to the OpenShot developer community! We look forward to meeting you!

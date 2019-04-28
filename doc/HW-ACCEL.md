## Hardware Acceleration

Observations for developers wanting to make hardware acceleration work.

*All observations are for Linux (but contributions welcome).*

## Supported FFmpeg Versions

* HW accel is supported from ffmpeg version 3.2 (3.3 for nVidia drivers)
* HW accel was removed for nVidia drivers in Ubuntu for ffmpeg 4+
* I could not manage to build a version of ffmpeg 4.1 with the nVidia SDK
that worked with nVidia cards. There might be a problem in ffmpeg 4+
that prohibits this.

**Notice:** The ffmpeg versions of Ubuntu and PPAs for Ubuntu show the
same behaviour. ffmpeg 3 has working nVidia hardware acceleration while
ffmpeg 4+ has no support for nVidia hardware acceleration
included.

## OpenShot Settings

The following settings are use by libopenshot to enable, disable, and control
the various hardware acceleration features.

```
/// Use video codec for faster video decoding (if supported)
int HARDWARE_DECODER = 0;

/// Number of threads of OpenMP
int OMP_THREADS = 12;

/// Number of threads that ffmpeg uses
int FF_THREADS = 8;

/// Maximum rows that hardware decode can handle
int DE_LIMIT_HEIGHT_MAX = 1100;

/// Maximum columns that hardware decode can handle
int DE_LIMIT_WIDTH_MAX = 1950;

/// Which GPU to use to decode (0 is the first)
int HW_DE_DEVICE_SET = 0;

/// Which GPU to use to encode (0 is the first)
int HW_EN_DEVICE_SET = 0;
```

## Libva / VA-API (Video Acceleration API)

The correct version of libva is needed (libva in Ubuntu 16.04 or libva2
in Ubuntu 18.04) for the AppImage to work with hardware acceleration.
An AppImage that works on both systems (supporting libva and libva2),
might be possible when no libva is included in the AppImage.

* vaapi is working for intel and AMD
* vaapi is working for decode only for nouveau
* nVidia driver is working for export only

## AMD Graphics Cards (RadeonOpenCompute/ROCm)

Decoding and encoding on the (AMD) GPU is possible with the default drivers.
On systems where ROCm is installed and run a future use for GPU acceleration
of effects could be implemented (contributions welcome).

## Multiple Graphics Cards

If the computer has multiple graphics cards installed, you can choose which
should be used by libopenshot. Also, you can optionally use one card for
decoding and the other for encoding (if both cards support acceleration).

## Help Us Improve Hardware Support

This information might be wrong, and we would love to continue improving
our support for hardware acceleration in OpenShot. Please help us update
this document if you find an error or discover some new information.

**Desperately Needed:** The manual at:
https://www.tal.org/tutorials/ffmpeg_nvidia_encode
works pretty well. I could compile and install a version of ffmpeg 4.1.3
on Mint 19.1 that supports the GPU on nVidia cards. A version of openshot
with hardware support using these libraries could use the nVidia GPU.
(A way to compile ffmpeg 4.0 and up with working nVidia
hardware acceleration support on Ubuntu Linux!)

**BUG:** Hardware supported decoding still has a bug. The speed gains with
decoding are by far not as great as with encoding. In case hardware accelerated
decoding does not work disable it. Hardware acceleration might also break 
because of graphics drivers that have bugs.

**Needed:** A way to get the options and limits of the GPU, like
supported codecs and the supported dimensions (width and height).

**Further improvement:** Right now the frame can be decoded on the GPU but the
frame is then copied to CPU memory. Before encoding the frame is then
copied to GPU memory for encoding. That is necessary because the modifications
are done by the CPU. Using the GPU for that too will make it possible to do
away with these two copies. A possible solution would be to use Vulkan compute
which would be available on Linux and Windows natively and on MacOS via MoltenVK.

## Credit

A big thanks to Peter M (https://github.com/eisneinechse) for all his work
on integrating hardware accelleration into libopenshot! The community thanks
you for this major contribution!

## Hardware Acceleration

OpenShot now has experimental support for hardware acceleration, which uses 1 (or more)
graphics cards to offload some of the work for both decoding and encoding. This is
very new and experimental (as of May 2019), but we look forward to "accelerating"
our support for this in the future!

The following table summarizes our current level of support:

|                    |  Linux Decode   | Linux Encode   | Mac Decode |    Mac Encode  | Windows Decode | Windows Encode | Notes            |
|--------------------|:---------------:|:--------------:|:----------:|:--------------:|:--------------:|:--------------:|------------------|
| VA-API             |   ✔️ &nbsp;      |    ✔️ &nbsp;    |      -     |        -       |       -        |        -       | *Linux Only*     |
| VDPAU              | ✔️ <sup>1</sup>  | ✅ <sup>2</sup> |      -    |        -       |       -        |        -       | *Linux Only*     |
| CUDA (NVDEC/NVENC) | ❌ <sup>3</sup> |   ✔️ &nbsp;     |      -     |        -       |       -        |    ✔️ &nbsp;    | *Cross Platform* |
| VideoToolBox       |           -     |         -      |  ✔️ &nbsp;  | ❌ <sup>4</sup> |      -        |       -         | *Mac Only*       |
| DXVA2              |           -     |         -      |      -     |        -       | ❌ <sup>3</sup> |      -         | *Windows Only*   |
| D3D11VA            |           -     |         -      |      -     |        -       | ❌ <sup>3</sup> |      -         | *Windows Only*   |
| QSV                | ❌ <sup>3</sup> |   ❌ &nbsp;   |  ❌ &nbsp; |   ❌ &nbsp;    |   ❌ &nbsp;     |    ❌ &nbsp;   | *Cross Platform* |

#### Notes

1.  VDPAU for some reason needs a card number one higher than it really is
2.  VDPAU is a decoder only
3.  Green frames (pixel data not correctly tranferred back to system memory)
4.  Crashes and burns

## Supported FFmpeg Versions

* HW accel is supported from FFmpeg version 3.4
* HW accel was removed for nVidia drivers in Ubuntu for FFmpeg 4+

**Notice:** The FFmpeg versions of Ubuntu and PPAs for Ubuntu show the
same behaviour. FFmpeg 3 has working nVidia hardware acceleration while
FFmpeg 4+ has no support for nVidia hardware acceleration
included.

## OpenShot Settings

The following settings are use by libopenshot to enable, disable, and control
the various hardware acceleration features.

```{cpp}
/// Use video codec for faster video decoding (if supported)
int HARDWARE_DECODER = 0;

/* 0 - No acceleration
   1 - Linux VA-API
   2 - nVidia NVDEC
   3 - Windows D3D9
   4 - Windows D3D11
   5 - MacOS / VideoToolBox
   6 - Linux VDPAU
   7 - Intel QSV */

/// Number of threads of OpenMP
int OMP_THREADS = 12;

/// Number of threads that FFmpeg uses
int FF_THREADS = 8;

/// Maximum rows that hardware decode can handle
int DE_LIMIT_HEIGHT_MAX = 1100;

/// Maximum columns that hardware decode can handle
int DE_LIMIT_WIDTH_MAX = 1950;

/// Which GPU to use to decode (0 is the first, LINUX ONLY)
int HW_DE_DEVICE_SET = 0;

/// Which GPU to use to encode (0 is the first, LINUX ONLY)
int HW_EN_DEVICE_SET = 0;
```

## Libva / VA-API (Video Acceleration API)

The correct version of libva is needed (libva in Ubuntu 16.04 or libva2
in Ubuntu 18.04) for the AppImage to work with hardware acceleration.
An AppImage that works on both systems (supporting libva and libva2),
might be possible when no libva is included in the AppImage.

*  vaapi is working for intel and AMD
*  vaapi is working for decode only for nouveau
*  nVidia driver is working for export only

## AMD Graphics Cards (RadeonOpenCompute/ROCm)

Decoding and encoding on the (AMD) GPU is possible with the default drivers.
On systems where ROCm is installed and run a future use for GPU acceleration
of effects could be implemented (contributions welcome).

## Multiple Graphics Cards

If the computer has multiple graphics cards installed, you can choose which
should be used by libopenshot. Also, you can optionally use one card for
decoding and the other for encoding (if both cards support acceleration).
This is currently only supported on Linux, due to the device name FFmpeg
expects (i.e. **/dev/dri/render128**). Contributions welcome if anyone can
determine what string format to pass for Windows and Mac.

## Help Us Improve Hardware Support

This information might be wrong, and we would love to continue improving
our support for hardware acceleration in OpenShot. Please help us update
this document if you find an error or discover new and/or useful information.

**FFmpeg 4 + nVidia** The manual at:
https://www.tal.org/tutorials/ffmpeg_nvidia_encode
works pretty well. We could compile and install a version of FFmpeg 4.1.3
on Mint 19.1 that supports the GPU on nVidia cards. A version of openshot
with hardware support using these libraries could use the nVidia GPU.

**BUG:** Hardware supported decoding still has some bugs (as you can see from
the chart above). Also, the speed gains with decoding are not as great
as with encoding. Currently, if hardware decoding fails, there is no
fallback (you either get green frames or an "invalid file" error in OpenShot).
This needs to be improved to successfully fall-back to software decoding.

**Needed:**
  * A way to get options and limits of the GPU, such as
 supported dimensions (width and height).
  *  A way to list the actual Graphic Cards available to FFmpeg (for the
  user to choose which card for decoding and encoding, as opposed
  to "Graphics Card X")

**Further improvement:** Right now the frame can be decoded on the GPU, but the
frame is then copied to CPU memory for modifications. It is then copied back to
GPU memory for encoding. Using the GPU for both decoding and modifications
will make it possible to do away with these two copies. A possible solution would
be to use Vulkan compute which would be available on Linux and Windows natively
and on MacOS via MoltenVK.

## Credit

A big thanks to Peter M (https://github.com/eisneinechse) for all his work
on integrating hardware acceleration into libopenshot! The community thanks
you for this major contribution!

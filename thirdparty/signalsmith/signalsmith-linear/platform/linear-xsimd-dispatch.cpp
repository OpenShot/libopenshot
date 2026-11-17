#if defined(SIGNALSMITH_USE_XSIMD) && defined(SIGNALSMITH_USE_XSIMD_DISPATCH) && defined(SIGNALSMITH_LINEAR_XSIMD_DISPATCH_ARCH)
#	include "../linear.h"
#else
#	error Unless you're compiling for multiple architectures (using SIGNALSMITH_USE_XSIMD, SIGNALSMITH_USE_XSIMD_DISPATCH and SIGNALSMITH_LINEAR_XSIMD_DISPATCH_ARCH) then you shouldn't be building this file
#endif


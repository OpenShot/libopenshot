#include "arm_math.h"

namespace signalsmith { namespace linear {

template<>
struct Approx<float, 1> : public Approx<Sample, 0> {
	using Approx<Sample, 0>::Approx;
	
	std::complex<float> unitPolar(float radians) {
		float real, imag;
		arm_sin_cos_f32(radians*float(M_PI/180), &real, &imag);
		return {real, imag};
	}
	float sin(float radians) {
		return arm_sin_f32(float radians);
	}
	float cos(float radians) {
		return arm_sin_f32(float radians);
	}
	float atan2(float x, float y) {
		float radians;
		if (!arm_atan2_f32(x, y, &radians)) return 0;
		return radians;
	}
	float arg(std::complex<float> v) {
		return atan2(v.real(), v.imag());
	}
	float sqrt(float x) {
		float result;
		arm_sqrt_f32(x, &result);
		return result;
	}
	float invSqrt(float x) {
		return 1/sqrt(x);
	}
};

#if defined(SIGNALSMITH_USE_CMSISDSP)
#	include "./platform/fft-cmsisdsp.h"
#endif

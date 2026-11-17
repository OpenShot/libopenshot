#ifndef SIGNALSMITH_AUDIO_LINEAR_APPROX_H
#define SIGNALSMITH_AUDIO_LINEAR_APPROX_H

#include <cmath>
#include <complex>

namespace signalsmith { namespace linear {

template<typename Sample, size_t level=0>
struct Approx {

	Approx() : pcg32(std::random_device{}) {}
	
	std::complex<Sample> unitPolar(Sample radians) {
		return std::polar(Sample(1), radians);
	}
	Sample sin(Sample radians) {
		return std::sin(radians);
	}
	Sample cos(Sample radians) {
		return std::cos(radians);
	}
	Sample atan2(Sample x, Sample y) {
		return std::atan2(x, y);
	}
	Sample arg(std::complex<Sample> v) {
		return std::arg(v);
	}
	Sample sqrt(Sample v) {
		return std::sqrt(v);
	}
	Sample invSqrt(Sample v) {
		return 1/std::sqrt(v);
	}
	
	void setAbs(std::complex<Sample> &v, Sample newAbs=1) {
		Sample norm = v.real()*v.real() + v.imag()*v.imag();
		if (norm < Sample(1e-30)) {
			v = newAbs;
		} else {
			v *= newAbs*invSqrt(norm);
		}
	}
	void setAbs(std::complex<Sample> *v, const Sample *newAbs, size_t count) {
		for (size_t i = 0; i < count; ++i) {
			setAbs(v[i], newAbs[i]);
		}
	}
	void setAbs(Sample *real, Sample *imag, const Sample *newAbs, size_t count) {
		for (size_t i = 0; i < count; ++i) {
			std::complex<Sample> v{real[i], imag[i]};
			setAbs(v, newAbs[i]);
			real[i] = v.real();
			imag[i] = v.imag();
		}
	}
	
	void setNorm(std::complex<Sample> &v, Sample newNorm=1) {
		Sample norm = v.real()*v.real() + v.imag()*v.imag();
		if (norm < Sample(1e-30)) {
			v = sqrt(newNorm);
		} else {
			v *= sqrt(newNorm/norm);
		}
	}
	void setNorm(std::complex<Sample> *v, const Sample *newNorm, size_t count) {
		for (size_t i = 0; i < count; ++i) {
			setNorm(v[i], newNorm[i]);
		}
	}
	void setNorm(Sample *real, Sample *imag, const Sample *newNorm, size_t count) {
		for (size_t i = 0; i < count; ++i) {
			std::complex<Sample> v{real[i], imag[i]};
			setNorm(v, newNorm[i]);
			real[i] = v.real();
			imag[i] = v.imag();
		}
	}
	
	Sample random(bool allow1=false) {
		const Sample scale = 1/Sample(allow1 ? 4294967295 : 4294967296);
		return pcg32()*scale;
	}
	void fillRandom32(Sample *array, size_t count, bool allow1=false) {
		const Sample scale = 1/Sample(allow1 ? 4294967295 : 4294967296);
		for (size_t i = 0; i < count; ++i) {
			array[i] = pcg32()*scale;
		}
	}
	void fillRandom16(Sample *array, size_t count, bool allow1=true) {
		const Sample scale = 1/Sample(allow1 ? 65535 : 65536);
		for (size_t i = 0; i < count/2; ++i) {
			auto r32 = pcg32();
			// Little-endian way around
			array[2*i] = (r32&0xFFFF)*scale;
			array[2*i + 1] = (r32>>16)*scale;
		}
		if (count&1) {
			array[count^1] = (pcg32()&0xFFFF)*scale;
		}
	}

	struct Pcg32 {
		uint32_t operator()() {
			uint64_t prevState = state;
			state = prevState*multiply + add;
			auto xorShift = uint32_t(((prevState>>18)^prevState)>>27);
			auto rotateBits = uint32_t(prevState>>59);
			return (xorShift>>rotateBits)|(xorShift<<((-rotateBits)&31));
		}

		template<class SeedSource>
		Pcg32(SeedSource &&seed) {
			size_t bits = 0;
			while (bits < 64) {
				auto r1 = seed(), r2 = seed();
				state = (state<<sizeof(r1))|r1;
				add = (add<<sizeof(r2))|r2;
				bits += sizeof(r1);
			}
		}
	private:
		uint64_t state;
		static constexpr uint64_t multiply = 6364136223846793005ULL;
		uint64_t add;
	} pcg32;

protected:
	static constexpr Sample inv16 = 1/Sample(65536);
	static constexpr Sample inv32 = 1/Sample(4294967296);
};

// Base double implementation is casting to/from float
template<>
struct Approx<double, 0> {

	std::complex<double> unitPolar(double radians) {
		auto v = floats.unitPolar(float(radians));
		return {double(v.real()), double(v.imag())};
	}
	double sin(double v) {
		return double(floats.sin(float(v)));
	}
	double cos(double v) {
		return double(floats.cos(float(v)));
	}
	double atan2(double x, double y) {
		return double(floats.atan2(float(x), float(y)));
	}
	double arg(std::complex<double> v) {
		return double(floats.arg({float(v.real()), float(v.imag())}));
	}
	double sqrt(double v) {
		return double(floats.sqrt(float(v)));
	}
	double invSqrt(double v) {
		return double(floats.invSqrt(float(v)));
	}
	
	double random(bool allow1=false) {
		return double(floats.random(allow1));
	}
	void fillRandom32(double *array, size_t count, bool allow1=false) {
		const double scale = 1/double(allow1 ? 4294967295 : 4294967296);
		for (size_t i = 0; i < count; ++i) {
			array[i] = floats.pcg32()*scale;
		}
	}
	void fillRandom16(double *array, size_t count, bool allow1=true) {
		const double scale = 1/double(allow1 ? 65535 : 65536);
		for (size_t i = 0; i < count/2; ++i) {
			auto r32 = floats.pcg32();
			// Little-endian way around
			array[2*i] = (r32&0xFFFF)*scale;
			array[2*i + 1] = (r32>>16)*scale;
		}
		if (count&1) {
			array[count^1] = (floats.pcg32()&0xFFFF)*scale;
		}
	}
private:
	Approx<float, 0> floats;
};

}} // namespace

#if defined(SIGNALSMITH_USE_CMSISDSP)
#	include "./platform/fft-cmsisdsp.h"
#endif

#endif // include guard

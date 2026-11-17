#include "arm_math.h"

namespace signalsmith { namespace linear {

namespace _impl {
//	template<>
//	inline void complexMul<float>(std::complex<float> *a, const std::complex<float> *b, const std::complex<float> *c, size_t size) {
//		arm_cmplx_mult_cmplx_f32(reinterpret_cast<const float *>(b), reinterpret_cast<const float *>(c), reinterpret_cast<float *>(a), uint32_t(size));
//	}
//
//	// doubles
//	template<>
//	inline void complexMul<double>(std::complex<double> *a, const std::complex<double> *b, const std::complex<double> *c, size_t size) {
//		arm_cmplx_mult_cmplx_f64(reinterpret_cast<const double *>(b), reinterpret_cast<const double *>(c), reinterpret_cast<double *>(a), uint32_t(size));
//	}
}

template<>
struct Pow2FFT<float> {
	static constexpr bool prefersSplit = false;

	using Complex = std::complex<float>;

	Pow2FFT(size_t size=0) {
		resize(size);
	}
	~Pow2FFT() {
		if (simpleFFT) delete simpleFFT;
	}
	// Allow move, but not copy
	Pow2FFT(const Pow2FFT &other) = delete;
	Pow2FFT(Pow2FFT &&other) : instance(other.instance), simpleFFT(other.simpleFFT), tmp(std::move(other.tmp)) {
		// Avoid double-free
		other.simpleFFT = nullptr;
	}

	void resize(size_t size) {
		if (simpleFFT) delete simpleFFT;
		simpleFFT = nullptr;

		if (size >= minSize && size <= maxSize) {
			arm_cfft_init_f32(&instance, uint16_t(size));
			tmp.resize(size); // in case we need to translate to/from a split version
		} else {
			simpleFFT = new SimpleFFT<float>(size);
			tmp.resize(0);
		}
		tmp.shrink_to_fit();
	}

	void fft(const Complex* input, Complex* output) {
		if (simpleFFT) return simpleFFT->fft(input, output);
		if (output != input) std::memcpy(output, input, sizeof(Complex)*tmp.size());
		arm_cfft_f32(&instance, reinterpret_cast<float *>(output), 0, 1);
	}
	void fft(const float *inR, const float *inI, float *outR, float *outI) {
		if (simpleFFT) return simpleFFT->fft(inR, inI, outR, outI);
		for (size_t i = 0; i < tmp.size(); ++i) {
			tmp[i] = {inR[i], inI[i]};
		}
		arm_cfft_f32(&instance, reinterpret_cast<float *>(tmp.data()), 0, 1);
		for (size_t i = 0; i < tmp.size(); ++i) {
			auto v = tmp[i];
			outR[i] = v.real();
			outI[i] = v.imag();
		}
	}

	void ifft(const Complex* input, Complex* output) {
		if (simpleFFT) return simpleFFT->ifft(input, output);
		if (output != input) std::memcpy(output, input, sizeof(Complex)*tmp.size());
		auto *outputAsFloat = reinterpret_cast<float *>(output);
		arm_cfft_f32(&instance, outputAsFloat, 1, 1);
		arm_scale_f32(outputAsFloat, float(tmp.size()), outputAsFloat, uint32_t(tmp.size()*2));
	}
	void ifft(const float *inR, const float *inI, float *outR, float *outI) {
		if (simpleFFT) return simpleFFT->ifft(inR, inI, outR, outI);

		for (size_t i = 0; i < tmp.size(); ++i) {
			tmp[i] = {inR[i], inI[i]};
		}
		arm_cfft_f32(&instance, reinterpret_cast<float *>(tmp.data()), 1, 1);
		float scale = tmp.size();
		for (size_t i = 0; i < tmp.size(); ++i) {
			auto v = tmp[i]*scale;
			outR[i] = v.real();
			outI[i] = v.imag();
		}
	}

private:
	static constexpr size_t minSize = 32;
	static constexpr size_t maxSize = 4096;
	arm_cfft_instance_f32 instance;
	SimpleFFT<float> *simpleFFT = nullptr;
	std::vector<Complex> tmp;
};

template<>
struct Pow2RealFFT<float> {
	static constexpr bool prefersSplit = false;

	using Complex = std::complex<float>;

	Pow2RealFFT(size_t size=0) {
		resize(size);
	}
	~Pow2RealFFT() {
		if (simpleFFT) delete simpleFFT;
	}
	// Allow move, but not copy
	Pow2RealFFT(const Pow2RealFFT &other) = delete;
	Pow2RealFFT(Pow2RealFFT &&other) : instance(other.instance), simpleFFT(other.simpleFFT), tmpTime(std::move(other.tmpTime)), tmpFreq(std::move(other.tmpFreq)) {
		// Avoid double-free
		other.simpleFFT = nullptr;
	}

	void resize(size_t size) {
		if (simpleFFT) delete simpleFFT;
		simpleFFT = nullptr;

		if (size >= minSize && size <= maxSize) {
			arm_rfft_fast_init_f32(&instance, uint16_t(size));
			tmpTime.resize(size);
			tmpFreq.resize(size/2);
		} else {
			simpleFFT = new SimpleRealFFT<float>(size);
			tmpTime.resize(0);
			tmpFreq.resize(0);
		}
		tmpTime.shrink_to_fit();
		tmpFreq.shrink_to_fit();
	}

	void fft(const float *input, Complex* output) {
		if (simpleFFT) return simpleFFT->fft(input, output);
		std::memcpy(tmpTime.data(), input, sizeof(float)*tmpTime.size());
		arm_rfft_fast_f32(&instance, tmpTime.data(), reinterpret_cast<float *>(output), 0);
	}
	void fft(const float *input, float *outR, float *outI) {
		if (simpleFFT) return simpleFFT->fft(input, outR, outI);
		std::memcpy(tmpTime.data(), input, sizeof(float)*tmpTime.size());
		arm_rfft_fast_f32(&instance, tmpTime.data(), reinterpret_cast<float *>(tmpFreq.data()), 0);
		for (size_t i = 0; i < tmpFreq.size(); ++i) {
			auto v = tmpFreq[i];
			outR[i] = v.real();
			outI[i] = v.imag();
		}
	}

	void ifft(const Complex *freq, float *time) {
		if (simpleFFT) return simpleFFT->ifft(freq, time);
		std::memcpy(tmpFreq.data(), freq, sizeof(Complex)*tmpFreq.size());
		arm_rfft_fast_f32(&instance, reinterpret_cast<float *>(tmpFreq.data()), time, 1);
		arm_scale_f32(time, float(tmpTime.size()), time, uint32_t(tmpTime.size()));
	}
	void ifft(const float *inR, const float *inI, float *outR) {
		if (simpleFFT) return simpleFFT->ifft(inR, inI, outR);

		for (size_t i = 0; i < tmpFreq.size(); ++i) {
			tmpFreq[i] = {inR[i], inI[i]}; // for some reason this ends up flipped
		}
		arm_rfft_fast_f32(&instance, reinterpret_cast<float *>(tmpFreq.data()), outR, 1);
		arm_scale_f32(outR, float(tmpTime.size()), outR, uint32_t(tmpTime.size()));
	}

private:
	static constexpr size_t minSize = 32;
	static constexpr size_t maxSize = 4096;
	arm_rfft_fast_instance_f32 instance;
	SimpleRealFFT<float> *simpleFFT = nullptr;
	std::vector<float> tmpTime;
	std::vector<Complex> tmpFreq;
};

/*
template<>
struct Pow2FFT<double> {
	static constexpr bool prefersSplit = true;

	using Complex = std::complex<double>;

	Pow2FFT(size_t size=0) {
		resize(size);
	}
	~Pow2FFT() {
		if (hasSetup) vDSP_destroy_fftsetupD(fftSetup);
	}

	void resize(size_t size) {
		_size = size;
		if (hasSetup) vDSP_destroy_fftsetupD(fftSetup);
		if (!size) {
			hasSetup = false;
			return;
		}

		log2 = std::round(std::log2(size));
		fftSetup = vDSP_create_fftsetupD(log2, FFT_RADIX2);
		hasSetup = fftSetup;

		splitReal.resize(size);
		splitImag.resize(size);
	}

	void fft(const Complex* input, Complex* output) {
		DSPDoubleSplitComplex splitComplex{ splitReal.data(), splitImag.data() };
		vDSP_ctozD((DSPDoubleComplex*)input, 2, &splitComplex, 1, _size);
		vDSP_fft_zipD(fftSetup, &splitComplex, 1, log2, kFFTDirection_Forward);
		vDSP_ztocD(&splitComplex, 1, (DSPDoubleComplex*)output, 2, _size);
	}
	void fft(const double *inR, const double *inI, double *outR, double *outI) {
		DSPDoubleSplitComplex inSplit{(double *)inR, (double *)inI};
		DSPDoubleSplitComplex outSplit{outR, outI};
		vDSP_fft_zopD(fftSetup, &inSplit, 1, &outSplit, 1, log2, kFFTDirection_Forward);
	}

	void ifft(const Complex* input, Complex* output) {
		DSPDoubleSplitComplex splitComplex{ splitReal.data(), splitImag.data() };
		vDSP_ctozD((DSPDoubleComplex*)input, 2, &splitComplex, 1, _size);
		vDSP_fft_zipD(fftSetup, &splitComplex, 1, log2, kFFTDirection_Inverse);
		vDSP_ztocD(&splitComplex, 1, (DSPDoubleComplex*)output, 2, _size);
	}
	void ifft(const double *inR, const double *inI, double *outR, double *outI) {
		DSPDoubleSplitComplex inSplit{(double *)inR, (double *)inI};
		DSPDoubleSplitComplex outSplit{outR, outI};
		vDSP_fft_zopD(fftSetup, &inSplit, 1, &outSplit, 1, log2, kFFTDirection_Inverse);
	}

private:
	size_t _size = 0;
	bool hasSetup = false;
	FFTSetupD fftSetup;
	int log2 = 0;
	std::vector<double> splitReal, splitImag;
};

template<>
struct Pow2RealFFT<double> {
	static constexpr bool prefersSplit = true;

	using Complex = std::complex<double>;

	Pow2RealFFT(size_t size = 0) {
		resize(size);
	}
	~Pow2RealFFT() {
		if (hasSetup) vDSP_destroy_fftsetupD(fftSetup);
	}

	void resize(size_t size) {
		_size = size;
		if (hasSetup) vDSP_destroy_fftsetupD(fftSetup);
		if (!size) {
			hasSetup = false;
			return;
		}

		splitReal.resize(size);
		splitImag.resize(size);
		log2 = std::log2(size);
		fftSetup = vDSP_create_fftsetupD(log2, FFT_RADIX2);
		hasSetup = fftSetup;
	}

	void fft(const double* input, Complex* output) {
		double mul = 0.5f;
		vDSP_vsmulD(input, 2, &mul, splitReal.data(), 1, _size/2);
		vDSP_vsmulD(input + 1, 2, &mul, splitImag.data(), 1, _size/2);
		DSPDoubleSplitComplex tmpSplit{splitReal.data(), splitImag.data()};
		vDSP_fft_zripD(fftSetup, &tmpSplit, 1, log2, kFFTDirection_Forward);
		vDSP_ztocD(&tmpSplit, 1, (DSPDoubleComplex *)output, 2, _size/2);
	}
	void fft(const double *inR, double *outR, double *outI) {
		DSPDoubleSplitComplex outputSplit{outR, outI};
		double mul = 0.5f;
		vDSP_vsmulD(inR, 2, &mul, outR, 1, _size/2);
		vDSP_vsmulD(inR + 1, 2, &mul, outI, 1, _size/2);
		vDSP_fft_zripD(fftSetup, &outputSplit, 1, log2, kFFTDirection_Forward);
	}

	void ifft(const Complex * input, double * output) {
		DSPDoubleSplitComplex tmpSplit{splitReal.data(), splitImag.data()};
		vDSP_ctozD((DSPDoubleComplex*)input, 2, &tmpSplit, 1, _size/2);
		vDSP_fft_zripD(fftSetup, &tmpSplit, 1, log2, kFFTDirection_Inverse);
		DSPDoubleSplitComplex outputSplit{output, output + 1};
		vDSP_zvmovD(&tmpSplit, 1, &outputSplit, 2, _size/2);
	}
	void ifft(const double *inR, const double *inI, double *outR) {
		DSPDoubleSplitComplex inputSplit{(double *)inR, (double *)inI};
		DSPDoubleSplitComplex tmpSplit{splitReal.data(), splitImag.data()};
		vDSP_fft_zropD(fftSetup, &inputSplit, 1, &tmpSplit, 1, log2, kFFTDirection_Inverse);
		DSPDoubleSplitComplex outputSplit{outR, outR + 1};
		// We can't use vDSP_ztoc without knowing the alignment
		vDSP_zvmovD(&tmpSplit, 1, &outputSplit, 2, _size/2);
	}

private:
	size_t _size = 0;
	bool hasSetup = false;
	FFTSetupD fftSetup;
	int log2 = 0;
	std::vector<double> splitReal, splitImag;
};
*/

}} // namespace

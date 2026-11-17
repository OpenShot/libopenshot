#ifndef SIGNALSMITH_LINEAR_PLATFORM_FFT_LINEAR_H
#define SIGNALSMITH_LINEAR_PLATFORM_FFT_LINEAR_H

#include "../linear.h"

namespace signalsmith { namespace linear {

namespace _impl_linear_fft {

/// SimpleFFT, but rewritten to use some Linear functions
template<typename Sample>
struct LinearFFT {
	static constexpr bool prefersSplit = true;

	using Complex = std::complex<Sample>;
	
	LinearFFT(size_t size=0) {
		resize(size);
	}
	
	void resize(size_t size) {
		twiddles.resize(size*3/4);
		for (size_t i = 0; i < size*3/4; ++i) {
			Sample twiddlePhase = -2*M_PI*i/size;
			twiddles[i] = std::polar(Sample(1), twiddlePhase);
		}
		working.resize(size);
	}
	
	void fft(const Complex *time, Complex *freq) {
		size_t size = working.size();
		if (size <= 1) {
			*freq = *time;
			return;
		}
		fftPass<false>(size, 1, time, freq, working.data());
	}

	void ifft(const Complex *freq, Complex *time) {
		size_t size = working.size();
		if (size <= 1) {
			*time = *freq;
			return;
		}
		fftPass<true>(size, 1, freq, time, working.data());
	}

	void fft(const Sample *inR, const Sample *inI, Sample *outR, Sample *outI) {
		size_t size = working.size();
		if (size <= 1) {
			*outR = *inR;
			*outI = *inI;
			return;
		}
		Sample *workingR = (Sample *)working.data(), *workingI = workingR + size;
		fftPass<false>(size, 1, inR, inI, outR, outI, workingR, workingI);
	}
	void ifft(const Sample *inR, const Sample *inI, Sample *outR, Sample *outI) {
		size_t size = working.size();
		if (size <= 1) {
			*outR = *inR;
			*outI = *inI;
			return;
		}
		Sample *workingR = (Sample *)working.data(), *workingI = workingR + size;
		fftPass<true>(size, 1, inR, inI, outR, outI, workingR, workingI);
	}
private:
	std::vector<Complex> twiddles;
	std::vector<Complex> working;
	
	template<bool conjB>
	static Complex mul(const Complex &a, const Complex &b) {
		return conjB ? Complex{
			a.real()*b.real() + a.imag()*b.imag(),
			a.imag()*b.real() - a.real()*b.imag()
		} : Complex{
			a.real()*b.real() - a.imag()*b.imag(),
			a.imag()*b.real() + a.real()*b.imag()
		};
	}

	// Calculate a [size]-point FFT, where each element is a block of [stride] values
	template<bool inverse>
	void fftPass(size_t size, size_t stride, const Complex *input, Complex *output, Complex *working) {
		if (size/4 > 1) {
			// Calculate four quarter-size FFTs
			fftPass<inverse>(size/4, stride*4, input, working, output);
			combine4<inverse>(size, stride, working, output);
		} else if (size == 4) {
			combine4<inverse>(4, stride, input, output);
		} else {
			// 2-point FFT
			for (size_t s = 0; s < stride; ++s) {
				Complex a = input[s];
				Complex b = input[s + stride];
				output[s] = a + b;
				output[s + stride] = a - b;
			}
		}
	}

	// Combine interleaved results into a single spectrum
	template<bool inverse>
	void combine4(size_t size, size_t stride, const Complex *input, Complex *output) const {
		auto twiddleStep = working.size()/size;
		for (size_t i = 0; i < size/4; ++i) {
			Complex twiddleB = twiddles[i*twiddleStep];
			Complex twiddleC = twiddles[i*2*twiddleStep];
			Complex twiddleD = twiddles[i*3*twiddleStep];
			
			const Complex *inputA = input + 4*i*stride;
			const Complex *inputB = input + (4*i + 1)*stride;
			const Complex *inputC = input + (4*i + 2)*stride;
			const Complex *inputD = input + (4*i + 3)*stride;
			Complex *outputA = output + i*stride;
			Complex *outputB = output + (i + size/4)*stride;
			Complex *outputC = output + (i + size/4*2)*stride;
			Complex *outputD = output + (i + size/4*3)*stride;
			for (size_t s = 0; s < stride; ++s) {
				Complex a = inputA[s];
				Complex b = mul<inverse>(inputB[s], twiddleB);
				Complex c = mul<inverse>(inputC[s], twiddleC);
				Complex d = mul<inverse>(inputD[s], twiddleD);
				Complex ac0 = a + c, ac1 = a - c;
				Complex bd0 = b + d, bd1 = inverse ? (b - d) : (d - b);
				Complex bd1i = {-bd1.imag(), bd1.real()};
				outputA[s] = ac0 + bd0;
				outputB[s] = ac1 + bd1i;
				outputC[s] = ac0 - bd0;
				outputD[s] = ac1 - bd1i;
			}
		}
	}

	// The same thing, but translated for split-complex input/output
	template<bool inverse>
	void fftPass(size_t size, size_t stride, const Sample *inputR, const Sample *inputI, Sample *outputR, Sample *outputI, Sample *workingR, Sample *workingI) const {
		if (size/4 > 1) {
			// Calculate four quarter-size FFTs
			fftPass<inverse>(size/4, stride*4, inputR, inputI, workingR, workingI, outputR, outputI);
			combine4<inverse>(size, stride, workingR, workingI, outputR, outputI);
		} else if (size == 4) {
			combine4<inverse>(4, stride, inputR, inputI, outputR, outputI);
		} else {
			// 2-point FFT
			for (size_t s = 0; s < stride; ++s) {
				Sample ar = inputR[s], ai = inputI[s];
				Sample br = inputR[s + stride], bi = inputI[s + stride];
				outputR[s] = ar + br;
				outputI[s] = ai + bi;
				outputR[s + stride] = ar - br;
				outputI[s + stride] = ai - bi;
			}
		}
	}

	// Combine interleaved results into a single spectrum
	template<bool inverse>
	void combine4(size_t size, size_t stride, const Sample *inputR, const Sample *inputI, Sample *outputR, Sample *outputI) const {
		auto twiddleStep = working.size()/size;
		for (size_t i = 0; i < size/4; ++i) {
			Complex twiddleB = twiddles[i*twiddleStep];
			Complex twiddleC = twiddles[i*2*twiddleStep];
			Complex twiddleD = twiddles[i*3*twiddleStep];
			
			const Sample *inputAr = inputR + 4*i*stride, *inputAi = inputI + 4*i*stride;
			const Sample *inputBr = inputR + (4*i + 1)*stride, *inputBi = inputI + (4*i + 1)*stride;
			const Sample *inputCr = inputR + (4*i + 2)*stride, *inputCi = inputI + (4*i + 2)*stride;
			const Sample *inputDr = inputR + (4*i + 3)*stride, *inputDi = inputI + (4*i + 3)*stride;
			Sample *outputAr = outputR + i*stride, *outputAi = outputI + i*stride;
			Sample *outputBr = outputR + (i + size/4)*stride, *outputBi = outputI + (i + size/4)*stride;
			Sample *outputCr = outputR + (i + size/4*2)*stride, *outputCi = outputI + (i + size/4*2)*stride;
			Sample *outputDr = outputR + (i + size/4*3)*stride, *outputDi = outputI + (i + size/4*3)*stride;
			for (size_t s = 0; s < stride; ++s) {
				Complex a = {inputAr[s], inputAi[s]};
				Complex b = mul<inverse>({inputBr[s], inputBi[s]}, twiddleB);
				Complex c = mul<inverse>({inputCr[s], inputCi[s]}, twiddleC);
				Complex d = mul<inverse>({inputDr[s], inputDi[s]}, twiddleD);
				Complex ac0 = a + c, ac1 = a - c;
				Complex bd0 = b + d, bd1 = inverse ? (b - d) : (d - b);
				Complex bd1i = {-bd1.imag(), bd1.real()};
				outputAr[s] = ac0.real() + bd0.real();
				outputAi[s] = ac0.imag() + bd0.imag();
				outputBr[s] = ac1.real() + bd1i.real();
				outputBi[s] = ac1.imag() + bd1i.imag();
				outputCr[s] = ac0.real() - bd0.real();
				outputCi[s] = ac0.imag() - bd0.imag();
				outputDr[s] = ac1.real() - bd1i.real();
				outputDi[s] = ac1.imag() - bd1i.imag();
			}
		}
	}
};

} // namespace

template<>
struct Pow2FFT<float> : public _impl_linear_fft::LinearFFT<float> {
private:
	using Super = _impl_linear_fft::LinearFFT<float>;
public:
	using Complex = std::complex<float>;

	Pow2FFT(size_t size=0) : Super(size) {}

	// Allow move, but not copy
	Pow2FFT(const Pow2FFT &other) = delete;
	Pow2FFT(Pow2FFT &&other) : Super(std::move(other)) {}
};

}} // namespace
#endif // include guard

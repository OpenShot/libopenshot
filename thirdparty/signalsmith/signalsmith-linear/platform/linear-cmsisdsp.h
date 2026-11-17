#include "arm_math.h"

namespace signalsmith { namespace linear {

template<>
struct LinearImpl<true> : public LinearImplBase<true> {
	using Base = LinearImplBase<true>;

	LinearImpl() : Base(this), cached(*this) {}

	template<class V>
	void reserve(size_t size) {
		// assume float/doubles if the right size
		if (sizeof(V) == sizeof(float)) {
			cached.reserveFloats(size*4);
		} else if (sizeof(V) == sizeof(double)) {
			cached.reserveDoubles(size*4);
		}
	}

	template<class Pointer, class Expr>
	void fill(Pointer pointer, Expr expr, size_t size) {
		fillExpr(pointer, expr, size);
	}

	template<class Pointer, class Expr>
	void fill(Pointer pointer, Expression<Expr> expr, size_t size) {
		return fillExpr(pointer, (Expr &)expr, size);
	};
	template<class Pointer, class Expr>
	void fill(Pointer pointer, WritableExpression<Expr> expr, size_t size) {
		return fillExpr(pointer, (Expr &)expr, size);
	};

private:
	// Most generic fill
	template<class Pointer, class Expr>
	void fillExpr(Pointer pointer, Expr expr, size_t size) {
		fillBasic(pointer, expr, size);
	}

	template<class Pointer, class Expr>
	void fillBasic(Pointer pointer, Expr expr, size_t size) {
		for (size_t i = 0; i < size; ++i) {
			pointer[i] = expr.get(i);
		}
	}
	template<class V, class Expr>
	void fillBasic(SplitPointer<V> pointer, Expr expr, size_t size) {
		using Complex = typename SplitPointer<V>::Complex;
		for (size_t i = 0; i < size; ++i) {
			Complex c = expr.get(i);
			pointer.real[i] = c.real();
			pointer.imag[i] = c.imag();
		}
	}
	
	template<typename V>
	void clear(V *v, size_t size) {
		for (size_t i = 0; i < size; ++i) v[i] = 0;
	}
	void clear(float *v, size_t size) {
		arm_fill_f32(0.0f, v, uint32_t(size));
	}
	void clear(double *v, size_t size) {
		for (size_t i = 0; i < size; ++i) v[i] = 0;
	}
	// Filling a split-complex vector with real values won't hit the specialisations below, so we handle it here
	template<class Expr>
	ItemType<Expr, float, void> fillBasic(SplitPointer<float> pointer, Expr expr, size_t size) {
		fillExpr(pointer.real, expr, size);
		clear(pointer.imag, size);
	}
	template<class Expr>
	ItemType<Expr, double, void> fillBasic(SplitPointer<double> pointer, Expr expr, size_t size) {
		fillExpr(pointer.real, expr, size);
		clear(pointer.imag, size);
	}
	
	// Copying from existing pointer
	void fillExpr(RealPointer<float> pointer, expression::ReadableReal<float> expr, size_t size) {
		arm_copy_f32(expr.pointer, pointer, uint32_t(size));
	}
	void fillExpr(RealPointer<float> pointer, WritableReal<float> expr, size_t size) {
		arm_copy_f32(expr.pointer, pointer, uint32_t(size));
	}
	void fillExpr(RealPointer<double> pointer, expression::ReadableReal<double> expr, size_t size) {
		arm_copy_f64(expr.pointer, pointer, uint32_t(size));
	}
	void fillExpr(RealPointer<double> pointer, WritableReal<double> expr, size_t size) {
		arm_copy_f64(expr.pointer, pointer, uint32_t(size));
	}
	void fillExpr(ComplexPointer<float> pointer, expression::ReadableComplex<float> expr, size_t size) {
		arm_copy_f32(reinterpret_cast<const float *>(expr.pointer), reinterpret_cast<float *>(pointer), uint32_t(size*2));
	}
	void fillExpr(ComplexPointer<float> pointer, WritableComplex<float> expr, size_t size) {
		arm_copy_f32(reinterpret_cast<const float *>(expr.pointer), reinterpret_cast<float *>(pointer), uint32_t(size*2));
	}
	void fillExpr(ComplexPointer<double> pointer, expression::ReadableComplex<double> expr, size_t size) {
		arm_copy_f64(reinterpret_cast<const double *>(expr.pointer), reinterpret_cast<double *>(pointer), uint32_t(size*2));
	}
	void fillExpr(ComplexPointer<double> pointer, WritableComplex<double> expr, size_t size) {
		arm_copy_f64(reinterpret_cast<const double *>(expr.pointer), reinterpret_cast<double *>(pointer), uint32_t(size*2));
	}
	void fillExpr(SplitPointer<float> pointer, expression::ReadableSplit<float> expr, size_t size) {
		arm_copy_f32(expr.pointer.real, pointer.real, uint32_t(size));
		arm_copy_f32(expr.pointer.imag, pointer.imag, uint32_t(size));
	}
	void fillExpr(SplitPointer<float> pointer, WritableSplit<float> expr, size_t size) {
		arm_copy_f32(expr.pointer.real, pointer.real, uint32_t(size));
		arm_copy_f32(expr.pointer.imag, pointer.imag, uint32_t(size));
	}
	void fillExpr(SplitPointer<double> pointer, expression::ReadableSplit<double> expr, size_t size) {
		arm_copy_f64(expr.pointer.real, pointer.real, uint32_t(size));
		arm_copy_f64(expr.pointer.imag, pointer.imag, uint32_t(size));
	}
	void fillExpr(SplitPointer<double> pointer, WritableSplit<double> expr, size_t size) {
		arm_copy_f64(expr.pointer.real, pointer.real, uint32_t(size));
		arm_copy_f64(expr.pointer.imag, pointer.imag, uint32_t(size));
	}

	// Real part of a split pointer
	void fillExpr(RealPointer<float> pointer, expression::Real<expression::ReadableSplit<float>> expr, size_t size) {
		arm_copy_f32(expr.a.pointer.real, pointer, uint32_t(size));
	}
	void fillExpr(RealPointer<double> pointer, expression::Real<expression::ReadableSplit<double>> expr, size_t size) {
		arm_copy_f64(expr.a.pointer.real, pointer, uint32_t(size));
	}
	void fillExpr(RealPointer<float> pointer, expression::Real<WritableSplit<float>> expr, size_t size) {
		arm_copy_f32(expr.a.pointer.real, pointer, uint32_t(size));
	}
	void fillExpr(RealPointer<double> pointer, expression::Real<WritableSplit<double>> expr, size_t size) {
		arm_copy_f64(expr.a.pointer.real, pointer, uint32_t(size));
	}
	// Imaginary part
	void fillExpr(RealPointer<float> pointer, expression::Imag<expression::ReadableSplit<float>> expr, size_t size) {
		arm_copy_f32(expr.a.pointer.imag, pointer, uint32_t(size));
	}
	void fillExpr(RealPointer<double> pointer, expression::Imag<expression::ReadableSplit<double>> expr, size_t size) {
		arm_copy_f64(expr.a.pointer.imag, pointer, uint32_t(size));
	}
	void fillExpr(RealPointer<float> pointer, expression::Imag<WritableSplit<float>> expr, size_t size) {
		arm_copy_f32(expr.a.pointer.imag, pointer, uint32_t(size));
	}
	void fillExpr(RealPointer<double> pointer, expression::Imag<WritableSplit<double>> expr, size_t size) {
		arm_copy_f64(expr.a.pointer.imag, pointer, uint32_t(size));
	}
	// Filling with a constant
	template<class V>
	void fillExpr(RealPointer<float> pointer, expression::ConstantExpr<V> expr, size_t size) {
		float v = expr.value;
		arm_fill_f32(v, pointer, uint32_t(size));
	}
	template<class V>
	void fillExpr(RealPointer<double> pointer, expression::ConstantExpr<V> expr, size_t size) {
		double v = expr.value;
		arm_fill_f64(v, pointer, uint32_t(size));
	}
//	template<class V>
//	void fillExpr(ComplexPointer<float> pointer, expression::ConstantExpr<V> expr, size_t size) {
//		std::complex<float> v = expr.value;
//		auto vSplit = dspSplit(&v);
//		auto pSplit = dspSplit(pointer);
//		vDSP_zvfill(&vSplit, &pSplit, 2, size);
//	}
//	template<class V>
//	void fillExpr(ComplexPointer<double> pointer, expression::ConstantExpr<V> expr, size_t size) {
//		std::complex<double> v = expr.value;
//		auto vSplit = dspSplit(&v);
//		auto pSplit = dspSplit(pointer);
//		vDSP_zvfillD(&vSplit, &pSplit, 2, size);
//	}
	template<class V>
	void fillExpr(SplitPointer<float> pointer, expression::ConstantExpr<V> expr, size_t size) {
		std::complex<float> v = expr.value;
		float vr = v.real(), vi = v.imag();
		arm_fill_f32(vr, pointer.real, uint32_t(size));
		arm_fill_f32(vi, pointer.imag, uint32_t(size));
	}
	template<class V>
	void fillExpr(SplitPointer<double> pointer, expression::ConstantExpr<V> expr, size_t size) {
		std::complex<double> v = expr.value;
		double vr = v.real(), vi = v.imag();
		arm_fill_f64(vr, pointer.real, uint32_t(size));
		arm_fill_f64(vi, pointer.imag, uint32_t(size));
	}

// Forwards .fillExpr() to .fillName(), but doesn't define that
#define SIGNALSMITH_AUDIO_LINEAR_OP1_R(Name) \
	template<class A> \
	void fillExpr(RealPointer<float> pointer, expression::Name<A> expr, size_t size) { \
		fill##Name(pointer, expr, size); \
	} \
	template<class A> \
	void fillExpr(RealPointer<double> pointer, expression::Name<A> expr, size_t size) { \
		fill##Name(pointer, expr, size); \
	} \
	template<class Expr> \
	void fill##Name(RealPointer<float> pointer, Expr expr, size_t size) { \
		fillBasic(pointer, expr, size); \
	} \
	template<class Expr> \
	void fill##Name(RealPointer<double> pointer, Expr expr, size_t size) { \
		fillBasic(pointer, expr, size); \
	}
#define SIGNALSMITH_AUDIO_LINEAR_OP1_C(Name) \
	template<class A> \
	void fillExpr(ComplexPointer<float> pointer, expression::Name<A> expr, size_t size) { \
		fill##Name(pointer, expr, size); \
	} \
	template<class A> \
	void fillExpr(ComplexPointer<double> pointer, expression::Name<A> expr, size_t size) { \
		fill##Name(pointer, expr, size); \
	} \
	template<class Expr> \
	void fill##Name(ComplexPointer<float> pointer, Expr expr, size_t size) { \
		fillBasic(pointer, expr, size); \
	} \
	template<class Expr> \
	void fill##Name(ComplexPointer<double> pointer, Expr expr, size_t size) { \
		fillBasic(pointer, expr, size); \
	} \
	template<class A> \
	void fillExpr(SplitPointer<float> pointer, expression::Name<A> expr, size_t size) { \
		fill##Name(pointer, expr, size); \
	} \
	template<class A> \
	void fillExpr(SplitPointer<double> pointer, expression::Name<A> expr, size_t size) { \
		fill##Name(pointer, expr, size); \
	} \
	template<class Expr> \
	void fill##Name(SplitPointer<float> pointer, Expr expr, size_t size) { \
		fillBasic(pointer, expr, size); \
	} \
	template<class Expr> \
	void fill##Name(SplitPointer<double> pointer, Expr expr, size_t size) { \
		fillBasic(pointer, expr, size); \
	}
// R -> R operators
#define SIGNALSMITH_AUDIO_LINEAR_TREE1_RR(Name, cmsis_prefix) \
	template<class A> \
	ItemType<A, float, void> fill##Name(RealPointer<float> pointer, expression::Name<A> expr, size_t size) { \
		auto floats = cached.floatScope(); \
		auto *a = floats.real(expr.a, size); \
		cmsis_prefix##_f32(a, pointer, uint32_t(size)); \
	} \
	template<class A> \
	ItemType<A, double, void> fill##Name(RealPointer<double> pointer, expression::Name<A> expr, size_t size) { \
		auto doubles = cached.doubleScope(); \
		auto *a = doubles.real(expr.a, size); \
		cmsis_prefix##_f64(a, pointer, uint32_t(size)); \
	}
// C -> C operators which are the same as element-wise R -> R ones
#define SIGNALSMITH_AUDIO_LINEAR_TREE1_CCe(Name, cmsis_prefix) \
	template<class A> \
	ItemType<A, std::complex<float>, void> fill##Name(ComplexPointer<float> pointer, expression::Name<A> expr, size_t size) { \
		auto floats = cached.floatScope(); \
		auto *a = floats.complex(expr.a, size); \
		cmsis_prefix##_f32(reinterpret_cast<const float *>(a), reinterpret_cast<float *>(pointer), size*2); \
	} \
	template<class A> \
	ItemType<A, std::complex<double>, void> fill##Name(ComplexPointer<double> pointer, expression::Name<A> expr, size_t size) { \
		auto doubles = cached.doubleScope(); \
		auto *a = doubles.complex(expr.a, size); \
		cmsis_prefix##_f64(reinterpret_cast<const double *>(a), reinterpret_cast<double *>(pointer), size*2); \
	}\
	template<class A> \
	ItemType<A, std::complex<float>, void> fill##Name(SplitPointer<float> pointer, expression::Name<A> expr, size_t size) { \
		auto floats = cached.floatScope(); \
		auto a = floats.split(expr.a, size); \
		cmsis_prefix##_f32(a.real, pointer.real, size); \
		cmsis_prefix##_f32(a.imag, pointer.imag, size); \
	} \
	template<class A> \
	ItemType<A, std::complex<double>, void> fill##Name(SplitPointer<double> pointer, expression::Name<A> expr, size_t size) { \
		auto doubles = cached.doubleScope(); \
		auto a = doubles.split(expr.a, size); \
		cmsis_prefix##_f64(a.real, pointer.real, size); \
		cmsis_prefix##_f64(a.imag, pointer.imag, size); \
	}
// C -> R operators
#define SIGNALSMITH_AUDIO_LINEAR_TREE1_RC(Name, cmsis_prefix) \
	template<class A> \
	ItemType<A, std::complex<float>, void> fill##Name(RealPointer<float> pointer, expression::Name<A> expr, size_t size) { \
		auto floats = cached.floatScope(); \
		auto *a = floats.complex(expr.a, size); \
		cmsis_prefix##_f32(reinterpret_cast<const float *>(a), pointer, size); \
	} \
	template<class A> \
	ItemType<A, std::complex<double>, void> fill##Name(RealPointer<double> pointer, expression::Name<A> expr, size_t size) { \
		auto doubles = cached.doubleScope(); \
		auto *a = doubles.complex(expr.a, size); \
		cmsis_prefix##_f64(reinterpret_cast<const double *>(a), pointer, size); \
	}
// C -> C operators
#define SIGNALSMITH_AUDIO_LINEAR_TREE1_CC(Name, cmsis_prefix) \
	template<class A> \
	ItemType<A, std::complex<float>, void> fill##Name(ComplexPointer<float> pointer, expression::Name<A> expr, size_t size) { \
		auto floats = cached.floatScope(); \
		auto *a = floats.complex(expr.a, size, pointer); \
		cmsis_prefix##_f32(reinterpret_cast<const float *>(a), reinterpret_cast<float *>(pointer), size); \
	} \
	template<class A> \
	ItemType<A, std::complex<double>, void> fill##Name(ComplexPointer<double> pointer, expression::Name<A> expr, size_t size) { \
		auto doubles = cached.doubleScope(); \
		auto *a = doubles.complex(expr.a, size, pointer); \
		cmsis_prefix##_f64(reinterpret_cast<const double *>(a), reinterpret_cast<double *>(pointer), size); \
	}\
	template<class A> \
	ItemType<A, std::complex<float>, void> fill##Name(SplitPointer<float> pointer, expression::Name<A> expr, size_t size) { \
		auto floats = cached.floatScope(); \
		auto *a = floats.complex(expr.a, size); \
		auto *r = floats(size*2); \
		cmsis_prefix##_f32(reinterpret_cast<const float *>(a), r, size); \
		deinterleave(r, pointer.real, pointer.imag, size); \
	} \
	template<class A> \
	ItemType<A, std::complex<double>, void> fill##Name(SplitPointer<double> pointer, expression::Name<A> expr, size_t size) { \
		auto doubles = cached.doubleScope(); \
		auto *a = doubles.complex(expr.a, size); \
		auto *r = doubles(size*2); \
		cmsis_prefix##_f64(reinterpret_cast<const double *>(a), r, size); \
		deinterleave(r, pointer.real, pointer.imag, size); \
	}
	SIGNALSMITH_AUDIO_LINEAR_OP1_R(Abs)
	SIGNALSMITH_AUDIO_LINEAR_TREE1_RR(Abs, arm_abs);
	SIGNALSMITH_AUDIO_LINEAR_TREE1_RC(Abs, arm_cmplx_mag);
	SIGNALSMITH_AUDIO_LINEAR_OP1_R(Neg)
	SIGNALSMITH_AUDIO_LINEAR_OP1_C(Neg)
	SIGNALSMITH_AUDIO_LINEAR_TREE1_RR(Neg, arm_negate);
	SIGNALSMITH_AUDIO_LINEAR_TREE1_CCe(Neg, arm_negate);
	SIGNALSMITH_AUDIO_LINEAR_OP1_R(Norm)
	// Norm of real number just squares it
	template<class A>
	ItemType<A, float, void> fillNorm(RealPointer<float> pointer, expression::Norm<A> expr, size_t size) {
		auto floats = cached.floatScope();
		auto *a = floats.real(expr.a, size, pointer);
		arm_mult_f32(a, a, pointer, uint32_t(size));
	}
	template<class A>
	ItemType<A, double, void> fillNorm(RealPointer<double> pointer, expression::Norm<A> expr, size_t size) {
		auto doubles = cached.doubleScope();
		auto *a = doubles.real(expr.a, size, pointer);
		arm_mult_f64(a, a, pointer, uint32_t(size));
	}
	// Special-case for when taking the norm of a split-pointer
	void fillNorm(RealPointer<float> pointer, expression::Norm<expression::ReadableSplit<float>> expr, size_t size) {
		auto real = expression::ReadableReal<float>(expr.a.pointer.real);
		auto imag = expression::ReadableReal<float>(expr.a.pointer.imag);
		auto newExpr = expression::makeAdd(expression::makeMul(real, real), expression::makeMul(imag, imag));
		return fillExpr(pointer, newExpr, size);
	}
	void fillNorm(RealPointer<float> pointer, expression::Norm<WritableSplit<float>> expr, size_t size) {
		auto real = expression::ReadableReal<float>(expr.a.pointer.real);
		auto imag = expression::ReadableReal<float>(expr.a.pointer.imag);
		auto newExpr = expression::makeAdd(expression::makeMul(real, real), expression::makeMul(imag, imag));
		return fillExpr(pointer, newExpr, size);
	}
	void fillNorm(RealPointer<double> pointer, expression::Norm<expression::ReadableSplit<double>> expr, size_t size) {
		auto real = expression::ReadableReal<double>(expr.a.pointer.real);
		auto imag = expression::ReadableReal<double>(expr.a.pointer.imag);
		auto newExpr = expression::makeAdd(expression::makeMul(real, real), expression::makeMul(imag, imag));
		return fillExpr(pointer, newExpr, size);
	}
	void fillNorm(RealPointer<double> pointer, expression::Norm<WritableSplit<double>> expr, size_t size) {
		auto real = expression::ReadableReal<double>(expr.a.pointer.real);
		auto imag = expression::ReadableReal<double>(expr.a.pointer.imag);
		auto newExpr = expression::makeAdd(expression::makeMul(real, real), expression::makeMul(imag, imag));
		return fillExpr(pointer, newExpr, size);
	}
	SIGNALSMITH_AUDIO_LINEAR_TREE1_RC(Norm, arm_cmplx_mag_squared);
	SIGNALSMITH_AUDIO_LINEAR_OP1_C(Conj)
	template<class A> \
	void fillConj(ComplexPointer<float> pointer, expression::Conj<A> expr, size_t size) {
		auto floats = cached.floatScope();
		auto *a = floats.complex(expr.a, size, pointer);
		arm_cmplx_conj_f32(reinterpret_cast<const float *>(a), reinterpret_cast<float *>(pointer), size);
	}
	template<class A>
	void fillConj(ComplexPointer<double> pointer, expression::Conj<A> expr, size_t size) {
		this->fill(pointer, expr.a, size);
		auto imagPtr = reinterpret_cast<double *>(pointer) + 1;
		// There is no arm_cmplx_conj_f64, do a manual loop
		for (size_t i = 0; i < size; ++i) {
			double v = imagPtr[2*i];
			imagPtr[2*i] = -v;
		}
	}
	template<class A>
	void fillConj(SplitPointer<float> pointer, expression::Conj<A> expr, size_t size) {
		this->fill(pointer, expr.a, size);
		arm_negate_f32(pointer.imag, pointer.imag, uint32_t(size));
	}
	template<class A>
	void fillConj(SplitPointer<double> pointer, expression::Conj<A> expr, size_t size) {
		this->fill(pointer, expr.a, size);
		arm_negate_f64(pointer.imag, pointer.imag, uint32_t(size));
	}
	SIGNALSMITH_AUDIO_LINEAR_OP1_R(Exp)
	template<class A>
	void fillExp(RealPointer<float> pointer, expression::Exp<A> expr, size_t size) {
		auto floats = cached.floatScope();
		auto *a = floats.real(expr.a, size, pointer);
		arm_vexp_f32(a, pointer, uint32_t(size));
	}
	SIGNALSMITH_AUDIO_LINEAR_OP1_R(Exp2)
	template<class A>
	void fillExp2(RealPointer<float> pointer, expression::Exp2<A> expr, size_t size) {
		auto floats = cached.floatScope();
		auto *a = floats.real(expr.a, size, pointer);
		arm_scale_f32(a, 0.6931471805599453f, pointer, uint32_t(size));
		arm_vexp_f32(pointer, pointer, uint32_t(size));
	}
	SIGNALSMITH_AUDIO_LINEAR_OP1_R(Log)
	template<class A>
	void fillLog(RealPointer<float> pointer, expression::Log<A> expr, size_t size) {
		auto floats = cached.floatScope();
		auto *a = floats.real(expr.a, size, pointer);
		arm_vlog_f32(a, pointer, uint32_t(size));
	}
	SIGNALSMITH_AUDIO_LINEAR_OP1_R(Log2)
	template<class A>
	void fillLog2(RealPointer<float> pointer, expression::Log2<A> expr, size_t size) {
		auto logExpr = expression::makeLog(expr.a);
		fillBasic(pointer, logExpr, size);
		arm_scale_f32(pointer, 1.4426950408889634f, pointer, uint32_t(size));
	}
	SIGNALSMITH_AUDIO_LINEAR_OP1_R(Log10)
	template<class A>
	void fillLog10(RealPointer<float> pointer, expression::Log10<A> expr, size_t size) {
		auto logExpr = expression::makeLog(expr.a);
		fillBasic(pointer, logExpr, size);
		arm_scale_f32(pointer, 0.43429448190325176f, pointer, uint32_t(size));
	}

	SIGNALSMITH_AUDIO_LINEAR_OP1_R(Sqrt)
	template<class A>
	ItemType<A, float, void> fillSqrt(RealPointer<float> pointer, expression::Sqrt<A> expr, size_t size) {
		auto floats = cached.floatScope();
		auto *a = floats.real(expr.a, size, pointer);
		// Use their fast-sqrt function
		for (size_t i = 0; i < size; ++i) {
			arm_sqrt_f32(a[i], &pointer[i]);
		}
	}
	SIGNALSMITH_AUDIO_LINEAR_OP1_R(Sin)
	template<class A>
	ItemType<A, float, void> fillSin(RealPointer<float> pointer, expression::Sin<A> expr, size_t size) {
		auto floats = cached.floatScope();
		auto *a = floats.real(expr.a, size, pointer);
		// Use their fast-sqrt function
		for (size_t i = 0; i < size; ++i) {
			pointer[i] = arm_sin_f32(a[i]);
		}
	}
	SIGNALSMITH_AUDIO_LINEAR_OP1_R(Cos)
	template<class A>
	ItemType<A, float, void> fillSin(RealPointer<float> pointer, expression::Cos<A> expr, size_t size) {
		auto floats = cached.floatScope();
		auto *a = floats.real(expr.a, size, pointer);
		// Use their fast-sqrt function
		for (size_t i = 0; i < size; ++i) {
			pointer[i] = arm_cos_f32(a[i]);
		}
	}
//	SIGNALSMITH_AUDIO_LINEAR_OP1_R(Ceil)
//	SIGNALSMITH_AUDIO_LINEAR_TREE1_RR(Ceil, vvceilf, vvceil);
//	SIGNALSMITH_AUDIO_LINEAR_OP1_R(Floor)
//	SIGNALSMITH_AUDIO_LINEAR_TREE1_RR(Floor, vvfloorf, vvfloor);
#undef SIGNALSMITH_AUDIO_LINEAR_OP1_R
#undef SIGNALSMITH_AUDIO_LINEAR_TREE1_RR
#undef SIGNALSMITH_AUDIO_LINEAR_TREE1_RC
#undef SIGNALSMITH_AUDIO_LINEAR_TREE1_CC

// Forwards .fillExpr() to .fillName(), but doesn't define that
#define SIGNALSMITH_AUDIO_LINEAR_OP2_R(Name) \
public: \
	template<class A, class B> \
	void fillExpr(RealPointer<float> pointer, expression::Name<A, B> expr, size_t size) { \
		fill##Name(pointer, expr, size); \
	} \
	template<class A, class B> \
	void fillExpr(RealPointer<double> pointer, expression::Name<A, B> expr, size_t size) { \
		fill##Name(pointer, expr, size); \
	} \
private:
#define SIGNALSMITH_AUDIO_LINEAR_OP2_C(Name) \
public: \
	template<class A, class B> \
	void fillExpr(ComplexPointer<float> pointer, expression::Name<A, B> expr, size_t size) { \
		fill##Name(pointer, expr, size); \
	} \
	template<class A, class B> \
	void fillExpr(ComplexPointer<double> pointer, expression::Name<A, B> expr, size_t size) { \
		fill##Name(pointer, expr, size); \
	} \
	template<class A, class B> \
	void fillExpr(SplitPointer<float> pointer, expression::Name<A, B> expr, size_t size) { \
		fill##Name(pointer, expr, size); \
	} \
	template<class A, class B> \
	void fillExpr(SplitPointer<double> pointer, expression::Name<A, B> expr, size_t size) { \
		fill##Name(pointer, expr, size); \
	} \
private:
// R x R -> R
#define SIGNALSMITH_AUDIO_LINEAR_TREE2_RRR(Name, cmsis_prefix) \
	template<class A, class B> \
	void fill##Name(RealPointer<float> pointer, expression::Name<A, B> expr, size_t size) { \
		auto floats = cached.floatScope(); \
		auto *a = floats.real(expr.a, size); \
		auto *b = floats.real(expr.b, size); \
		cmsis_prefix##_f32(a, b, pointer, uint32_t(size)); \
	} \
	template<class A, class B> \
	ItemType<A, double, void> fill##Name(RealPointer<double> pointer, expression::Name<A, B> expr, size_t size) { \
		auto doubles = cached.doubleScope(); \
		auto *a = doubles.real(expr.a, size); \
		auto *b = doubles.real(expr.b, size); \
		cmsis_prefix##_f64(a, b, pointer, uint32_t(size)); \
	}
// C x C -> C where it's elementwise
#define SIGNALSMITH_AUDIO_LINEAR_TREE2_CCCe(Name, cmsis_prefix) \
	template<class A, class B> \
	void fill##Name(ComplexPointer<float> pointer, expression::Name<A, B> expr, size_t size) { \
		auto floats = cached.floatScope(); \
		auto *a = floats.complex(expr.a, size); \
		auto *b = floats.complex(expr.b, size); \
		cmsis_prefix##_f32(reinterpret_cast<const float *>(a), reinterpret_cast<const float *>(b), reinterpret_cast<float *>(pointer), size*2); \
	} \
	template<class A, class B> \
	void fill##Name(ComplexPointer<double> pointer, expression::Name<A, B> expr, size_t size) { \
		auto doubles = cached.doubleScope(); \
		auto *a = doubles.complex(expr.a, size); \
		auto *b = doubles.complex(expr.b, size); \
		cmsis_prefix##_f64(reinterpret_cast<const double *>(a), reinterpret_cast<const double *>(b), reinterpret_cast<double *>(pointer), size*2); \
	}\
	template<class A, class B> \
	void fill##Name(SplitPointer<float> pointer, expression::Name<A, B> expr, size_t size) { \
		auto floats = cached.floatScope(); \
		auto a = floats.split(expr.a, size); \
		auto b = floats.split(expr.b, size); \
		cmsis_prefix##_f32(a.real, b.real, pointer.real, size); \
		cmsis_prefix##_f32(a.imag, b.imag, pointer.imag, size); \
	} \
	template<class A, class B> \
	void fill##Name(SplitPointer<double> pointer, expression::Name<A, B> expr, size_t size) { \
		auto doubles = cached.doubleScope(); \
		auto a = doubles.split(expr.a, size); \
		auto b = doubles.split(expr.b, size); \
		cmsis_prefix##_f64(a.real, b.real, pointer.real, size); \
		cmsis_prefix##_f64(a.imag, b.imag, pointer.imag, size); \
	}
#define SIGNALSMITH_AUDIO_LINEAR_TREE2COMM_RRkR(Name, cmsis_prefix) \
	template<class A, class V> \
	void fill##Name(RealPointer<float> pointer, expression::Name<A, expression::ConstantExpr<V>> expr, size_t size) { \
		auto floats = cached.floatScope(); \
		auto *a = floats.real(expr.a, size, pointer); \
		float b = expr.b.value; \
		cmsis_prefix##_f32(a, b, pointer, uint32_t(size)); \
	} \
	template<class A, class V> \
	void fill##Name(RealPointer<double> pointer, expression::Name<A, expression::ConstantExpr<V>> expr, size_t size) { \
		auto doubles = cached.doubleScope(); \
		auto *a = doubles.real(expr.a, size, pointer); \
		double b = expr.b.value; \
		cmsis_prefix##_f64(a, b, pointer, uint32_t(size)); \
	} \
	template<class V, class B> \
	void fill##Name(RealPointer<float> pointer, expression::Name<expression::ConstantExpr<V>, B> expr, size_t size) { \
		auto floats = cached.floatScope(); \
		float a = expr.a.value; \
		auto *b = floats.real(expr.b, size, pointer); \
		cmsis_prefix##_f32(b, a, pointer, uint32_t(size)); \
	} \
	template<class V, class B> \
	void fill##Name(RealPointer<double> pointer, expression::Name<expression::ConstantExpr<V>, B> expr, size_t size) { \
		auto doubles = cached.doubleScope(); \
		double a = expr.a.value; \
		auto *b = doubles.real(expr.b, size, pointer); \
		cmsis_prefix##_f64(b, a, pointer, uint32_t(size)); \
	}
	SIGNALSMITH_AUDIO_LINEAR_OP2_R(Add)
	SIGNALSMITH_AUDIO_LINEAR_TREE2_RRR(Add, arm_add);
	SIGNALSMITH_AUDIO_LINEAR_TREE2COMM_RRkR(Add, arm_offset);
	SIGNALSMITH_AUDIO_LINEAR_OP2_C(Add)
	SIGNALSMITH_AUDIO_LINEAR_TREE2_CCCe(Add, arm_add);
	SIGNALSMITH_AUDIO_LINEAR_OP2_R(Sub)
	SIGNALSMITH_AUDIO_LINEAR_TREE2_RRR(Sub, arm_sub);
	SIGNALSMITH_AUDIO_LINEAR_OP2_C(Sub)
	SIGNALSMITH_AUDIO_LINEAR_TREE2_CCCe(Sub, arm_sub);
	// Flip a constant subtraction around into adding a negative constant, or negation + adding
	template<class A, class V>
	void fillSub(RealPointer<float> pointer, expression::Sub<A, expression::ConstantExpr<V>> expr, size_t size) {
		expression::ConstantExpr<V> negB{-expr.b.value};
		fillAdd(pointer, expression::makeAdd(expr.a, negB), size);
	}
	template<class A, class V>
	void fillSub(RealPointer<double> pointer, expression::Sub<A, expression::ConstantExpr<V>> expr, size_t size) {
		expression::ConstantExpr<V> negB{-expr.b.value};
		fillAdd(pointer, expression::makeAdd(expr.a, negB), size);
	}
	template<class A, class V>
	void fillSub(RealPointer<float> pointer, expression::Sub<expression::ConstantExpr<V>, A> expr, size_t size) {
		expression::ConstantExpr<V> negA{-expr.a.value};
		fillNeg(pointer, expression::makeNeg(expression::makeAdd(expr.b, negA)), size);
	}
	template<class A, class V>
	void fillSub(RealPointer<double> pointer, expression::Sub<expression::ConstantExpr<V>, A> expr, size_t size) {
		expression::ConstantExpr<V> negA{-expr.a.value};
		fillNeg(pointer, expression::makeNeg(expression::makeAdd(expr.b, negA)), size);
	}
	SIGNALSMITH_AUDIO_LINEAR_OP2_R(Mul)
	SIGNALSMITH_AUDIO_LINEAR_TREE2_RRR(Mul, arm_mult);
	SIGNALSMITH_AUDIO_LINEAR_TREE2COMM_RRkR(Mul, arm_scale);
	SIGNALSMITH_AUDIO_LINEAR_OP2_C(Mul)
	template<class A, class B>
	void fillMul(ComplexPointer<float> pointer, expression::Mul<A, B> expr, size_t size) {
		auto floats = cached.floatScope();
		auto *a = floats.complex(expr.a, size);
		auto *b = floats.complex(expr.b, size);
		arm_cmplx_mult_cmplx_f32(reinterpret_cast<const float *>(a), reinterpret_cast<const float *>(b), reinterpret_cast<float *>(pointer), size);
	}
	template<class A, class B>
	void fillMul(ComplexPointer<double> pointer, expression::Mul<A, B> expr, size_t size) {
		auto doubles = cached.doubleScope();
		auto *a = doubles.complex(expr.a, size);
		auto *b = doubles.complex(expr.b, size);
		arm_cmplx_mult_cmplx_f64(reinterpret_cast<const double *>(a), reinterpret_cast<const double *>(b), reinterpret_cast<double *>(pointer), size);
	}
	template<class A, class B>
	void fillMul(SplitPointer<float> pointer, expression::Mul<A, B> expr, size_t size) {
		auto floats = cached.floatScope();
		auto a = floats.split(expr.a, size);
		auto b = floats.split(expr.b, size);
		auto *tmp = floats(size);
		arm_mult_f32(a.real, b.real, pointer.real, uint32_t(size));
		arm_mult_f32(a.imag, b.imag, tmp, uint32_t(size));
		arm_sub_f32(pointer.real, tmp, pointer.real, uint32_t(size));
		arm_mult_f32(a.real, b.imag, tmp, uint32_t(size));
		arm_mult_f32(a.imag, b.real, pointer.imag, uint32_t(size));
		arm_add_f32(tmp, pointer.imag, pointer.imag, uint32_t(size));
	}
	template<class A, class B>
	void fillMul(SplitPointer<double> pointer, expression::Mul<A, B> expr, size_t size) {
		auto doubles = cached.doubleScope();
		auto a = doubles.split(expr.a, size);
		auto b = doubles.split(expr.b, size);
		auto *tmp = doubles(size);
		arm_mult_f64(a.real, b.real, pointer.real, uint32_t(size));
		arm_mult_f64(a.imag, b.imag, tmp, uint32_t(size));
		arm_sub_f64(pointer.real, tmp, pointer.real, uint32_t(size));
		arm_mult_f64(a.real, b.imag, tmp, uint32_t(size));
		arm_mult_f64(a.imag, b.real, pointer.imag, uint32_t(size));
		arm_add_f64(tmp, pointer.imag, pointer.imag, uint32_t(size));
	}
	SIGNALSMITH_AUDIO_LINEAR_OP2_R(Div)
	// Generic
	template<class P, class A, class B>
	void fillDiv(P p, expression::Div<A, B> expr, size_t size) {
		fillBasic(p, expr, size);
	}
	// Turn division by constant into multiplication by its reciprocal
	template<class A, class V>
	void fillDiv(RealPointer<float> pointer, expression::Div<A, expression::ConstantExpr<V>> expr, size_t size) {
		expression::ConstantExpr<V> recipB{1.0f/expr.b.value};
		fillMul(pointer, expression::makeMul(expr.a, recipB), size);
	}
	template<class A, class V>
	void fillDiv(RealPointer<double> pointer, expression::Sub<A, expression::ConstantExpr<V>> expr, size_t size) {
		expression::ConstantExpr<V> recipB{1.0/expr.b.value};
		fillMul(pointer, expression::makeMul(expr.a, recipB), size);
	}
//	// Otherwise, do a direct loop
//	template<class A, class B>
//	void fillDiv(RealPointer<float> pointer, expression::Div<A, B> expr, size_t size) {
//		auto floats = cached.floatScope();
//		auto *a = floats.real(expr.a, size);
//		auto *b = floats.real(expr.b, size);
//		for (size_t i = 0; i < size; ++i) {
//			pointer[i] = a[i]/b[i];
//		}
//	}
//	template<class A, class B>
//	void fillDiv(RealPointer<double> pointer, expression::Div<A, B> expr, size_t size) {
//		auto doubles = cached.doubleScope();
//		auto *a = doubles.real(expr.a, size);
//		auto *b = doubles.real(expr.b, size);
//		for (size_t i = 0; i < size; ++i) {
//			pointer[i] = a[i]/b[i];
//		}
//	}
#undef SIGNALSMITH_AUDIO_LINEAR_TREE2_RRR
#undef SIGNALSMITH_AUDIO_LINEAR_TREE2COMM_RRkR
#undef SIGNALSMITH_AUDIO_LINEAR_TREE2_CCCe
#undef SIGNALSMITH_AUDIO_LINEAR_OP2_R
#undef SIGNALSMITH_AUDIO_LINEAR_OP2_C

/*=========== move this down the file to gradually add overrides, and see which one makes it fail
//*/

protected:
	CachedResults<LinearImpl> cached;

	static void deinterleave(const float *interleaved, float *real, float *imag, size_t size) {
		for (size_t i = 0; i < size; ++i) {
			real[i] = interleaved[2*i];
			imag[i] = interleaved[2*i + 1];
		}
	}
};

}}; // namespace

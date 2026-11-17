#ifndef SIGNALSMITH_AUDIO_LINEAR_H
#define SIGNALSMITH_AUDIO_LINEAR_H

#if defined(__FAST_MATH__) && (__apple_build_version__ >= 16000000) && (__apple_build_version__ <= 16000099) && !defined(SIGNALSMITH_IGNORE_BROKEN_APPLECLANG)
#	error Apple Clang 16.0.0 generates incorrect SIMD for ARM. If you HAVE to use this version of Clang, turn off -ffast-math.
#endif

#if defined(_MSC_VER)
// MSVC can misreport is_trivially_copyable for CRTP bases; approximate intent.
#	define SIGNALSMITH_IS_TRIVIALLY_COPYABLE(T) (std::is_trivially_copy_constructible<T>::value && std::is_trivially_destructible<T>::value)
#else
#	define SIGNALSMITH_IS_TRIVIALLY_COPYABLE(T) std::is_trivially_copyable<T>::value
#endif

#ifndef M_PI
#	define M_PI 3.14159265358979323846
#endif

#include <cmath>
#include <cassert>
#include <complex>
#include <array>
#include <vector>
#include <type_traits>
#include <functional>
#include <random>

#ifdef SIGNALSMITH_LOG_WARNINGS
#	include <iostream>
#endif

namespace signalsmith { namespace linear {

template<typename V>
using ConstRealPointer = const V *;
template<typename V>
using RealPointer = V *;

template<typename V>
using ConstComplexPointer = const std::complex<V> *;
template<typename V>
using ComplexPointer = std::complex<V> *;

template<typename V>
struct ConstSplitPointer {
	ConstRealPointer<V> real, imag;
	ConstSplitPointer(ConstRealPointer<V> real, ConstRealPointer<V> imag) : real(real), imag(imag) {}

	// Array-like access for convenience
	const std::complex<V> operator[](std::ptrdiff_t i) const {
		return {real[i], imag[i]};
	}
};

template<typename V>
struct SplitPointer {
	using Complex = std::complex<V>;

	RealPointer<V> real, imag;
	SplitPointer(RealPointer<V> real, RealPointer<V> imag) : real(real), imag(imag) {}
	operator ConstSplitPointer<V>() const {
		return {real, imag};
	}

	// Array-like access for convenience
	const Complex operator[](std::ptrdiff_t i) const {
		return {real[i], imag[i]};
	}
	
	// Assignable (if not const) and converts to `std::complex<V>`
	struct Value : public Complex {
		Value(V &_real, V &_imag) : Complex(_real, _imag),  _real(_real), _imag(_imag) {}
	
		V real() const {
			return _real;
		}
		void real(V v) {
			_real = v;
			Complex::real(v);
		}
		V imag() const {
			return _imag;
		}
		void imag(V v) {
			_imag = v;
			Complex::imag(v);
		}

#define LINEAR_SPLIT_POINTER_ASSIGNMENT_OP(OP) \
		Value & operator OP(const std::complex<V> &other) { \
			std::complex<V>::operator OP(other); \
			_real = Complex::real(); \
			_imag = Complex::imag(); \
			return *this; \
		}
		LINEAR_SPLIT_POINTER_ASSIGNMENT_OP(=);
		LINEAR_SPLIT_POINTER_ASSIGNMENT_OP(+=);
		LINEAR_SPLIT_POINTER_ASSIGNMENT_OP(-=);
		LINEAR_SPLIT_POINTER_ASSIGNMENT_OP(*=);
		LINEAR_SPLIT_POINTER_ASSIGNMENT_OP(/=);
#undef LINEAR_SPLIT_POINTER_ASSIGNMENT_OP

	private:
		V &_real;
		V &_imag;
	};

	Value operator[](std::ptrdiff_t i) {
		return {real[i], imag[i]};
	}
};

template<bool=true>
struct LinearImpl;
using Linear = LinearImpl<true>;

// Everything we deal with is actually one of these
template<class BaseExpr>
struct Expression;
template<class BaseExpr>
struct WritableExpression;

#define EXPRESSION_NAME(Class, nameExpr) \
	static std::string name() {\
		return nameExpr; \
	}

// Expression templates, which always hold const pointers
namespace expression {
	// All base Exprs inherit from this, so we can SFINAE-test for them
	struct Base {};
	inline void mustBeExpr(const Base &) {}

	struct BaseUnary : public Base {};
	struct BaseBinary : public Base {};
	struct BasePointer : public Base {};
	struct BaseConstant : public Base {};

	template<typename V>
	struct ConstantExpr : public BaseConstant {
		EXPRESSION_NAME(Constant, "V");
		using Unwrapped = ConstantExpr;
		V value;

		static_assert(SIGNALSMITH_IS_TRIVIALLY_COPYABLE(V), "ConstantExpr<V> values must be trivially copyable");

		ConstantExpr(V value) : value(value) {}
		
		const V get(std::ptrdiff_t) const {
			return value;
		}
	};
	
	template<typename V>
	using Arithmetic = decltype(std::declval<V>() + std::declval<V>());

	// If the constant is one of our assignable complex proxies, use the basic one instead
	template<>
	struct ConstantExpr<typename SplitPointer<float>::Value> : public ConstantExpr<std::complex<float>> {
		using ConstantExpr<std::complex<float>>::ConstantExpr;
	};
	template<>
	struct ConstantExpr<typename SplitPointer<double>::Value> : public ConstantExpr<std::complex<double>> {
		using ConstantExpr<std::complex<double>>::ConstantExpr;
	};

	template<class V, typename=void>
	struct ExprTest {
		using ConstantE = ConstantExpr<Arithmetic<V>>;

		static_assert(SIGNALSMITH_IS_TRIVIALLY_COPYABLE(ConstantE), "ConstantExpr<V> must be trivially copyable");

		static ConstantE wrap(const V &v) {
			return {v};
		}
	};
	template<class Expr>
	struct ExprTest<Expr, decltype(mustBeExpr(std::declval<Expr>()))> {
		static Expr wrap(const Expr &expr) {
			return expr;
		}
	};
	// Constant class, only defined for non-Expr types
	template<class Expr>
	using Constant = typename ExprTest<Expr>::ConstantE;
	
	template<class Expr>
	auto ensureExpr(const Expr &expr) -> decltype(ExprTest<Expr>::wrap(expr)) {
		return ExprTest<Expr>::wrap(expr);
	};

	// Expressions that just read from a pointer
	template<typename V>
	struct ReadableReal : public BasePointer {
		EXPRESSION_NAME(ReadableReal, "V*");
		using Unwrapped = ReadableReal;
		ConstRealPointer<V> pointer;

		ReadableReal(ConstRealPointer<V> pointer) : pointer(pointer) {}
		
		V get(std::ptrdiff_t i) const {
			return pointer[i];
		}
	};
	template<typename V>
	struct ReadableComplex : public BasePointer {
		EXPRESSION_NAME(ReadableComplex, "VC*");
		using Unwrapped = ReadableComplex;
		ConstComplexPointer<V> pointer;

		ReadableComplex(ConstComplexPointer<V> pointer) : pointer(pointer) {}

		std::complex<V> get(std::ptrdiff_t i) const {
			return pointer[i];
		}
	};
	template<typename V>
	struct ReadableSplit : public BasePointer {
		EXPRESSION_NAME(ReadableSplit, "VS*");
		using Unwrapped = ReadableSplit;
		ConstSplitPointer<V> pointer;

		ReadableSplit(ConstSplitPointer<V> pointer) : pointer(pointer) {}

		std::complex<V> get(std::ptrdiff_t i) const {
			return {pointer.real[i], pointer.imag[i]};
		}
	};
	
	// Remove `Expression<>` or `WritableExpression<>` layers, and add const to pointers
	template<class E>
	typename E::Unwrapped unwrap(E e) {
		return e;
	}
	template<class E>
	typename E::Unwrapped unwrap(Expression<E> e) {
		return e;
	}
	template<class E>
	typename E::Unwrapped unwrap(WritableExpression<E> e) {
		return e;
	}
	template<class E>
	using Unwrap = decltype(unwrap(std::declval<E>()));

	template<class Expr>
	using ElementType = typename std::decay<decltype(std::declval<Expr>().get(0))>::type;
	
	template<class T>
	static constexpr bool isSplittable() {
		return std::is_same<T, std::complex<float>>::value || std::is_same<T, std::complex<double>>::value;
	}
}
	
// + - * / % ^ & | ~ ! = < > += -= *= /= %= ^= &= |= << >> >>= <<= == != <= >= <=>(since C++20) && || ++ -- , ->* -> ( ) [ ]

#define SIGNALSMITH_AUDIO_LINEAR_UNARY_PREFIX(Name, OP) \
namespace expression { \
	template<class A> \
	struct Name : public BaseUnary { \
		EXPRESSION_NAME(Name, (#Name "<") + A::name() + ">"); \
		using Unwrapped = Name<Unwrap<A>>; \
		A a; \
		Name(const A &a) : a(a) {} \
		auto get(std::ptrdiff_t i) const -> decltype(OP a.get(i)) const { \
			return OP a.get(i); \
		} \
		template<class T> \
		using ReplaceWith = Name<T>;\
	}; \
	template<class A> \
	Name<Unwrap<A>> make##Name(A a) { \
		return {a}; \
	} \
} \
template<class A> \
Expression<expression::Name<expression::Unwrap<A>>> operator OP(const Expression<A> &a) { \
	return {a}; \
}
SIGNALSMITH_AUDIO_LINEAR_UNARY_PREFIX(Neg, -)
// Two negatives cancel out
template<class A>
Expression<A> operator-(const Expression<expression::Neg<A>> &expr) { \
	return expr.a;
}
#undef SIGNALSMITH_AUDIO_LINEAR_UNARY_PREFIX

#define SIGNALSMITH_AUDIO_LINEAR_BINARY_INFIX(Name, OP) \
namespace expression { \
	template<class A, class B> \
	struct Name : public BaseBinary { \
		EXPRESSION_NAME(Name, (#Name "<") + A::name() + "," + B::name() + ">"); \
		using Unwrapped = Name<Unwrap<A>, Unwrap<B>>; \
		A a; \
		B b; \
		Name(const A &a, const B &b) : a(a), b(b) {} \
		auto get(std::ptrdiff_t i) const -> decltype(a.get(i) OP b.get(i)) const { \
			return a.get(i) OP b.get(i); \
		} \
		template<class T1, class T2> \
		using ReplaceWith = Name<T1, T2>;\
		template<class T> \
		using ReplaceWithL = Name<T, B>;\
		template<class T> \
		using ReplaceWithR = Name<A, T>;\
	}; \
	template<class A, class B> \
	Name<Unwrap<A>, Unwrap<B>> make##Name(A a, B b) { \
		return {a, b}; \
	} \
} \
template<class A, class B> \
const Expression<expression::Name<expression::Unwrap<A>, expression::Unwrap<B>>> operator OP(const Expression<A> &a, const Expression<B> &b) { \
	return {a, b}; \
} \
template<class A, class B> \
const Expression<expression::Name<expression::Unwrap<A>, expression::Constant<B>>> operator OP(const Expression<A> &a, const B &b) { \
	return {a, b}; \
} \
template<class A, class B> \
const Expression<expression::Name<expression::Constant<A>, expression::Unwrap<B>>> operator OP(const A &a, const Expression<B> &b) { \
	return {a, b}; \
}
SIGNALSMITH_AUDIO_LINEAR_BINARY_INFIX(Add, +)
SIGNALSMITH_AUDIO_LINEAR_BINARY_INFIX(Mul, *)
SIGNALSMITH_AUDIO_LINEAR_BINARY_INFIX(Sub, -)
SIGNALSMITH_AUDIO_LINEAR_BINARY_INFIX(Div, /)
#undef SIGNALSMITH_AUDIO_LINEAR_BINARY_INFIX

namespace expression {
#define SIGNALSMITH_AUDIO_LINEAR_FUNC1(Name, func) \
	template<class A> \
	struct Name; \
	template<class A> \
	Name<Unwrap<A>> make##Name(A a) { \
		return {a}; \
	} \
	template<class A> \
	struct Name : public BaseUnary { \
		EXPRESSION_NAME(Name, (#Name "<") + A::name() + ">"); \
		using Unwrapped = Name<Unwrap<A>>; \
		A a; \
		Name(const A &a) : a(a) {} \
		auto get(std::ptrdiff_t i) const -> decltype(func(a.get(i))) { \
			return func(a.get(i)); \
		} \
		template<class T> \
		using ReplaceWith = Name<T>;\
	};

	template<class A>
	A fastAbs(const A &a) {
		return std::abs(a);
	}
	template<class A>
	A fastAbs(const std::complex<A> &a) {
		return std::hypot(a.real(), a.imag());
	}
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Abs, fastAbs)
	
	template<class A>
	A fastNorm(const A &a) {
		return a*a;
	}
	template<class A>
	A fastNorm(const std::complex<A> &a) {
		A real = a.real(), imag = a.imag();
		return real*real + imag*imag;
	}
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Norm, fastNorm)

	// Single-argument functions
	// Abs, Norm, Exp, Exp2, Log, Log2, Log10, Sqrt, Cbrt, Ceil, Floor, Trunc, Round, Conj, Real, Imag, Arg, Proj, Sin, Cos, Tan, Asin, Acos, Atan, Sinh, Cosh, Tanh, Asinh, Acosh, Atanh, Erf, Erfc, Tgamma, Lgamma

	// .abs and .norm are handled above
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Exp, std::exp)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Exp2, std::exp2)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Log, std::log)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Log2, std::log2)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Log10, std::log10)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Sqrt, std::sqrt)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Cbrt, std::cbrt)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Ceil, std::ceil)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Floor, std::floor)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Trunc, std::trunc)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Round, std::round)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Conj, std::conj)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Real, std::real)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Imag, std::imag)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Arg, std::arg)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Proj, std::proj)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Sin, std::sin)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Cos, std::cos)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Tan, std::tan)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Asin, std::asin)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Acos, std::acos)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Atan, std::atan)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Sinh, std::sinh)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Cosh, std::cosh)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Tanh, std::tanh)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Asinh, std::asinh)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Acosh, std::acosh)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Atanh, std::atanh)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Erf, std::erf)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Erfc, std::erfc)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Tgamma, std::tgamma)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Lgamma, std::lgamma)
#undef SIGNALSMITH_AUDIO_LINEAR_FUNC1

#define SIGNALSMITH_AUDIO_LINEAR_FUNC2(Name, func) \
	template<typename VA, typename VB> \
	auto common##Name(VA a, VB b) -> decltype(func((decltype(a + b))a, (decltype(a + b))b)) { \
		return func((decltype(a + b))a, (decltype(a + b))b); \
	} \
	template<class A, class B> \
	struct Name : public BaseBinary { \
		EXPRESSION_NAME(Name, (#Name "<") + A::name() + "," + B::name() + ">"); \
		using Unwrapped = Name<Unwrap<A>, Unwrap<B>>; \
		A a; \
		B b; \
		Name(const A &a, const B &b) : a(a), b(b) {} \
		auto get(std::ptrdiff_t i) const -> decltype(common##Name(a.get(i), b.get(i))) const { \
			return common##Name(a.get(i), b.get(i)); \
		} \
		template<class T1, class T2> \
		using ReplaceWith = Name<T1, T2>;\
		template<class T> \
		using ReplaceWithL = Name<T, B>;\
		template<class T> \
		using ReplaceWithR = Name<A, T>;\
	}; \
	template<class A, class B> \
	Name<Unwrap<A>, Unwrap<B>> make##Name(A a, B b) { \
		return {a, b}; \
	}
	// Min, Max, Dim, Pow, Atan2, Hypot, Copysign, Polar
	SIGNALSMITH_AUDIO_LINEAR_FUNC2(Max, std::fmax)
	SIGNALSMITH_AUDIO_LINEAR_FUNC2(Min, std::fmin)
	SIGNALSMITH_AUDIO_LINEAR_FUNC2(Dim, std::fdim)
	SIGNALSMITH_AUDIO_LINEAR_FUNC2(Pow, std::pow)
	SIGNALSMITH_AUDIO_LINEAR_FUNC2(Atan2, std::atan2)
	SIGNALSMITH_AUDIO_LINEAR_FUNC2(Hypot, std::hypot)
	SIGNALSMITH_AUDIO_LINEAR_FUNC2(Copysign, std::copysign)
	SIGNALSMITH_AUDIO_LINEAR_FUNC2(Polar, std::polar)
#undef SIGNALSMITH_AUDIO_LINEAR_FUNC2

} // expression::

template<class BaseExpr>
struct Expression : public BaseExpr {
	template<class ...Args>
	Expression(Args &&...args) : BaseExpr(std::forward<Args>(args)...) {
		static_assert(SIGNALSMITH_IS_TRIVIALLY_COPYABLE(BaseExpr), "BaseExpr must be trivially copyable");
		static_assert(SIGNALSMITH_IS_TRIVIALLY_COPYABLE(Expression), "Expression<> must be trivially copyable");
	}

	auto operator[](std::ptrdiff_t i) -> decltype(BaseExpr::get(i)) const {
		return BaseExpr::get(i);
	}

#define SIGNALSMITH_AUDIO_LINEAR_FUNC1(ExprName, methodName) \
	const Expression<expression::ExprName<expression::Unwrap<BaseExpr>>> methodName() const { \
		return {*this}; \
	}
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Abs, abs)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Norm, norm)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Exp, exp)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Exp2, exp2)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Log, log)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Log2, log2)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Log10, log10)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Sqrt, sqrt)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Cbrt, cbrt)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Ceil, ceil)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Floor, floor)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Trunc, trunc)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Round, round)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Conj, conj)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Real, real)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Imag, imag)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Arg, arg)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Proj, proj)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Sin, sin)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Cos, cos)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Tan, tan)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Asin, asin)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Acos, acos)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Atan, atan)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Sinh, sinh)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Cosh, cosh)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Tanh, tanh)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Asinh, asinh)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Acosh, acosh)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Atanh, atanh)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Erf, erf)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Erfc, erfc)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Tgamma, tgamma)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Lgamma, lgamma)
#undef SIGNALSMITH_AUDIO_LINEAR_FUNC1
};
template<class BaseExpr>
struct WritableExpression : public Expression<BaseExpr> {
	using Expression<BaseExpr>::Expression;

	WritableExpression(const WritableExpression &other) = default;

	template<class Expr>
	WritableExpression & operator=(const Expr &expr) {
		this->linear.fill(this->pointer, expression::ensureExpr(expr), this->size);
		return *this;
	}

	WritableExpression & operator=(const WritableExpression &expr) {
		this->linear.fill(this->pointer, expr, this->size);
		return *this;
	}
#define SIGNALSMITH_AUDIO_ASSIGNMENT_OP(assignOp, binaryOp) \
	template<class E>\
	WritableExpression & operator assignOp(const E &expr) { \
		return *this = (*this) binaryOp expr; \
	}
	SIGNALSMITH_AUDIO_ASSIGNMENT_OP(+=, +)
	SIGNALSMITH_AUDIO_ASSIGNMENT_OP(-=, -)
	SIGNALSMITH_AUDIO_ASSIGNMENT_OP(*=, *)
	SIGNALSMITH_AUDIO_ASSIGNMENT_OP(/=, /)
#undef SIGNALSMITH_AUDIO_ASSIGNMENT_OP

	// Use the pointer's `operator[]` instead of the expression
	auto operator[](std::ptrdiff_t i) -> decltype(this->pointer[i]) {
		return this->pointer[i];
	}

	auto operator[](std::ptrdiff_t i) const -> decltype(this->pointer[i]) {
		return this->pointer[i];
	}
};

/// Helper class for temporary storage
template<typename V, bool allowAllocation, size_t alignBytes>
struct Temporary {

	Temporary() {}
	Temporary(const Temporary &other) = delete;
	Temporary & operator=(const Temporary &other) = delete;
	~Temporary() {
		if (buffer) delete[] buffer;
	}
	
	void reserve(size_t size) {
		if (buffer) delete[] buffer;
		buffer = new V[size];
		alignedBuffer = nextAligned(buffer);
		if (alignedBuffer != buffer) {
			delete[] buffer;
			buffer = new V[size + extraAlignmentItems];
			alignedBuffer = nextAligned(buffer);
		}
		start = alignedBuffer;
		end = alignedBuffer + size;
	}

	void clear() {
		start = alignedBuffer;
		fallbacks.resize(0);
		fallbacks.shrink_to_fit();
	}

	// You should have one of these every time you're using the temporary storage
	struct Scoped {
		Scoped(Temporary &temporary) : temporary(temporary), restoreStart(temporary.start), fallbackSize(temporary.fallbacks.size()) {}
		
		~Scoped() {
			temporary.start = restoreStart;
			temporary.fallbacks.resize(fallbackSize);
		}
		
		V * operator()(size_t size) {
			return temporary.getChunk(size);
		}

	private:
		Temporary &temporary;
		V *restoreStart;
		size_t fallbackSize;
	};

private:
	static constexpr size_t extraAlignmentItems = alignBytes/sizeof(V);
	static V * nextAligned(V *ptr) {
		return (V*)((size_t(ptr) + (alignBytes - 1))&~(alignBytes - 1));
	}

	size_t depth = 0;
	V *start = nullptr, *end = nullptr;
	V *buffer = nullptr, *alignedBuffer = nullptr;

	std::vector<std::vector<V>> fallbacks;

	/// Valid until the next call to .clear() or .reserve()
	V * getChunk(size_t size) {
		V *result = start;
		V *newStart = start + size;
		if (newStart > end) {
#ifdef SIGNALSMITH_LOG_WARNINGS
		std::cerr << "Signalsmith Linear: insufficient temporary storage (" << (newStart > buffer) << "\n";
#endif
			if (!allowAllocation) return nullptr;
			// OK, actually we ran out of temporary space, so allocate
			fallbacks.emplace_back(size + extraAlignmentItems);
			result = nextAligned(fallbacks.back().data());
			// but we're not happy about it. >:(
		}
		start = nextAligned(newStart);
		return result;
	}
};

template<class Linear, bool allowAllocation=true, size_t alignBytes=64>
struct CachedResults {
	using TemporaryFloats = Temporary<float, allowAllocation, alignBytes>;
	using TemporaryDoubles = Temporary<double, allowAllocation, alignBytes>;

	template<typename V>
	using WritableReal = typename Linear::template WritableReal<V>;
	template<typename V>
	using WritableComplex = typename Linear::template WritableComplex<V>;
	template<typename V>
	using WritableSplit = typename Linear::template WritableSplit<V>;
	
	CachedResults(Linear &linear) : linear(linear) {}
	
	struct ScopedFloat : public TemporaryFloats::Scoped {
		ScopedFloat(Linear &linear, TemporaryFloats &temporary) : TemporaryFloats::Scoped(temporary), linear(linear) {}
		
		template<class Expr>
		ConstRealPointer<float> real(Expr expr, size_t size) {
			auto chunk = (*this)(size);
			linear.fill(chunk, expr, size);
			return chunk;
		}
		ConstRealPointer<float> real(expression::ReadableReal<float> expr, size_t) {
			return expr.pointer;
		}
		ConstRealPointer<float> real(WritableReal<float> expr, size_t) {
			return expr.pointer;
		}
		template<class Expr>
		ConstRealPointer<float> real(Expr expr, size_t size, RealPointer<float> canUse) {
			linear.fill(canUse, expr, size);
			return canUse;
		}
		ConstRealPointer<float> real(expression::ReadableReal<float> expr, size_t, RealPointer<float>) {
			return expr.pointer;
		}
		ConstRealPointer<float> real(WritableReal<float> expr, size_t, RealPointer<float>) {
			return expr.pointer;
		}
		template<class Expr>
		ConstComplexPointer<float> complex(Expr expr, size_t size) {
			auto chunk = (*this)(size*2);
			linear.fill((ComplexPointer<float>)chunk, expr, size);
			return reinterpret_cast<const std::complex<float> *>(chunk);
		}
		ConstComplexPointer<float> complex(expression::ReadableComplex<float> expr, size_t) {
			return expr.pointer;
		}
		ConstComplexPointer<float> complex(WritableComplex<float> expr, size_t) {
			return expr.pointer;
		}
		template<class Expr>
		ConstComplexPointer<float> complex(Expr expr, size_t size, ComplexPointer<float> canUse) {
			linear.fill(canUse, expr, size);
			return canUse;
		}
		ConstComplexPointer<float> complex(expression::ReadableComplex<float> expr, size_t, ComplexPointer<float>) {
			return expr.pointer;
		}
		ConstComplexPointer<float> complex(WritableComplex<float> expr, size_t, ComplexPointer<float>) {
			return expr.pointer;
		}
		template<class Expr>
		ConstSplitPointer<float> split(Expr expr, size_t size) {
			SplitPointer<float> chunk{(*this)(size), (*this)(size)};
			linear.fill(chunk, expr, size);
			return chunk;
		}
		ConstSplitPointer<float> split(expression::ReadableSplit<float> expr, size_t) {
			return expr.pointer;
		}
		ConstSplitPointer<float> split(WritableSplit<float> expr, size_t) {
			return expr.pointer;
		}
		template<class Expr>
		ConstSplitPointer<float> split(Expr expr, size_t size, SplitPointer<float> canUse) {
			linear.fill(canUse, expr, size);
			return canUse;
		}
		ConstSplitPointer<float> split(expression::ReadableSplit<float> expr, size_t, SplitPointer<float>) {
			return expr.pointer;
		}
		ConstSplitPointer<float> split(WritableSplit<float> expr, size_t, SplitPointer<float>) {
			return expr.pointer;
		}
	private:
		Linear &linear;
	};
	ScopedFloat floatScope() {
		return {linear, floats};
	}
	void reserveFloats(size_t size) {
		floats.reserve(size);
	}
	template<class Fn>
	void reserveFloats(size_t size, Fn &&allocationWarning) {
		floats.reserve(size);
		floats.allocationWarning = allocationWarning;
	}

	struct ScopedDouble : public TemporaryDoubles::Scoped {
		ScopedDouble(Linear &linear, TemporaryDoubles &temporary) : TemporaryDoubles::Scoped(temporary), linear(linear) {}
		
		template<class Expr>
		ConstRealPointer<double> real(Expr expr, size_t size) {
			auto chunk = (*this)(size);
			linear.fill(chunk, expr, size);
			return chunk;
		}
		ConstRealPointer<double> real(expression::ReadableReal<double> expr, size_t) {
			return expr.pointer;
		}
		ConstRealPointer<double> real(WritableReal<double> expr, size_t) {
			return expr.pointer;
		}
		template<class Expr>
		ConstRealPointer<double> real(Expr expr, size_t size, RealPointer<double> canUse) {
			linear.fill(canUse, expr, size);
			return canUse;
		}
		ConstRealPointer<double> real(expression::ReadableReal<double> expr, size_t, RealPointer<double>) {
			return expr.pointer;
		}
		ConstRealPointer<double> real(WritableReal<double> expr, size_t, RealPointer<double>) {
			return expr.pointer;
		}
		template<class Expr>
		ConstComplexPointer<double> complex(Expr expr, size_t size) {
			auto chunk = (*this)(size*2);
			linear.fill((ComplexPointer<double>)chunk, expr, size);
			return reinterpret_cast<const std::complex<double> *>(chunk);
		}
		ConstComplexPointer<double> complex(expression::ReadableComplex<double> expr, size_t) {
			return expr.pointer;
		}
		ConstComplexPointer<double> complex(WritableComplex<double> expr, size_t) {
			return expr.pointer;
		}
		template<class Expr>
		ConstComplexPointer<double> complex(Expr expr, size_t size, ComplexPointer<double> canUse) {
			linear.fill(canUse, expr, size);
			return canUse;
		}
		ConstComplexPointer<double> complex(expression::ReadableComplex<double> expr, size_t, ComplexPointer<double>) {
			return expr.pointer;
		}
		ConstComplexPointer<double> complex(WritableComplex<double> expr, size_t, ComplexPointer<double>) {
			return expr.pointer;
		}
		template<class Expr>
		ConstSplitPointer<double> split(Expr expr, size_t size) {
			SplitPointer<double> chunk{(*this)(size), (*this)(size)};
			linear.fill(chunk, expr, size);
			return chunk;
		}
		ConstSplitPointer<double> split(expression::ReadableSplit<double> expr, size_t) {
			return expr.pointer;
		}
		ConstSplitPointer<double> split(WritableSplit<double> expr, size_t) {
			return expr.pointer;
		}
		template<class Expr>
		ConstSplitPointer<double> split(Expr expr, size_t size, SplitPointer<double> canUse) {
			linear.fill(canUse, expr, size);
			return canUse;
		}
		ConstSplitPointer<double> split(expression::ReadableSplit<double> expr, size_t, SplitPointer<double>) {
			return expr.pointer;
		}
		ConstSplitPointer<double> split(WritableSplit<double> expr, size_t, SplitPointer<double>) {
			return expr.pointer;
		}
	private:
		Linear &linear;
	};
	ScopedDouble doubleScope() {
		return {linear, doubles};
	}
	void reserveDoubles(size_t size) {
		doubles.reserve(size);
	}
	template<class Fn>
	void reserveDoubles(size_t size, Fn &&allocationWarning) {
		doubles.reserve(size);
		doubles.allocationWarning = allocationWarning;
	}
	
private:
	TemporaryFloats floats;
	TemporaryDoubles doubles;
	Linear &linear;
};

template<bool useLinear=true>
struct LinearImplBase {
	using Linear = LinearImpl<useLinear>;

	template<class V>
	void reserve(size_t) {}

	template<typename V>
	struct WritableReal {
		EXPRESSION_NAME(WritableReal, "rw V*");
		Linear &linear;
		RealPointer<V> pointer;
		size_t size;
		WritableReal(Linear &linear, RealPointer<V> pointer, size_t size) : linear(linear), pointer(pointer), size(size) {
			static_assert(SIGNALSMITH_IS_TRIVIALLY_COPYABLE(WritableReal), "must be trivially copyable");
		}
		
		using Unwrapped = expression::ReadableReal<V>;
		operator Unwrapped() const {
			return {pointer};
		}
		
		V get(std::ptrdiff_t i) const {
			return pointer[i];
		}

		V & operator[](std::ptrdiff_t i) {
			return pointer[i];
		}
		const V & operator[](std::ptrdiff_t i) const {
			return pointer[i];
		}
	};
	template<typename V>
	struct WritableComplex {
		EXPRESSION_NAME(WritableComplex, "rw VC*");
		Linear &linear;
		ComplexPointer<V> pointer;
		size_t size;
		WritableComplex(Linear &linear, ComplexPointer<V> pointer, size_t size) : linear(linear), pointer(pointer), size(size) {}

		using Unwrapped = expression::ReadableComplex<V>;
		operator Unwrapped() const {
			return {pointer};
		}

		std::complex<V> get(std::ptrdiff_t i) const {
			return pointer[i];
		}

		std::complex<V> & operator[](std::ptrdiff_t i) {
			return pointer[i];
		}
		const std::complex<V> & operator[](std::ptrdiff_t i) const {
			return pointer[i];
		}
	};
	template<typename V>
	struct WritableSplit {
		EXPRESSION_NAME(WritableSplit, "rw VS*");
		Linear &linear;
		SplitPointer<V> pointer;
		size_t size;
		WritableSplit(Linear &linear, SplitPointer<V> pointer, size_t size) : linear(linear), pointer(pointer), size(size) {}

		using Unwrapped = expression::ReadableSplit<V>;
		operator Unwrapped() const {
			return ConstSplitPointer<V>{pointer.real, pointer.imag};
		}

		std::complex<V> get(std::ptrdiff_t i) const {
			return {pointer.real[i], pointer.imag[i]};
		}
		auto operator[](std::ptrdiff_t i) -> decltype(pointer[i]) {
			return pointer[i];
		}
		auto operator[](std::ptrdiff_t i) const -> decltype(pointer[i]) {
			return pointer[i];
		}
	};

	// Arithmetic values get turned into constants
	template<typename V, typename=typename std::enable_if<std::is_arithmetic<V>::value, V>::type>
	Expression<expression::Constant<V>> wrap(V v) {
		return {v};
	}

	// Pass `Expression`s through
	template<class V>
	Expression<V> wrap(Expression<V> expr) {
		return expr;
	}

	// Wrap a pointer as an expression
	template<typename V>
	Expression<expression::ReadableReal<V>> wrap(ConstRealPointer<V> pointer) {
		return {pointer};
	}
	template<typename V>
	Expression<expression::ReadableComplex<V>> wrap(ConstComplexPointer<V> pointer) {
		return {pointer};
	}
	template<typename V>
	Expression<expression::ReadableSplit<V>> wrap(ConstSplitPointer<V> pointer) {
		return {pointer};
	}
	template<typename V>
	Expression<expression::ReadableSplit<V>> wrap(ConstRealPointer<V> real, ConstRealPointer<V> imag) {
		return {ConstSplitPointer<V>{real, imag}};
	}

	// When a length is supplied, make it writable
	template<typename V>
	WritableExpression<WritableReal<V>> wrap(RealPointer<V> pointer, size_t size) {
		return {self(), pointer, size};
	}
	template<typename V>
	WritableExpression<WritableComplex<V>> wrap(ComplexPointer<V> pointer, size_t size) {
		return {self(), pointer, size};
	}
	template<typename V>
	WritableExpression<WritableSplit<V>> wrap(SplitPointer<V> pointer, size_t size) {
		return {self(), pointer, size};
	}
	template<typename V>
	WritableExpression<WritableSplit<V>> wrap(RealPointer<V> real, RealPointer<V> imag, size_t size) {
		return {self(), SplitPointer<V>{real, imag}, size};
	}

	// Wrap vectors (writable)
	template<typename V, class A>
	WritableExpression<WritableReal<V>> wrap(std::vector<V, A> &vector) {
		return {self(), vector.data(), vector.size()};
	}
	template<typename V, class A>
	WritableExpression<WritableComplex<V>> wrap(std::vector<std::complex<V>, A> &vector) {
		return {self(), vector.data(), vector.size()};
	}
	template<typename V, class A1, class A2>
	WritableExpression<WritableSplit<V>> wrap(std::vector<V, A1> &real, std::vector<V, A2> &imag) {
		SplitPointer<V> pointer{real.data(), imag.data()};
		size_t size = std::min<size_t>(real.size(), imag.size());
		return {self(), pointer, size};
	}
	// and read-only
	template<typename V, class A>
	Expression<expression::ReadableReal<V>> wrap(const std::vector<V, A> &vector) {
		return {vector.data()};
	}
	template<typename V, class A>
	Expression<expression::ReadableComplex<V>> wrap(const std::vector<std::complex<V>, A> &vector) {
		return {vector.data()};
	}
	template<typename V, class A1, class A2>
	Expression<expression::ReadableSplit<V>> wrap(const std::vector<V, A1> &real, const std::vector<V, A2> &imag) {
		ConstSplitPointer<V> pointer{real.data(), imag.data()};
		return {pointer};
	}

	template<class ...Args>
	auto operator()(Args &&...args) -> decltype(wrap(std::forward<Args>(args)...)) {
		return wrap(std::forward<Args>(args)...);
	}

	template<class Pointer, class Expr>
	void fill(Pointer pointer, Expr expr, size_t size) {
		for (size_t i = 0; i < size; ++i) {
			pointer[i] = expr.get(i);
		}
	}
	template<class V, class Expr>
	void fill(SplitPointer<V> pointer, Expr expr, size_t size) {
		for (size_t i = 0; i < size; ++i) {
			std::complex<V> v = expr.get(i);
			pointer.real[i] = v.real();
			pointer.imag[i] = v.imag();
		}
	}

	// Remove the Expression<...> layer, so the simplification template-matching works
	template<class Pointer, class Expr>
	void fill(Pointer pointer, Expression<Expr> expr, size_t size) {
		return self().fill(pointer, (Expr &)expr, size);
	}
	// Separate otherwise it's ambiguous between the two previous ones
	template<class V, class Expr>
	void fill(SplitPointer<V> pointer, Expression<Expr> expr, size_t size) {
		return self().fill(pointer, (Expr &)expr, size);
	}

#define SIGNALSMITH_AUDIO_LINEAR_FUNC1(ExprName, methodName) \
	template<class A> \
	auto methodName(A &&a) -> Expression<decltype(expression::make##ExprName(wrap(std::forward<A>(a))))> { \
		return expression::make##ExprName(wrap(std::forward<A>(a))); \
	}
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Abs, abs)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Norm, norm)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Exp, exp)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Exp2, exp2)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Log, log)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Log2, log2)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Log10, log10)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Sqrt, sqrt)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Cbrt, cbrt)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Ceil, ceil)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Floor, floor)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Trunc, trunc)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Round, round)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Conj, conj)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Real, real)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Imag, imag)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Arg, arg)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Proj, proj)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Sin, sin)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Cos, cos)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Tan, tan)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Asin, asin)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Acos, acos)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Atan, atan)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Sinh, sinh)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Cosh, cosh)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Tanh, tanh)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Asinh, asinh)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Acosh, acosh)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Atanh, atanh)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Erf, erf)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Erfc, erfc)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Tgamma, tgamma)
	SIGNALSMITH_AUDIO_LINEAR_FUNC1(Lgamma, lgamma)
#undef SIGNALSMITH_AUDIO_LINEAR_FUNC1

#define SIGNALSMITH_AUDIO_LINEAR_FUNC2(ExprName, methodName) \
	template<class A, class B> \
	auto methodName(A a, B b) -> Expression<decltype(expression::make##ExprName(wrap(a), wrap(b)))> { \
		return expression::make##ExprName(wrap(a), wrap(b)); \
	}
	SIGNALSMITH_AUDIO_LINEAR_FUNC2(Max, max)
	SIGNALSMITH_AUDIO_LINEAR_FUNC2(Min, min)
	SIGNALSMITH_AUDIO_LINEAR_FUNC2(Dim, dim)
	SIGNALSMITH_AUDIO_LINEAR_FUNC2(Pow, pow)
	SIGNALSMITH_AUDIO_LINEAR_FUNC2(Atan2, atan2)
	SIGNALSMITH_AUDIO_LINEAR_FUNC2(Hypot, hypot)
	SIGNALSMITH_AUDIO_LINEAR_FUNC2(Copysign, copysign)
	SIGNALSMITH_AUDIO_LINEAR_FUNC2(Polar, polar)
#undef SIGNALSMITH_AUDIO_LINEAR_FUNC2

protected:
	LinearImplBase(Linear *linearThis) {
		assert((LinearImplBase *)linearThis == this);
	}

	Linear & self() {
		return *(Linear *)this;
	}
};
#undef EXPRESSION_NAME

/* SFINAE template for checking that an expression naturally returns a particular item type

e.g.
	template<class A>
	ItemType<A, float, void> fill(...)
*/
template<class InputExpr, typename Item, class OutputExpr>
using ItemType = typename std::enable_if<
	std::is_same<expression::ElementType<InputExpr>, Item>::value,
	OutputExpr
>::type;

// Fallback implementation - this should be specialised (with useLinear=true) with faster methods where available
template<bool useLinear>
struct LinearImpl : public LinearImplBase<useLinear> {
	LinearImpl() : LinearImplBase<useLinear>(this), cached(*this) {}

	using LinearImplBase<useLinear>::fill;
	
	// Override .fill() for specific pointer/expressions which you can do quickly.  Calling `cached.realFloat()` etc. will call back to .fill()
	template<class Expr>
	void fill(RealPointer<float> pointer, expression::Sqrt<expression::Norm<Expr>> expr, size_t size) {
		auto floats = cached.floatScope(); // temporary storage - usually not actually needed for specialisations
		
		auto normExpr = expr.a;
		auto array = floats.real(normExpr, size); // fill the temporary storage with real values, return a pointer Expr
		
		// The idea is to use an existing fast function, but this is an example
		for (size_t i = 0; i < size; ++i) {
			pointer[i] = std::sqrt(array[i]);
		}
	}
	
	template<typename V>
	void reserve(size_t size) {
		// Assume it's either float or double
		if (sizeof(V) == sizeof(double)) {
			cached.reserveDoubles(size);
		} else if (sizeof(V) == sizeof(float)) {
			cached.reserveFloats(size);
		}
	}
private:
	CachedResults<LinearImpl, true, 16> cached;
};

}}; // namespace

#if defined(SIGNALSMITH_USE_ACCELERATE)
#	include "./platform/linear-accelerate.h"
#elif defined(SIGNALSMITH_USE_XSIMD)
#	include "./platform/linear-xsimd.h"
#elif defined(SIGNALSMITH_USE_CMSISDSP)
#	include "./platform/linear-cmsisdsp.h"
//#elif defined(SIGNALSMITH_USE_IPP)
//#	include "./platform/linear-ipp.h"
#endif

#undef SIGNALSMITH_IS_TRIVIALLY_COPYABLE

#endif // include guard

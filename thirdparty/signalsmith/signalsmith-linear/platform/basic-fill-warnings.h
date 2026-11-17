#pragma once

namespace signalsmith { namespace linear {

#ifdef SIGNALSMITH_LOG_WARNINGS
#	include <iostream>
template<class T>
static std::string typeName() {
	return "?";
}
template<>
std::string typeName<float>() {
	return "float";
}
template<>
std::string typeName<double>() {
	return "double";
}
template<>
std::string typeName<std::complex<float>>() {
	return "complex<float>";
}
template<>
std::string typeName<std::complex<double>>() {
	return "complex<double>";
}
template<>
std::string typeName<const std::complex<float>>() {
	return "const complex<float>";
}
template<>
std::string typeName<const std::complex<double>>() {
	return "const complex<double>";
}
static size_t _basicFillWarningCounter = 1;
static void basicFillWarningReset() {
	// All warnings will print again
	++_basicFillWarningCounter;
}
template<class Expr>
static void basicFillWarning() {
	static size_t counter = 0;
	if (counter != _basicFillWarningCounter) {
		counter = _basicFillWarningCounter;
		std::cerr << "Signalsmith Linear: used basic .fill() for " << Expr::name() << " -> " << typeName<decltype(std::declval<Expr>().get(0))>() << "\n";
	}
}
static void basicFillWarning(const char *method) {
	static size_t counter = 0;
	if (counter != _basicFillWarningCounter) {
		counter = _basicFillWarningCounter;
		std::cerr << "Signalsmith Linear: used basic ." << method << "()\n";
	}
}
#else
static void basicFillWarningReset() {}
template<class Expr>
static void basicFillWarning() {}
#endif

}}; // namespace

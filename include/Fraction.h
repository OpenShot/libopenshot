#ifndef OPENSHOT_FRACTION_H
#define OPENSHOT_FRACTION_H

/**
 * \file
 * \brief Header file for Fraction class
 * \author Copyright (c) 2011 Jonathan Thomas
 */

namespace openshot {

	/**
	 * \brief The class represents a fraction
	 *
	 * Fractions are often used in video editing to represent ratios and rates, for example:
	 * pixel ratios, frames per second, timebase, and other common ratios.  Fractions are preferred
	 * over decimals due to their increased precision.
	 */
	class Fraction {
	public:
		int num; ///<Numerator for the fraction
		int den; ///<Denominator for the fraction

		/// Default Constructor
		Fraction();
		/// Constructor with numerator and denominator
		Fraction(int num, int den);

		/// Calculate the greatest common denominator
		int GreatestCommonDenominator();

		/// Reduce this fraction (i.e. 640/480 = 4/3)
		void Reduce();

		/// Return this fraction as a float (i.e. 1/2 = 0.5)
		float ToFloat();

		/// Return this fraction as a double (i.e. 1/2 = 0.5)
		double ToDouble();

		/// Return the reciprocal as a Fraction
		Fraction Reciprocal();
	};


}

#endif

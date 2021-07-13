/**
 * @file
 * @brief Header file for Fraction class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

/* LICENSE
 *
 * Copyright (c) 2008-2019 OpenShot Studios, LLC
 * <http://www.openshotstudios.com/>. This file is part of
 * OpenShot Library (libopenshot), an open-source project dedicated to
 * delivering high quality video editing and animation solutions to the
 * world. For more information visit <http://www.openshot.org/>.
 *
 * OpenShot Library (libopenshot) is free software: you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * OpenShot Library (libopenshot) is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with OpenShot Library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef OPENSHOT_FRACTION_H
#define OPENSHOT_FRACTION_H

#include <string>    // for std::string
#include <utility>   // for std::pair
#include <map>       // for std::map
#include <vector>    // for std::vector

namespace openshot {

	/**
	 * @brief This class represents a fraction
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

		/// Constructor that accepts a (num, den) pair
		Fraction(std::pair<int, int> pair);

		/// Constructor that takes a vector of length 2 (containing {num, den})
		Fraction(std::vector<int> vector);

		/// Constructor that takes a key-value mapping (keys: 'num'. 'den')
		Fraction(std::map<std::string, int> mapping);

		/// Calculate the greatest common denominator
		int GreatestCommonDenominator();

		/// Reduce this fraction (i.e. 640/480 = 4/3)
		void Reduce();

		/// Return this fraction as a float (i.e. 1/2 = 0.5)
		float ToFloat();

		/// Return this fraction as a double (i.e. 1/2 = 0.5)
		double ToDouble() const;

		/// Return a rounded integer of the fraction (for example 30000/1001 returns 30)
		int ToInt();

		/// Return the reciprocal as a Fraction
		Fraction Reciprocal() const;
	};

	// Stream output operator for openshot::Fraction
	template<class charT, class traits>
	std::basic_ostream<charT, traits>&
	operator<<(std::basic_ostream<charT, traits>& o, const openshot::Fraction& frac) {
    	std::basic_ostringstream<charT, traits> s;
    	s.flags(o.flags());
    	s.imbue(o.getloc());
    	s.precision(o.precision());
    	s << "Fraction(" << frac.num << ", " << frac.den << ")";
    	return o << s.str();
	};
}
#endif

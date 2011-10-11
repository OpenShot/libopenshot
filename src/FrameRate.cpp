/**
 * \file
 * \brief Source code for the FrameRate class
 * \author Copyright (c) 2011 Jonathan Thomas
 */

#include "../include/FrameRate.h"

using namespace openshot;

Framerate::Framerate(int numerator, int denominator) {
	m_numerator = numerator;
	m_denominator = denominator;
}

Framerate::Framerate() {
	m_numerator = 24;
	m_denominator = 1;
}

// Return a rounded integer of the frame rate (for example 30000/1001 returns 30 fps)
int Framerate::GetRoundedFPS() {
	return round((float) m_numerator / m_denominator);
}

// Return a float of the frame rate (for example 30000/1001 returns 29.97...)
float Framerate::GetFPS() {
	return (float) m_numerator / m_denominator;
}

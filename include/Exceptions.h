/**
 * @file
 * @brief Header file for all Exception classes
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @section LICENSE
 *
 * Copyright (c) 2008-2014 OpenShot Studios, LLC
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

#ifndef OPENSHOT_EXCEPTIONS_H
#define OPENSHOT_EXCEPTIONS_H

#include <string>
using namespace std;

namespace openshot {

	/**
	 * @brief Base exception class with a custom message variable.
	 *
	 * A custom error message field has been added to the std::exception base class.  All
	 * OpenShot exception classes inherit from this class.
	 */
	class BaseException : public std::exception //: public exception
	{
	protected:
		string m_message;
	public:
		BaseException(string message) : m_message(message) { }
		virtual ~BaseException() throw () {}
		virtual const char* what() const throw () {
			// return custom message
			return m_message.c_str();
		}
	};

	/// Exception when a required chunk is missing
	class ChunkNotFound : public BaseException
	{
	public:
		string file_path;
		int frame_number;
		int chunk_number;
		int chunk_frame;
		ChunkNotFound(string message, int frame_number, int chunk_number, int chunk_frame)
			: BaseException(message), frame_number(frame_number), chunk_number(chunk_number), chunk_frame(chunk_frame) { }
		virtual ~ChunkNotFound() throw () {}
	};


	/// Exception when accessing a blackmagic decklink card
	class DecklinkError : public BaseException
	{
	public:
		DecklinkError(string message)
			: BaseException(message) { }
		virtual ~DecklinkError() throw () {}
	};

	/// Exception when decoding audio packet
	class ErrorDecodingAudio : public BaseException
	{
	public:
		string file_path;
		int frame_number;
		ErrorDecodingAudio(string message, int frame_number)
			: BaseException(message), frame_number(frame_number) { }
		virtual ~ErrorDecodingAudio() throw () {}
	};

	/// Exception when encoding audio packet
	class ErrorEncodingAudio : public BaseException
	{
	public:
		string file_path;
		int frame_number;
		ErrorEncodingAudio(string message, int frame_number)
			: BaseException(message), frame_number(frame_number) { }
		virtual ~ErrorEncodingAudio() throw () {}
	};

	/// Exception when encoding audio packet
	class ErrorEncodingVideo : public BaseException
	{
	public:
		string file_path;
		int frame_number;
		ErrorEncodingVideo(string message, int frame_number)
			: BaseException(message), frame_number(frame_number) { }
		virtual ~ErrorEncodingVideo() throw () {}
	};

	/// Exception when an invalid # of audio channels are detected
	class InvalidChannels : public BaseException
	{
	public:
		string file_path;
		InvalidChannels(string message, string file_path)
			: BaseException(message), file_path(file_path) { }
		virtual ~InvalidChannels() throw () {}
	};

	/// Exception when no valid codec is found for a file
	class InvalidCodec : public BaseException
	{
	public:
		string file_path;
		InvalidCodec(string message, string file_path)
			: BaseException(message), file_path(file_path) { }
		virtual ~InvalidCodec() throw () {}
	};

	/// Exception for files that can not be found or opened
	class InvalidFile : public BaseException
	{
	public:
		string file_path;
		InvalidFile(string message, string file_path)
			: BaseException(message), file_path(file_path) { }
		virtual ~InvalidFile() throw () {}
	};

	/// Exception when no valid format is found for a file
	class InvalidFormat : public BaseException
	{
	public:
		string file_path;
		InvalidFormat(string message, string file_path)
			: BaseException(message), file_path(file_path) { }
		virtual ~InvalidFormat() throw () {}
	};

	/// Exception for invalid JSON
	class InvalidJSON : public BaseException
	{
	public:
		string file_path;
		InvalidJSON(string message, string file_path)
			: BaseException(message), file_path(file_path) { }
		virtual ~InvalidJSON() throw () {}
	};

	/// Exception when invalid encoding options are used
	class InvalidOptions : public BaseException
	{
	public:
		string file_path;
		InvalidOptions(string message, string file_path)
			: BaseException(message), file_path(file_path) { }
		virtual ~InvalidOptions() throw () {}
	};

	/// Exception when invalid sample rate is detected during encoding
	class InvalidSampleRate : public BaseException
	{
	public:
		string file_path;
		InvalidSampleRate(string message, string file_path)
			: BaseException(message), file_path(file_path) { }
		virtual ~InvalidSampleRate() throw () {}
	};

	/// Exception for missing JSON Change key
	class InvalidJSONKey : public BaseException
	{
	public:
		string json;
		InvalidJSONKey(string message, string json)
			: BaseException(message), json(json) { }
		virtual ~InvalidJSONKey() throw () {}
	};

	/// Exception when no streams are found in the file
	class NoStreamsFound : public BaseException
	{
	public:
		string file_path;
		NoStreamsFound(string message, string file_path)
			: BaseException(message), file_path(file_path) { }
		virtual ~NoStreamsFound() throw () {}
	};

	/// Exception for frames that are out of bounds.
	class OutOfBoundsFrame : public BaseException
	{
	public:
		int FrameRequested;
		int MaxFrames;
		OutOfBoundsFrame(string message, int frame_requested, int max_frames)
			: BaseException(message), FrameRequested(frame_requested), MaxFrames(max_frames) { }
		virtual ~OutOfBoundsFrame() throw () {}
	};

	/// Exception for an out of bounds key-frame point.
	class OutOfBoundsPoint : public BaseException
	{
	public:
		int PointRequested;
		int MaxPoints;
		OutOfBoundsPoint(string message, int point_requested, int max_points)
			: BaseException(message), PointRequested(point_requested), MaxPoints(max_points) { }
		virtual ~OutOfBoundsPoint() throw () {}
	};

	/// Exception when memory could not be allocated
	class OutOfMemory : public BaseException
	{
	public:
		string file_path;
		OutOfMemory(string message, string file_path)
			: BaseException(message), file_path(file_path) { }
		virtual ~OutOfMemory() throw () {}
	};

	/// Exception when a reader is closed, and a frame is requested
	class ReaderClosed : public BaseException
	{
	public:
		string file_path;
		ReaderClosed(string message, string file_path)
			: BaseException(message), file_path(file_path) { }
		virtual ~ReaderClosed() throw () {}
	};

	/// Exception when resample fails
	class ResampleError : public BaseException
	{
	public:
		string file_path;
		ResampleError(string message, string file_path)
			: BaseException(message), file_path(file_path) { }
		virtual ~ResampleError() throw () {}
	};

	/// Exception when too many seek attempts happen
	class TooManySeeks : public BaseException
	{
	public:
		string file_path;
		TooManySeeks(string message, string file_path)
			: BaseException(message), file_path(file_path) { }
		virtual ~TooManySeeks() throw () {}
	};

	/// Exception when a writer is closed, and a frame is requested
	class WriterClosed : public BaseException
	{
	public:
		string file_path;
		WriterClosed(string message, string file_path)
			: BaseException(message), file_path(file_path) { }
		virtual ~WriterClosed() throw () {}
	};
}

#endif

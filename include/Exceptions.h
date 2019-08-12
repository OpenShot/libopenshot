/**
 * @file
 * @brief Header file for all Exception classes
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

#ifndef OPENSHOT_EXCEPTIONS_H
#define OPENSHOT_EXCEPTIONS_H

#include <string>

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
		std::string m_message;
	public:
		BaseException(std::string message) : m_message(message) { }
		virtual ~BaseException() noexcept {}
		virtual const char* what() const noexcept {
			// return custom message
			return m_message.c_str();
		}
	};

	/// Exception when a required chunk is missing
	class ChunkNotFound : public BaseException
	{
	public:
		int64_t frame_number;
		int64_t chunk_number;
		int64_t chunk_frame;
		ChunkNotFound(std::string message, int64_t frame_number, int64_t chunk_number, int64_t chunk_frame)
			: BaseException(message), frame_number(frame_number), chunk_number(chunk_number), chunk_frame(chunk_frame) { }
		virtual ~ChunkNotFound() noexcept {}
	};


	/// Exception when accessing a blackmagic decklink card
	class DecklinkError : public BaseException
	{
	public:
		DecklinkError(std::string message)
			: BaseException(message) { }
		virtual ~DecklinkError() noexcept {}
	};

	/// Exception when decoding audio packet
	class ErrorDecodingAudio : public BaseException
	{
	public:
		int64_t frame_number;
		ErrorDecodingAudio(std::string message, int64_t frame_number)
			: BaseException(message), frame_number(frame_number) { }
		virtual ~ErrorDecodingAudio() noexcept {}
	};

	/// Exception when encoding audio packet
	class ErrorEncodingAudio : public BaseException
	{
	public:
		int64_t frame_number;
		ErrorEncodingAudio(std::string message, int64_t frame_number)
			: BaseException(message), frame_number(frame_number) { }
		virtual ~ErrorEncodingAudio() noexcept {}
	};

	/// Exception when encoding audio packet
	class ErrorEncodingVideo : public BaseException
	{
	public:
		int64_t frame_number;
		ErrorEncodingVideo(std::string message, int64_t frame_number)
			: BaseException(message), frame_number(frame_number) { }
		virtual ~ErrorEncodingVideo() noexcept {}
	};

	/// Exception when an invalid # of audio channels are detected
	class InvalidChannels : public BaseException
	{
	public:
		std::string file_path;
		InvalidChannels(std::string message, std::string file_path)
			: BaseException(message), file_path(file_path) { }
		virtual ~InvalidChannels() noexcept {}
	};

	/// Exception when no valid codec is found for a file
	class InvalidCodec : public BaseException
	{
	public:
		std::string file_path;
		InvalidCodec(std::string message, std::string file_path)
			: BaseException(message), file_path(file_path) { }
		virtual ~InvalidCodec() noexcept {}
	};

	/// Exception for files that can not be found or opened
	class InvalidFile : public BaseException
	{
	public:
		std::string file_path;
		InvalidFile(std::string message, std::string file_path)
			: BaseException(message), file_path(file_path) { }
		virtual ~InvalidFile() noexcept {}
	};

	/// Exception when no valid format is found for a file
	class InvalidFormat : public BaseException
	{
	public:
		std::string file_path;
		InvalidFormat(std::string message, std::string file_path)
			: BaseException(message), file_path(file_path) { }
		virtual ~InvalidFormat() noexcept {}
	};

	/// Exception for invalid JSON
	class InvalidJSON : public BaseException
	{
	public:
		std::string file_path;
		InvalidJSON(std::string message, std::string file_path)
			: BaseException(message), file_path(file_path) { }
		virtual ~InvalidJSON() noexcept {}
	};

	/// Exception when invalid encoding options are used
	class InvalidOptions : public BaseException
	{
	public:
		std::string file_path;
		InvalidOptions(std::string message, std::string file_path)
			: BaseException(message), file_path(file_path) { }
		virtual ~InvalidOptions() noexcept {}
	};

	/// Exception when invalid sample rate is detected during encoding
	class InvalidSampleRate : public BaseException
	{
	public:
		std::string file_path;
		InvalidSampleRate(std::string message, std::string file_path)
			: BaseException(message), file_path(file_path) { }
		virtual ~InvalidSampleRate() noexcept {}
	};

	/// Exception for missing JSON Change key
	class InvalidJSONKey : public BaseException
	{
	public:
		std::string json;
		InvalidJSONKey(std::string message, std::string json)
			: BaseException(message), json(json) { }
		virtual ~InvalidJSONKey() noexcept {}
	};

	/// Exception when no streams are found in the file
	class NoStreamsFound : public BaseException
	{
	public:
		std::string file_path;
		NoStreamsFound(std::string message, std::string file_path)
			: BaseException(message), file_path(file_path) { }
		virtual ~NoStreamsFound() noexcept {}
	};

	/// Exception for frames that are out of bounds.
	class OutOfBoundsFrame : public BaseException
	{
	public:
		int64_t FrameRequested;
		int64_t MaxFrames;
		OutOfBoundsFrame(std::string message, int64_t frame_requested, int64_t max_frames)
			: BaseException(message), FrameRequested(frame_requested), MaxFrames(max_frames) { }
		virtual ~OutOfBoundsFrame() noexcept {}
	};

	/// Exception for an out of bounds key-frame point.
	class OutOfBoundsPoint : public BaseException
	{
	public:
		int PointRequested;
		int MaxPoints;
		OutOfBoundsPoint(std::string message, int point_requested, int max_points)
			: BaseException(message), PointRequested(point_requested), MaxPoints(max_points) { }
		virtual ~OutOfBoundsPoint() noexcept {}
	};

	/// Exception when memory could not be allocated
	class OutOfMemory : public BaseException
	{
	public:
		std::string file_path;
		OutOfMemory(std::string message, std::string file_path)
			: BaseException(message), file_path(file_path) { }
		virtual ~OutOfMemory() noexcept {}
	};

	/// Exception when a reader is closed, and a frame is requested
	class ReaderClosed : public BaseException
	{
	public:
		std::string file_path;
		ReaderClosed(std::string message, std::string file_path)
			: BaseException(message), file_path(file_path) { }
		virtual ~ReaderClosed() noexcept {}
	};

	/// Exception when resample fails
	class ResampleError : public BaseException
	{
	public:
		std::string file_path;
		ResampleError(std::string message, std::string file_path)
			: BaseException(message), file_path(file_path) { }
		virtual ~ResampleError() noexcept {}
	};

	/// Exception when too many seek attempts happen
	class TooManySeeks : public BaseException
	{
	public:
		std::string file_path;
		TooManySeeks(std::string message, std::string file_path)
			: BaseException(message), file_path(file_path) { }
		virtual ~TooManySeeks() noexcept {}
	};

	/// Exception when a writer is closed, and a frame is requested
	class WriterClosed : public BaseException
	{
	public:
		std::string file_path;
		WriterClosed(std::string message, std::string file_path)
			: BaseException(message), file_path(file_path) { }
		virtual ~WriterClosed() noexcept {}
	};
}

#endif

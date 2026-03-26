//
//  RNData.cpp
//  Rayne
//
//  Copyright 2015 by Überpixel. All rights reserved.
//  Unauthorized use is punishable by torture, mutilation, and vivisection.
//

#include <zlib.h>

#include "../Base/RNBase.h"
#include <fcntl.h>
#if RN_PLATFORM_POSIX
	#include <unistd.h>
	#define O_BINARY 0x0
#elif RN_PLATFORM_WINDOWS
	#include "../Base/RNUnistd.h"
#endif

#include <sys/stat.h>

#include "../System//RNFile.h"
#include "../System/RNFileManager.h"
#include "RNData.h"
#include "../Math/RNAlgorithm.h"
#include "RNSerialization.h"
#include "RNString.h"

#define kRNDataIncreaseLength 64
#define kRNDataReadBufferSize 1024

namespace RN
{
	static inline int8 GetBase64DecodedValue(unsigned char character, bool urlSafe)
	{
		if(character >= 'A' && character <= 'Z') return character - 'A';
		if(character >= 'a' && character <= 'z') return character - 'a' + 26;
		if(character >= '0' && character <= '9') return character - '0' + 52;
		if(urlSafe)
		{
			if(character == '-') return 62;
			if(character == '_') return 63;
		}
		else
		{
			if(character == '+') return 62;
			if(character == '/') return 63;
		}

		return -1;
	}

	static inline char GetBase64EncodedValue(uint8 value, bool urlSafe)
	{
		static const char *base64Characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
		static const char *base64URLCharacters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
		return urlSafe ? base64URLCharacters[value] : base64Characters[value];
	}

	RNDefineMeta(Data, Object)

	Data::Data()
	{
		_length = 0;
		_allocated = 10;
		_bytes = (uint8 *)malloc(_allocated);

		_ownsData = _freeData = true;
	}

	Data::Data(size_t length) :
		Data(nullptr, length, false, true)
	{
	}

	Data::Data(const void *bytes, size_t length) :
		Data(bytes, length, false, true)
	{
	}

	Data::Data(const void *bytes, size_t length, bool noCopy, bool deleteWhenDone)
	{
		if(noCopy)
		{
			_bytes = const_cast<uint8 *>(static_cast<const uint8 *>(bytes));
			_allocated = 0;
			_length = length;

			_ownsData = false;
			_freeData = deleteWhenDone;
		}
		else
		{
			Initialize(bytes, length);
		}
	}


	Data::Data(const Data *other)
	{
		Initialize(other->_bytes, other->_length);
	}

	Data::~Data()
	{
		if(_freeData)
			free(_bytes);
	}


	Data::Data(Deserializer *deserializer)
	{
		uint8 *data = static_cast<uint8 *>(deserializer->DecodeBytes(&_length));

		_allocated = _length;
		_freeData = _ownsData = true;
		_bytes = (uint8 *)malloc(_allocated);

		std::copy(data, data + _length, _bytes);
	}

	void Data::Serialize(Serializer *serializer) const
	{
		serializer->EncodeBytes(_bytes, _length);
	}

	String *Data::GetSha256Hash() const
	{
		char hex[65];
		ComputeSHA256Hex(_bytes, _length, hex);
		return String::WithString(hex, false);
	}


	Data *Data::WithBytes(const uint8 *bytes, size_t length)
	{
		Data *data = new Data(bytes, length);
		return data->Autorelease();
	}

	Data *Data::WithBase64EncodedString(const String *string, bool urlSafe)
	{
		if(!string) return nullptr;

		std::string input(string->GetUTF8String());
		std::string output;
		output.reserve((input.length() / 4) * 3 + 3);

		int value = 0;
		int bits = -8;
		for(unsigned char character : input)
		{
			if(character == '=')
			{
				break;
			}

			if(character == ' ' || character == '\n' || character == '\r' || character == '\t')
			{
				continue;
			}

			int decodedValue = GetBase64DecodedValue(character, urlSafe);
			if(decodedValue < 0) return nullptr;

			value = (value << 6) + decodedValue;
			bits += 6;
			if(bits >= 0)
			{
				output.push_back(static_cast<char>((value >> bits) & 0xFF));
				bits -= 8;
			}
		}

		return Data::WithBytes(reinterpret_cast<const uint8 *>(output.data()), output.length());
	}

	Expected<Data *> Data::WithContentsOfFile(const String *path)
	{
		File *file = File::WithName(path, File::Mode::Read | File::Mode::NoCreate);
		if(!file)
			return InvalidArgumentException(RNSTR("Couldn't open file " << path));

		size_t size = file->GetSize();
		uint8 *bytes = (uint8 *)malloc(size);
		file->Read(bytes, size);

		Data *data = new Data(bytes, size, false, true);
		return data->Autorelease();
	}


	void Data::Initialize(const void *bytes, size_t length)
	{
		_ownsData = true;
		_freeData = true;

		_allocated = length;
		_length = length;

		_bytes = (uint8 *)malloc(_allocated);

		if(bytes)
		{
			const uint8 *data = static_cast<const uint8 *>(bytes);
			std::copy(data, data + _length, _bytes);
		}
	}

	void Data::AssertSize(size_t minimumLength)
	{
		if(!_ownsData)
			throw InconsistencyException("The Data object doesn't own it's data and can't modify it!");

		if(_allocated < minimumLength)
		{
			uint8 *temp = (uint8 *)malloc(minimumLength + kRNDataIncreaseLength);
			std::copy(_bytes, _bytes + _length, temp);
			free(_bytes);

			_bytes = temp;
			_allocated = minimumLength + kRNDataIncreaseLength;
		}
	}


	void Data::Append(const void *bytes, size_t length)
	{
		AssertSize(_length + length);
		const uint8 *data = static_cast<const uint8 *>(bytes);

		std::copy(data, data + length, _bytes + _length);
		_length += length;
	}

	void Data::Append(Data *other)
	{
		Append(other->_bytes, other->_length);
	}

	void Data::ReplaceBytes(const void *bytes, const Range &range)
	{
		if(range.origin + range.length > _length)
			throw RangeException("range is not within the datas bounds!");

		const uint8 *data = static_cast<const uint8 *>(bytes);
		std::copy(data, data + range.length, _bytes + range.origin);
	}

	size_t Data::WriteToFile(const String *path)
	{
		File *file = File::WithName(path, File::Mode::Write);
		if(!file)
			throw InvalidArgumentException(RNSTR("Couldn't create file " << path));

		return file->Write(_bytes, _length);
	}

	String *Data::GetBase64EncodedString(bool urlSafe) const
	{
		std::string output;
		output.reserve(((_length + 2) / 3) * 4);

		for(size_t index = 0; index < _length; index += 3)
		{
			uint8 first = _bytes[index];
			uint8 second = (index + 1 < _length) ? _bytes[index + 1] : 0;
			uint8 third = (index + 2 < _length) ? _bytes[index + 2] : 0;

			output.push_back(GetBase64EncodedValue((first >> 2) & 0x3F, urlSafe));
			output.push_back(GetBase64EncodedValue(((first & 0x03) << 4) | ((second >> 4) & 0x0F), urlSafe));
			output.push_back((index + 1 < _length) ? GetBase64EncodedValue(((second & 0x0F) << 2) | ((third >> 6) & 0x03), urlSafe) : '=');
			output.push_back((index + 2 < _length) ? GetBase64EncodedValue(third & 0x3F, urlSafe) : '=');
		}

		return String::WithBytes(output.data(), output.length(), Encoding::UTF8);
	}


	Data *Data::GetDataInRange(Range range) const
	{
		if(range.origin + range.length > _length)
			throw RangeException("range is not within the datas bounds!");

		uint8 *data = (uint8 *)malloc(range.length);
		std::copy(_bytes + range.origin, _bytes + range.origin + range.length, data);

		Data *temp = new Data(data, range.length, true, true);
		return temp->Autorelease();
	}

	void Data::GetBytesInRange(void *buffer, Range range) const
	{
		if(range.origin + range.length > _length)
			throw RangeException("range is not within the datas bounds!");

		uint8 *data = static_cast<uint8 *>(buffer);
		std::copy(_bytes + range.origin, _bytes + range.origin + range.length, data);
	}

	// deflate compression
	// adapted from https://stackoverflow.com/a/4538786
	Data *Data::GetCompressed() const
	{
		RN::Data *compressed = new RN::Data();

		const size_t BUFSIZE = 128 * 1024;
		uint8_t temp_buffer[BUFSIZE];

		z_stream strm;
		strm.zalloc = 0;
		strm.zfree = 0;
		strm.next_in = _bytes;
		strm.avail_in = _length;
		strm.next_out = temp_buffer;
		strm.avail_out = BUFSIZE;

		deflateInit(&strm, Z_BEST_COMPRESSION);

		while(strm.avail_in != 0)
		{
			int res = deflate(&strm, Z_NO_FLUSH);

			if(res != Z_OK)
			{
				compressed->Release();
				return nullptr;
			}

			if(strm.avail_out == 0)
			{
				compressed->Append(temp_buffer, BUFSIZE);
				strm.next_out = temp_buffer;
				strm.avail_out = BUFSIZE;
			}
		}

		int deflate_res = Z_OK;
		while(deflate_res == Z_OK)
		{
			if(strm.avail_out == 0)
			{
				compressed->Append(temp_buffer, BUFSIZE);
				strm.next_out = temp_buffer;
				strm.avail_out = BUFSIZE;
			}
			deflate_res = deflate(&strm, Z_FINISH);
		}

		if(deflate_res != Z_STREAM_END)
		{
			compressed->Release();
			return nullptr;
		}

		compressed->Append(temp_buffer, BUFSIZE - strm.avail_out);
		deflateEnd(&strm);

		return compressed;
	}

	// deflate decompression
	Data *Data::GetDecompressed() const
	{
		RN::Data *decompressed = new RN::Data();

		const size_t BUFSIZE = 128 * 1024;
		uint8_t temp_buffer[BUFSIZE];

		z_stream strm;
		strm.zalloc = 0;
		strm.zfree = 0;
		strm.next_in = _bytes;
		strm.avail_in = _length;
		strm.next_out = temp_buffer;
		strm.avail_out = BUFSIZE;

		inflateInit(&strm);

		int inflate_res = Z_OK;
		while(inflate_res == Z_OK)
		{
			if(strm.avail_out == 0)
			{
				decompressed->Append(temp_buffer, BUFSIZE);
				strm.next_out = temp_buffer;
				strm.avail_out = BUFSIZE;
			}
			inflate_res = inflate(&strm, Z_NO_FLUSH);
		}

		if(inflate_res != Z_STREAM_END)
		{
			decompressed->Release();
			return nullptr;
		}

		decompressed->Append(temp_buffer, BUFSIZE - strm.avail_out);
		inflateEnd(&strm);

		return decompressed;
	}
} // namespace RN

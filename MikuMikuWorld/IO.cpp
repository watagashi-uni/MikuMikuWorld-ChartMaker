#include "IO.h"
#ifdef _WIN32
#include <Windows.h>
#endif
#ifdef __APPLE__
#include <AppKit/AppKit.h>
#endif
#include <algorithm>
#include <zlib.h>
#include <sstream>
#include <cassert>
#include <cctype>
#include <codecvt>
#include <locale>

#undef min
#undef max

namespace IO
{
	MessageBoxResult messageBox(std::string title, std::string message, MessageBoxButtons buttons, MessageBoxIcon icon, void* parentWindow)
	{
#ifdef _WIN32
		UINT flags = 0;
		switch (icon)
		{
		case MessageBoxIcon::Information:	flags |= MB_ICONINFORMATION; break;
		case MessageBoxIcon::Warning:		flags |= MB_ICONWARNING; break;
		case MessageBoxIcon::Error:			flags |= MB_ICONERROR; break;
		case MessageBoxIcon::Question:		flags |= MB_ICONQUESTION; break;
		default: break;
		}

		switch (buttons)
		{
		case MessageBoxButtons::Ok:			flags |= MB_OK; break;
		case MessageBoxButtons::OkCancel:	flags |= MB_OKCANCEL; break;
		case MessageBoxButtons::YesNo:		flags |= MB_YESNO; break;
		case MessageBoxButtons::YesNoCancel:flags |= MB_YESNOCANCEL; break;
		default: break;
		}

		const int result = MessageBoxExW(reinterpret_cast<HWND>(parentWindow), mbToWideStr(message).c_str(), mbToWideStr(title).c_str(), flags, 0);
		switch (result)
		{
		case IDABORT:	return MessageBoxResult::Abort;
		case IDCANCEL:	return MessageBoxResult::Cancel;
		case IDIGNORE:	return MessageBoxResult::Ignore;
		case IDNO:		return MessageBoxResult::No;
		case IDYES:		return MessageBoxResult::Yes;
		case IDOK:		return MessageBoxResult::Ok;
		default:		return MessageBoxResult::None;
		}
#elif defined(__APPLE__)
		@autoreleasepool
		{
			NSAlert* alert = [[NSAlert alloc] init];
			[alert setMessageText:[NSString stringWithUTF8String:title.c_str()] ?: @""];
			[alert setInformativeText:[NSString stringWithUTF8String:message.c_str()] ?: @""];

			switch (icon)
			{
			case MessageBoxIcon::Warning: [alert setAlertStyle:NSAlertStyleWarning]; break;
			case MessageBoxIcon::Error: [alert setAlertStyle:NSAlertStyleCritical]; break;
			default: [alert setAlertStyle:NSAlertStyleInformational]; break;
			}

			switch (buttons)
			{
			case MessageBoxButtons::OkCancel:
				[alert addButtonWithTitle:@"OK"];
				[alert addButtonWithTitle:@"Cancel"];
				break;
			case MessageBoxButtons::YesNo:
				[alert addButtonWithTitle:@"Yes"];
				[alert addButtonWithTitle:@"No"];
				break;
			case MessageBoxButtons::YesNoCancel:
				[alert addButtonWithTitle:@"Yes"];
				[alert addButtonWithTitle:@"No"];
				[alert addButtonWithTitle:@"Cancel"];
				break;
			case MessageBoxButtons::Ok:
			default:
				[alert addButtonWithTitle:@"OK"];
				break;
			}

			const NSModalResponse response = [alert runModal];
			const NSInteger buttonIndex = response - NSAlertFirstButtonReturn;
			if (buttons == MessageBoxButtons::YesNo || buttons == MessageBoxButtons::YesNoCancel)
			{
				if (buttonIndex == 0) return MessageBoxResult::Yes;
				if (buttonIndex == 1) return MessageBoxResult::No;
				if (buttonIndex == 2) return MessageBoxResult::Cancel;
			}
			else if (buttons == MessageBoxButtons::OkCancel && buttonIndex == 1)
			{
				return MessageBoxResult::Cancel;
			}
			return MessageBoxResult::Ok;
		}
#else
		return MessageBoxResult::Ok;
#endif
	}

	char* reverse(char* str)
	{
		char* end = str;
		char* start = str;

		if (!str || !*str) return str;
		while (*(end + 1)) end++;
		while (end > start)
		{
			int ch = *end;
			*end-- = *start;
			*start++ = ch;
		}
		return str;
	}

	char* tostringBaseN(char* buff, long long num, int base)
	{
		int sign = num < 0;
		char* savedbuff = buff;

		if (base < 2 || base >= sizeof(digits)) return NULL;
		if (buff)
		{
			do
			{
				*buff++ = digits[abs(num % base)];
				num /= base;
			} while (num);
			if (sign)
			{
				*buff++ = '-';
			}
			*buff = 0;
			reverse(savedbuff);
		}
		return savedbuff;
	}

	bool isComment(const std::string_view& line, const std::string_view& delim)
	{
		if (line.empty())
			return true;

		return line.find_first_of(delim) == 0 || line.at(0) == '\n';
	}

	bool startsWith(const std::string_view& line, const std::string_view& key)
	{
		if (line.length() < key.length())
			return false;

		return std::equal(key.begin(), key.end(), line.begin());
	}

	bool endsWith(const std::string_view& line, const std::string_view& key)
	{
		if (line.length() < key.length())
			return false;

		return std::equal(key.rbegin(), key.rend(), line.rbegin());
	}

	bool isDigit(const std::string_view& str)
	{
		if (str.empty())
			return false;

		return std::all_of(str.begin() + (str.at(0) == '-' ? 1 : 0), str.end(), [](unsigned char c) { return std::isdigit(c); });
	}

	std::string trim(const std::string& line)
	{
		if (line.empty())
			return line;

		size_t start = line.find_first_not_of(" \t\r\n");
		if (start == std::string::npos)
			return "";

		size_t end = line.find_last_not_of(" \t\r\n");

		return line.substr(start, end - start + 1);
	}

	std::vector<std::string> split(const std::string& line, const std::string& delim)
	{
		std::vector<std::string> values;
		size_t start = 0;
		size_t end = line.length() - 1;

		while (start < line.length() && end != std::string::npos)
		{
			end = line.find_first_of(delim, start);
			values.push_back(line.substr(start, end - start));

			start = end + 1;
		}

		return values;
	}

	std::string wideStringToMb(const std::wstring& str)
	{
#ifdef _WIN32
		if (str.empty())
			return {};

		int size = WideCharToMultiByte(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0, NULL, NULL);
		std::string result(size, 0);
		WideCharToMultiByte(CP_UTF8, 0, &str[0], (int)str.size(), &result[0], size, NULL, NULL);

		return result;
#else
		std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
		return converter.to_bytes(str);
#endif
	}

	std::wstring mbToWideStr(const std::string& str)
	{
#ifdef _WIN32
		if (str.empty())
			return {};

		int size = MultiByteToWideChar(CP_UTF8, 0, &str[0], str.size(), NULL, 0);
		std::wstring wResult(size, 0);
		MultiByteToWideChar(CP_UTF8, 0, &str[0], str.size(), &wResult[0], size);

		return wResult;
#else
		std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
		return converter.from_bytes(str);
#endif
	}

	std::string concat(const char* s1, const char* s2, const char* join)
	{
		return std::string(s1).append(join).append(s2);
	}

	static std::vector<uint8_t> processCompressionStream(const std::vector<uint8_t>& data, z_stream* stream, int flush, int (*process)(z_streamp, int))
	{
		stream->avail_in = data.size();
		stream->next_in = (Byte*)data.data();

		std::vector<uint8_t> dest;
		std::unique_ptr<Byte[]> buf(new Byte[Z_CHUNK_SIZE]);
		int result = Z_OK;

		do
		{
			stream->next_out = buf.get();
			stream->avail_out = Z_CHUNK_SIZE;

			result = process(stream, flush);
			if (dest.size() < stream->total_out)
			{
				size_t count = Z_CHUNK_SIZE - stream->avail_out;
				for (size_t i = 0; i < count; i++)
					dest.push_back(buf[i]);
			}

		} while (result == Z_OK);

		return dest;
	}

	std::vector<uint8_t> inflateGzip(const std::vector<uint8_t>& data)
	{
		z_stream stream{};
		memset(&stream, 0, sizeof(z_stream));

		int result = inflateInit2(&stream, 15 | 16);
		if (result != Z_OK)
			return {};

		std::vector<uint8_t> dest = processCompressionStream(data, &stream, Z_FULL_FLUSH, inflate);

		inflateEnd(&stream);
		return dest;
	}

	std::vector<uint8_t> deflateGzip(const std::vector<uint8_t>& data)
	{
		z_stream stream{};
		memset(&stream, 0, sizeof(z_stream));

		int result = deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 | 16, 8, Z_DEFAULT_STRATEGY);
		if (result != Z_OK)
			return {};

		std::vector<uint8_t> dest = processCompressionStream(data, &stream, Z_FINISH, deflate);

		deflateEnd(&stream);
		return dest;
	}

	bool isGzipCompressed(const std::vector<uint8_t>& data)
	{
		return data.size() > 2 && data[0] == 0x1F && data[1] == 0x8B;
	}
}

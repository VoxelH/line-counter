#include "utils.hpp"
#include <system_error>
#include <windows.h>
#include <stringapiset.h>

namespace vlc
{
	bool is_console_handle(HANDLE h)
	{
		DWORD mode{};
		return GetConsoleMode(h, &mode) != FALSE;
	}

	void write_all_bytes_to_file(HANDLE h, const char* data, size_t size)
	{
		size_t remaining = size;
		while (remaining > 0)
		{
			DWORD written;
			if (false == WriteFile(h, data, remaining, &written, nullptr))
				throw std::system_error(GetLastError(), std::system_category(), "Failed to write to file");
			remaining -= written;
		}
	}

	bool read_all_bytes_from_file(HANDLE h, char* buffer, size_t want)
	{
		size_t remaining = want;
		while (remaining > 0)
		{
			char * ptr = buffer;
			ptr += want - remaining;

			DWORD read;
			if (false == ReadFile(h, ptr, remaining, &read, nullptr))
			{
				DWORD last_error = GetLastError();
				if (last_error == ERROR_HANDLE_EOF)
					return true;
				else
					throw std::system_error(last_error, std::system_category(), "Failed to reading from file");
			}
			remaining -= read;
		}

		return false;
	}

	void write_stdout(const std::wstring& wstr)
	{
		HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
		if (is_console_handle(stdout_handle))
		{
			DWORD junk;
			if (false == WriteConsoleW(stdout_handle, wstr.data(), wstr.size(), &junk, nullptr))
				throw std::system_error(GetLastError(), std::system_category(), "Failed writing to console");
		}
		else
		{
			std::string utf8 = u16tou8(wstr);
			write_all_bytes_to_file(stdout_handle, utf8.data(), utf8.size());
		}
	}

	void writeln_stdout(const std::wstring& wstr)
	{
		write_stdout(wstr + L'\n');
	}

	std::string u16tou8(const std::wstring& wstr, bool throw_on_invalid_chars)
	{
		if (wstr.empty())
			return {};
	
		int flags = throw_on_invalid_chars ? WC_ERR_INVALID_CHARS : 0;

		int required_size = WideCharToMultiByte(CP_UTF8, flags, wstr.data(), wstr.size(), nullptr, 0, nullptr, nullptr);
		if (required_size <= 0)
			throw std::system_error(GetLastError(), std::system_category(), "Failed to convert the given string");

		std::string str;
		str.resize(required_size);

		int written = WideCharToMultiByte(CP_UTF8, flags, wstr.data(), wstr.size(), str.data(), required_size, nullptr, nullptr);
		if (written <= 0)
			throw std::system_error(GetLastError(), std::system_category(), "Failed to convert the given string");

		return str;
	}

	std::wstring u8tou16(const std::string& str, bool throw_on_invalid_chars)
	{
		if (str.empty())
			return {};

		int flags = throw_on_invalid_chars ? MB_ERR_INVALID_CHARS : 0;

		int required_size = MultiByteToWideChar(CP_UTF8, flags, str.data(), static_cast<int>(str.size()), nullptr, 0);
		if (required_size <= 0)
			throw std::system_error(GetLastError(), std::system_category(), "Failed to convert the given string");

		std::wstring wstr;
		wstr.resize(static_cast<size_t>(required_size));

		int written = MultiByteToWideChar(CP_UTF8, flags, str.data(), static_cast<int>(str.size()), wstr.data(), required_size);
		if (written <= 0)
			throw std::system_error(GetLastError(), std::system_category(), "Failed to convert the given string");

		return wstr;
	}

}
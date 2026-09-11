#pragma once
#include <string>
#include <Windows.h>

namespace vlc
{
	bool is_console_handle(HANDLE h);
	void write_all_bytes_to_file(HANDLE h, const char* data, size_t size);

	// RESULT: Designates whether EOF has been met
	bool read_all_bytes_from_file(HANDLE h, char* buffer, size_t want);


	// stdout is the console output handle: directly write wide string sequence
	// stdout is redirected to a file handle: convert to utf8 character sequence and then write to the file
	void write_stdout(const std::wstring & wstr);
	void writeln_stdout(const std::wstring& wstr);

	std::string u16tou8(const std::wstring & wstr, bool throw_on_invalid_chars = true);
	std::wstring u8tou16(const std::string & str, bool throw_on_invalid_chars = true);
}
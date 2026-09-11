#include <vector>
#include <string>
#include <algorithm>
#include <iterator>
#include <iostream>
#include <filesystem>
#include "fs.hpp"
#include "str.hpp"
#include "utils.hpp"
#include "cmdparser.hpp"

using namespace vlc;

constexpr int LINE_COUNT_STR_ALIGMENT_SIZE = 10;

int main()
{
	int argc;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	std::vector<std::wstring> args(argc);
	for (int i = 0; i < argc; i++)
		args[i] = argv[i];
	LocalFree(argv);

	vlc::ParsedVlcCommandLineArguments pa;
	try
	{
		pa = vlc::parse_vlc_cmdline(args);
	}
	catch (const std::exception& e)
	{
		writeln_stdout(u8tou16(e.what()));
		return -1;
	}

	writeln_stdout(std::wstring(L"Option follow_symlink: ") + (pa.follow_symlink ? L"TRUE" : L"FALSE"));
	writeln_stdout(std::wstring(L"Option follow_junction_or_mountpoint: ") + (pa.follow_junction_and_mountpoint ? L"TRUE" : L"FALSE"));
	for (auto&& inc_ptrn : pa.include_patterns)
		writeln_stdout(L"Includes: " + inc_ptrn.to_str());
	for (auto&& exc_ptrn : pa.exclude_patterns)
		writeln_stdout(L"Excludes: " + exc_ptrn.to_str());

	auto unable_enter = [](const std::filesystem::path& p, const std::exception_ptr& exception) {
		write_stdout(L"\033[31m");
		write_stdout(L"Failed to open dir ");
		write_stdout(p.c_str());
		write_stdout(L": ");
		try
		{
			std::rethrow_exception(exception);
		}
		catch (const std::exception& e)
		{
			write_stdout(u8tou16(e.what()));
		}
		writeln_stdout(L"\033[0m");
		};
	auto unable_enum = [](const std::filesystem::path& p, const std::exception_ptr& exception) {
		write_stdout(L"\033[31m");
		write_stdout(L"Failed to enum dir ");
		write_stdout(p.c_str());
		write_stdout(L": ");
		try
		{
			std::rethrow_exception(exception);
		}
		catch (const std::exception& e)
		{
			write_stdout(u8tou16(e.what()));
		}
		writeln_stdout(L"\033[0m");
		};

	size_t total_line_count = 0;

	writeln_stdout(L"LINE       FILE");

	auto handle_single_file = [&](const RegularPath& current_entry)
		{
			DWORD file_attributes = GetFileAttributesW(current_entry.to_str().c_str());
			if (file_attributes & FILE_ATTRIBUTE_DIRECTORY) return;

			UniqueHandle<CommonHandleDeleter> hFile{ CreateFileW(current_entry.to_str().c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr) };
			if (hFile.get() == INVALID_HANDLE_VALUE)
			{
				throw std::system_error(GetLastError(), std::system_category(), "Failed to open file");
			}
			LARGE_INTEGER file_size;
			if (false == GetFileSizeEx(hFile.get(), &file_size))
			{
				throw std::system_error(GetLastError(), std::system_category(), "Failed to get file size");
			}
			size_t line_count = 1;
			auto buffer = std::make_unique<char[]>(file_size.QuadPart);
			read_all_bytes_from_file(hFile.get(), buffer.get(), file_size.QuadPart);

			for (size_t offset = 0; offset < file_size.QuadPart; offset++)
			{
				if (buffer[offset] == '\n') line_count += 1;
				else if (buffer[offset] == '\r')
				{
					if (offset != file_size.QuadPart - 1 && buffer[offset + 1] == '\n')
						offset++;
					line_count += 1;
				}
			}
			total_line_count += line_count;

			std::wstring lc_str = std::to_wstring(line_count);
			if (lc_str.size() < LINE_COUNT_STR_ALIGMENT_SIZE) 
				lc_str += std::wstring(LINE_COUNT_STR_ALIGMENT_SIZE - lc_str.size(), L' ');

			writeln_stdout(lc_str + L" " + current_entry.to_str());
		};

	for (auto&& inc_ptrn : pa.include_patterns)
	{
		walk_dir_match(
			inc_ptrn, pa.follow_symlink, pa.follow_junction_and_mountpoint, true, false,
			[&](const vlc::RegularPath& entry)
			{
				for (auto&& exclude_ptrn : pa.exclude_patterns)
				{
					if (vlc::match_lexically(exclude_ptrn, entry, false))
						return;
				}
				try
				{
					handle_single_file(entry);
				}
				catch (const std::exception& e)
				{
					write_stdout(L"\033[31m");
					write_stdout(L"Failed to read file ");
					write_stdout(entry.to_str());
					write_stdout(L": ");
					write_stdout(u8tou16(e.what()));
					writeln_stdout(L"\033[0m");
				}
			},
			unable_enter, unable_enum);
	}

	writeln_stdout(L"\nTotal: " + std::to_wstring(total_line_count));
}
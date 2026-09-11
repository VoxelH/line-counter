#pragma once

#include "fs.hpp"
#include <vector>
#include <map>

namespace vlc
{
	struct ParsedArguments
	{
		std::vector<std::wstring> toggle_options;
		std::map<std::wstring, std::wstring> keyvalue_options;
		std::vector<std::wstring> ordinary_arguments;
	};

	ParsedArguments common_parse_cmdline(const std::vector<std::wstring>& args);

	struct ParsedVlcCommandLineArguments
	{
		bool follow_symlink;
		bool follow_junction_and_mountpoint;
		std::vector<RegularPath> include_patterns;
		std::vector<RegularPath> exclude_patterns;
	};

	// Ensures there is at least one include pattern
	// Ensures there is at least one exclude pattern when `--exclude` is specified
	ParsedVlcCommandLineArguments parse_vlc_cmdline(const std::vector<std::wstring> & args);
}
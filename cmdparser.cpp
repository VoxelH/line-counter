#include "cmdparser.hpp"
#include "str.hpp"
#include "utils.hpp"
#include <map>

namespace vlc
{
	ParsedArguments common_parse_cmdline(const std::vector<std::wstring>& args)
	{
		ParsedArguments result;

		auto iter_arg_sep = std::find(args.begin() + 1, args.end(), L"--");
		if (iter_arg_sep != args.end())
		{
			for (auto option_iter = args.begin() + 1; option_iter != iter_arg_sep; option_iter++)
			{
				auto&& option_str = *option_iter;
				if (!str::starts_with(std::wstring_view(option_str), std::wstring_view(L"--")) || option_str.size() <= 2)
				{
					std::string errmsg = "Options should be formatted as `--<key>=<value>` or `--<toggle>` but \"";
					errmsg += u16tou8(option_str);
					errmsg += "\" is met";
					throw std::invalid_argument(errmsg);
				}
				auto eq_op_iter = option_str.size() > 2 ? std::find(option_str.begin() + 2, option_str.end(), L'=') : option_str.end();

				auto key = option_str.substr(2, eq_op_iter - option_str.begin() - 2);
				if (result.keyvalue_options.find(key) != result.keyvalue_options.end() || std::find(result.toggle_options.begin(), result.toggle_options.end(), key) != result.toggle_options.end())
					throw std::invalid_argument(std::string("Duplicate option key: ") + u16tou8(key));

				if (eq_op_iter != option_str.end())  // kv option
				{
					auto value = option_str.substr(eq_op_iter - option_str.begin() + 1);
					result.keyvalue_options[key] = value;
				}
				else
				{
					result.toggle_options.push_back(key);
				}
			}
			std::copy(iter_arg_sep + 1, args.end(), std::back_inserter(result.ordinary_arguments));
		}
		else
		{
			std::copy(args.begin() + 1, args.end(), std::back_inserter(result.ordinary_arguments));
		}
		return result;
	}

	ParsedVlcCommandLineArguments parse_vlc_cmdline(const std::vector<std::wstring>& args)
	{
		ParsedArguments parsed_cmdline = common_parse_cmdline(args);
		ParsedVlcCommandLineArguments pcla;

		if (std::find(parsed_cmdline.toggle_options.begin(), parsed_cmdline.toggle_options.end(), L"follow-symlink") != parsed_cmdline.toggle_options.end())
		{
			throw std::invalid_argument("`--follow-symlink=<BOOL>` is not a toggle option");
		}
		if (std::find(parsed_cmdline.toggle_options.begin(), parsed_cmdline.toggle_options.end(), L"follow-junction-and-mountpoint") != parsed_cmdline.toggle_options.end())
		{
			throw std::invalid_argument("`--follow-junction-and-mountpoint=<BOOL>` is not a toggle option");
		}

		auto follow_symlink_option_iter = parsed_cmdline.keyvalue_options.find(L"follow-symlink");
		auto follow_junction_and_mountpoint_option_iter = parsed_cmdline.keyvalue_options.find(L"follow-junction-and-mountpoint");

		if (follow_symlink_option_iter != parsed_cmdline.keyvalue_options.end())
		{
			auto&& option_value = follow_symlink_option_iter->second;
			if (option_value == L"true") pcla.follow_symlink = true;
			else if (option_value == L"false") pcla.follow_symlink = false;
			else throw std::invalid_argument(u16tou8(option_value) + " is not a valid option value for `--follow-symlink=<BOOL>`. Only `true` and `false` are valid");
		}
		else
			pcla.follow_symlink = true;

		if (follow_junction_and_mountpoint_option_iter != parsed_cmdline.keyvalue_options.end())
		{
			auto&& option_value = follow_junction_and_mountpoint_option_iter->second;
			if (option_value == L"true") pcla.follow_junction_and_mountpoint = true;
			else if (option_value == L"false") pcla.follow_junction_and_mountpoint = false;
			else throw std::invalid_argument(u16tou8(option_value) + " is not a valid option value for `--follow-junction-or-mountpoint=<BOOL>`. Only `true` and `false` are valid");
		}
		else
			pcla.follow_junction_and_mountpoint = true;

		auto exclude_sep_iter = std::find(parsed_cmdline.ordinary_arguments.begin(), parsed_cmdline.ordinary_arguments.end(), L"--exclude");
		if (exclude_sep_iter - parsed_cmdline.ordinary_arguments.begin() == 0) throw std::invalid_argument("There must be at least one include pattern");
		for (auto iter = parsed_cmdline.ordinary_arguments.begin(); iter != exclude_sep_iter; iter++)
			pcla.include_patterns.push_back(RegularPath::from_str(*iter));

		if (exclude_sep_iter != parsed_cmdline.ordinary_arguments.end())
		{
			if (exclude_sep_iter == parsed_cmdline.ordinary_arguments.end() - 1) throw std::invalid_argument("There must be at least one exclude pattern when `--exclude` is specified");
			for (auto iter = exclude_sep_iter + 1; iter != parsed_cmdline.ordinary_arguments.end(); iter++)
				pcla.exclude_patterns.push_back(RegularPath::from_str(*iter));
		}

		return pcla;
	}
}
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace vlc
{
	namespace str
	{
		template <typename CharT>
		bool starts_with(std::basic_string_view<CharT> str, std::basic_string_view<CharT> prefix)
		{
			if (str.length() < prefix.length()) return false;
			for (size_t i = 0; i < prefix.length(); i++) 
				if (prefix[i] != str[i])
					return false;
			return true;
		}

		template <typename CharT>
		bool ends_with(std::basic_string_view<CharT> str, std::basic_string_view<CharT> suffix)
		{
			if (str.length() < suffix.length()) return false;
			for (size_t i = 0; i < suffix.length(); i++)
				if (suffix[i] != str[str.length() - suffix.length() + i])
					return false;
			return true;
		}


		template <typename CharT>
		CharT to_upper(CharT ch)
		{
			return ('a' <= ch && ch <= 'z') ? ch - 32 : ch;
		}

		template <typename CharT>
		std::basic_string<CharT> to_upper(std::basic_string_view<CharT> text)
		{
			std::basic_string<CharT> result;
			for (CharT ch : text)
				result += to_upper(ch);
			return result;
		}

		template <typename CharT>
		std::vector<std::basic_string<CharT>> split(std::basic_string_view<CharT> text, CharT separator)
		{
			std::vector<std::basic_string<CharT>> result = { std::basic_string<CharT>() };
			for (CharT ch : text)
			{
				if (ch == separator) { result.emplace_back(); }
				else { result.back() += ch; }
			}
			return result;
		}

		template <typename CharT, typename InputIt>
		std::basic_string<CharT> join(InputIt _begin, InputIt _end, CharT separator)
		{
			std::basic_string<CharT> result;
			for (InputIt iter = _begin; iter != _end; iter++)
			{
				if (iter != _begin) { result += separator; }
				result += *iter;
			}
			return result;
		}

		template <typename CharT>
		std::basic_string<CharT> replace(std::basic_string_view<CharT> str, CharT _old, CharT _new)
		{
			std::basic_string<CharT> result;
			for (CharT ch : str)
				result += ch == _old ? _new : ch;
			return result;
		}

		template <typename CharT>
		bool match(std::basic_string_view<CharT> text, std::basic_string_view<CharT> pattern)
		{
			using StringType = std::basic_string<CharT>;
			using StringViewType = std::basic_string_view<CharT>;
			static constexpr CharT ASTERISK = static_cast<CharT>('*');
			static constexpr CharT QUESTION_MARK = static_cast<CharT>('?');

			// text_slice sub_pattern 可以是空字符串
			auto match_subpattern = [](StringViewType text_slice, StringViewType subpattern) -> bool
				{
					if (text_slice.length() != subpattern.length()) return false;
					for (size_t i = 0; i < text_slice.length(); i++)
					{
						bool match = (subpattern[i] == QUESTION_MARK) ? true : text_slice[i] == subpattern[i];
						if (!match) { return false; }
					}
					return true;
				};

			if (pattern.empty()) return text.empty();

			// 去除重复 *
			StringType simplified_pattern;
			for (CharT ch : pattern)
			{
				if (ch != ASTERISK) { simplified_pattern += ch; }
				else if (!(simplified_pattern.length() >= 1 && simplified_pattern.back() == ASTERISK)) { simplified_pattern += ASTERISK; }
			}

			// subpattern中不存在ASTERISK
			std::vector<StringType> split_subpatterns = split(StringViewType(simplified_pattern), ASTERISK);

			// 空pattern字符串被排除后，split_subpattern存在空字符串 <=> 元素数量>=2
			bool ignore_prefix = split_subpatterns.front().empty();
			bool ignore_suffix = split_subpatterns.back().empty();

			// 实际上空subpattern序列现在也能返回true了
			if (ignore_prefix && ignore_suffix && split_subpatterns.size() == 2) return true;
			if (ignore_prefix) split_subpatterns.erase(split_subpatterns.begin());
			if (ignore_suffix) split_subpatterns.erase(split_subpatterns.end() - 1);


			// 一个 pattern 在拆分后的结构大概是 
			// (*) subpattern_1 subpattern_2 .. subpattern_n (*)
			// 判断是否匹配的核心思路是:
			// 1. subpattern_1 .. subpattern_n 找的到互不重叠匹配项，且顺序与subpattern的顺序相同
			// 2. left * (optional): 若为false, prefix是否满足subpattern_1
			// 3. right * (optional): 若为false, suffix是否满足subpattern_n
			// 但是对于只有一个subpattern, 且没有前导和后导*的情况下, subpattern_1和subpattern_n本身就是一个元素，
			// 不可能说一个subpattern同时存在于两个位置的匹配，因此只能同时满足prefix和suffix中的一个，
			// 必须保证prefix和suffix完全重合，即 subpattern[0].length() == text.length()
			if (!ignore_prefix)
			{
				const auto front_len = split_subpatterns.front().length();
				if (front_len > text.length() || !match_subpattern(text.substr(0, front_len), split_subpatterns.front()))
					return false;
			}
			if (!ignore_suffix)
			{
				if (!ignore_prefix && split_subpatterns.size() == 1) { return split_subpatterns.back().length() == text.length(); }
				const auto back_len = split_subpatterns.back().length();
				if (back_len > text.length() || !match_subpattern(text.substr(text.length() - back_len, back_len), split_subpatterns.back()))
					return false;
			}

			StringViewType text_remaining_part(text);
			for (auto subpattern_iter = split_subpatterns.begin(); subpattern_iter != split_subpatterns.end(); subpattern_iter++)
			{
				StringViewType subpattern(*subpattern_iter);
				if (text_remaining_part.length() < subpattern.length())
					return false;
				bool matched_one = false;
				size_t first_matched_offset;
				for (size_t offset = 0; offset < text_remaining_part.length() - subpattern.length() + 1; offset++)
				{
					if (match_subpattern(text_remaining_part.substr(offset, subpattern.length()), subpattern))
					{
						matched_one = true;
						first_matched_offset = offset;
						break;
					}
				}
				if (!matched_one) { return false; }
				else
				{
					text_remaining_part = text_remaining_part.substr(first_matched_offset + subpattern.length());
				}
			}

			return true;
		}
	}
}
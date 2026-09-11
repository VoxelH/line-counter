#include "fs.hpp"
#include "str.hpp"
#include <windows.h>
#include <stack>
#include <array>
#include <memory>
#include <set>

namespace vlc
{
	std::filesystem::path getcwd()
	{
		auto required_size = GetCurrentDirectoryW(0, nullptr);
		if (required_size == 0) throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "Failed to get current working directory.");
		std::wstring result;
		result.resize(required_size);
		auto written = GetCurrentDirectoryW(required_size, result.data());
		if (written == 0) throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "Failed to get current working directory.");
		result.resize(written);
		return result;
	}
	std::filesystem::path getfullpath(const std::filesystem::path& path)
	{
		auto required_size = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
		if (required_size == 0) throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "Failed to get full path.");
		std::wstring result;
		result.resize(required_size);
		auto written = GetFullPathNameW(path.c_str(), required_size, result.data(), nullptr);
		if (written == 0) throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "Failed to get full path.");
		result.resize(written);
		return result;
	}
	std::filesystem::path getvcwd(std::optional<wchar_t> volume_name)
	{
		if (!volume_name) return getcwd();
		std::wstring fmt_path;
		fmt_path += volume_name.value();
		fmt_path += L':';
		return getfullpath(fmt_path);
	}

	class Win32FindWrapper
	{
	private:
		UniqueHandle<FindCloseDeleter> _find_handle;
		bool _available;
		std::filesystem::path _directory_path;
		DirectoryEntry _entry;
	public:
		Win32FindWrapper(const std::filesystem::path& directory_path)
		{
			this->_directory_path = directory_path;
			HANDLE handle = FindFirstFileW((directory_path / L"*").c_str(), &_entry.find_data);
			if (handle == INVALID_HANDLE_VALUE)
			{
				_available = false;
				DWORD last_error = GetLastError();
				if (last_error != ERROR_FILE_NOT_FOUND)
					throw std::system_error(last_error, std::system_category(), "Failed to initialize Win32FindWrapper: invalid handle value returned by FindFirstFileW()");
			}
			else
			{
				_available = true;
				_find_handle.reset(handle);
				_entry.path = directory_path / _entry.find_data.cFileName;
			}
		}
		Win32FindWrapper(Win32FindWrapper&& move_src) noexcept
		{
			_find_handle = std::move(move_src._find_handle);
			_available = move_src._available;
			_directory_path = std::move(move_src._directory_path);
			_entry = std::move(move_src._entry);

			move_src._available = false;
		}
		Win32FindWrapper& operator =(Win32FindWrapper&& move_src) noexcept
		{
			if (this != &move_src)
			{
				_find_handle = std::move(move_src._find_handle);
				_available = move_src._available;
				_directory_path = std::move(move_src._directory_path);
				_entry = std::move(move_src._entry);

				move_src._available = false;
			}

			return *this;
		}
		Win32FindWrapper(const Win32FindWrapper& copy_src) = delete;
		Win32FindWrapper& operator =(const Win32FindWrapper& copy_src) = delete;

		const DirectoryEntry& current() const
		{
			if (!available())
				throw std::out_of_range("Already at the end");
			return _entry;
		}
		void next()
		{
			if (!available())
				throw std::out_of_range("Already at the end");
			if (false == FindNextFileW(_find_handle.get(), &_entry.find_data))
			{
				_available = false;
				if (GetLastError() != ERROR_NO_MORE_FILES)
					throw std::system_error(GetLastError(), std::system_category(), "Failed to advance Win32FindWrapper: unexpected error occurred calling FindNextFileW()");
			}
			else
			{
				_entry.path = _directory_path / _entry.find_data.cFileName;
			}
		}
		bool available() const noexcept { return _available; }
		std::filesystem::path get_directory_path() const { return _directory_path; }
	};

	DirectoryIterator::DirectoryIterator(const std::filesystem::path& path, bool _skip_dots)
		: _shared_find(std::make_shared<Win32FindWrapper>(path)), _skip_dots(_skip_dots), _is_end_sentinel(false)
	{
		if (!_skip_dots) return;
		while (_shared_find->available() && (std::wcscmp(_shared_find->current().find_data.cFileName, L".") == 0 || std::wcscmp(_shared_find->current().find_data.cFileName, L"..") == 0))
		{
			_shared_find->next();
		}
	}
	DirectoryIterator::DirectoryIterator(const DirectoryIterator& other) : _shared_find(other._shared_find), _skip_dots(other._skip_dots), _is_end_sentinel(other._is_end_sentinel) {}
	DirectoryIterator::DirectoryIterator(DirectoryIterator&& other) : _shared_find(std::move(other._shared_find)), _skip_dots(other._skip_dots), _is_end_sentinel(other._is_end_sentinel) {}
	DirectoryIterator& DirectoryIterator::operator = (const DirectoryIterator& other) {
		_shared_find = other._shared_find;
		_skip_dots = other._skip_dots;
		_is_end_sentinel = other._is_end_sentinel;
		return *this;
	}
	DirectoryIterator& DirectoryIterator::operator = (DirectoryIterator&& other)
	{
		if (this == &other) return *this;
		_shared_find = std::move(other._shared_find);
		_skip_dots = other._skip_dots;
		_is_end_sentinel = other._is_end_sentinel;
		return *this;
	}
	DirectoryIterator::~DirectoryIterator() noexcept {}

	DirectoryIterator::reference DirectoryIterator::operator*() const
	{
		if (_is_end_sentinel)
			throw std::runtime_error("Iterator is end sentinel");
		return _shared_find->current();
	}
	DirectoryIterator::pointer DirectoryIterator::operator->() const
	{
		if (_is_end_sentinel)
			throw std::runtime_error("Iterator is end sentinel");
		return &_shared_find->current();
	}
	DirectoryIterator& DirectoryIterator::operator++()
	{
		if (_is_end_sentinel)
			throw std::runtime_error("Iterator is end sentinel");
		do
		{
			_shared_find->next();
		} while (_skip_dots && _shared_find->available() && (std::wcscmp(_shared_find->current().find_data.cFileName, L".") == 0 || std::wcscmp(_shared_find->current().find_data.cFileName, L"..") == 0));
		return *this;
	}
	DirectoryIterator DirectoryIterator::operator++(int)
	{
		// Actually useless
		if (_is_end_sentinel)
			throw std::runtime_error("Iterator is end sentinel");
		DirectoryIterator temp = *this;
		operator++();
		return temp;
	}
	bool operator ==(const DirectoryIterator& a, const DirectoryIterator& b) noexcept
	{
		if (a._shared_find.get() != b._shared_find.get()) return false;

		bool a_is_at_end = !a._shared_find->available() || a._is_end_sentinel;
		bool b_is_at_end = !b._shared_find->available() || b._is_end_sentinel;
		if (!a_is_at_end && !b_is_at_end)
			return std::wcscmp(a._shared_find->current().find_data.cFileName, b._shared_find->current().find_data.cFileName) == 0;
		else
			return a_is_at_end && b_is_at_end;
	}
	bool operator !=(const DirectoryIterator& a, const DirectoryIterator& b) noexcept
	{
		return !(a == b);
	}

	DirectoryIterator begin(const DirectoryIterator& a) noexcept
	{
		return a;
	}
	DirectoryIterator end(const DirectoryIterator& a) noexcept
	{
		DirectoryIterator b = a;
		b._is_end_sentinel = true;
		return b;
	}

	static constexpr int SYMLINK_FLAG_RELATIVE = 1;

	typedef struct _REPARSE_DATA_BUFFER {
		ULONG  ReparseTag;
		USHORT ReparseDataLength;
		USHORT Reserved;
		union {
			struct {
				USHORT SubstituteNameOffset;
				USHORT SubstituteNameLength;
				USHORT PrintNameOffset;
				USHORT PrintNameLength;
				ULONG  Flags;
				WCHAR  PathBuffer[1];
			} SymbolicLinkReparseBuffer;
			struct {
				USHORT SubstituteNameOffset;
				USHORT SubstituteNameLength;
				USHORT PrintNameOffset;
				USHORT PrintNameLength;
				WCHAR  PathBuffer[1];
			} MountPointReparseBuffer;
			struct {
				UCHAR DataBuffer[1];
			} GenericReparseBuffer;
		} DUMMYUNIONNAME;
	} REPARSE_DATA_BUFFER, * PREPARSE_DATA_BUFFER;

	DirectoryType get_directory_type(HANDLE file_handle)
	{
		BY_HANDLE_FILE_INFORMATION file_information;
		if (false == GetFileInformationByHandle(file_handle, &file_information))
			throw std::system_error(GetLastError(), std::system_category(), "Failed to get file information");

		if (false == (file_information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			return DirectoryType::NOT_A_DIRECTORY;

		if (false == (file_information.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
			return DirectoryType::REGULAR_DIRECTORY;

		auto buffer = std::make_unique<std::array<unsigned char, MAXIMUM_REPARSE_DATA_BUFFER_SIZE>>();
		PREPARSE_DATA_BUFFER reparse_data_buffer = reinterpret_cast<PREPARSE_DATA_BUFFER>(buffer->data());
		DWORD junk;
		bool dic_result = DeviceIoControl(file_handle, FSCTL_GET_REPARSE_POINT, nullptr, 0, buffer->data(), MAXIMUM_REPARSE_DATA_BUFFER_SIZE, &junk, nullptr);
		if (!dic_result)
			throw std::system_error(GetLastError(), std::system_category(), "Failed to get file information");

		switch (reparse_data_buffer->ReparseTag)
		{
		case IO_REPARSE_TAG_SYMLINK:
			return DirectoryType::SYMLINK_DIRECTORY;
		case IO_REPARSE_TAG_MOUNT_POINT:
			return DirectoryType::JUNCTION_OR_MOUNTPOINT;
		default:
			return DirectoryType::UNKNOWN;
		}
	}

	int compare(const RegularPath& lhs, const RegularPath& rhs)
	{
		// volume
		if (lhs.volume.has_value() != rhs.volume.has_value())
			return lhs.volume.has_value() ? 1 : -1;
		if (lhs.volume.has_value() && lhs.volume.value() != rhs.volume.value())
			return str::to_upper(lhs.volume.value()) - str::to_upper(rhs.volume.value());

		// root_dir
		if (lhs.has_root_dir ^ rhs.has_root_dir)
			return lhs.has_root_dir ? 1 : -1;
#undef min  // FUCK windows.h
		// segments
		for (size_t i = 0; i < std::min(lhs.segments.size(), rhs.segments.size()); i++)
		{
			if (lhs.segments[i].compare(rhs.segments[i]) != 0)
				return lhs.segments[i].compare(rhs.segments[i]);
		}
		auto l_size = lhs.segments.size();
		auto r_size = rhs.segments.size();
		return l_size == r_size ? 0 : l_size < r_size ? -1 : 1;
	}
	bool operator > (const RegularPath& lhs, const RegularPath& rhs) { return compare(lhs, rhs) > 0; }
	bool operator < (const RegularPath& lhs, const RegularPath& rhs) { return compare(lhs, rhs) < 0; }
	bool operator >= (const RegularPath& lhs, const RegularPath& rhs) { return compare(lhs, rhs) >= 0; }
	bool operator <= (const RegularPath& lhs, const RegularPath& rhs) { return compare(lhs, rhs) <= 0; }
	bool operator == (const RegularPath& lhs, const RegularPath& rhs) { return compare(lhs, rhs) == 0; }
	bool operator != (const RegularPath& lhs, const RegularPath& rhs) { return compare(lhs, rhs) != 0; }

	RegularPath::RegularPath(std::optional<wchar_t> volume, bool has_root_dir, const std::vector<std::wstring>& segments) : volume(volume), has_root_dir(has_root_dir), segments(segments) {}
	RegularPath RegularPath::from_str(std::wstring_view str)
	{
		if (str.empty())
			throw std::invalid_argument("Empty path string is invalid");

		std::wstring path_slash_unified = vlc::str::replace(str, L'/', L'\\');
		if (vlc::str::starts_with(std::wstring_view(str), std::wstring_view(L"\\\\")) && !str::starts_with(std::wstring_view(str), std::wstring_view(L"\\\\\\")))
			throw std::invalid_argument("Unsupported path type (possibly is NT Path, Win32 Path, or NT Path)");


		std::wstring path_removed_duplicate_slashes;
		for (wchar_t ch : path_slash_unified)
			if (ch != L'\\' || !vlc::str::ends_with(std::wstring_view(path_removed_duplicate_slashes), std::wstring_view(L"\\")))
				path_removed_duplicate_slashes += ch;

		std::optional<wchar_t> volume;
		bool has_root_dir;
		std::vector<std::wstring> segments;

		std::wstring_view remaining_path_string(path_removed_duplicate_slashes);
		if (remaining_path_string.length() >= 2 && remaining_path_string[1] == L':')
		{
			volume = remaining_path_string[0];
			remaining_path_string = remaining_path_string.substr(2);
		}
		else { volume = std::nullopt; }

		if (vlc::str::starts_with(std::wstring_view(remaining_path_string), std::wstring_view(L"\\")))
		{
			has_root_dir = true;
			remaining_path_string = remaining_path_string.substr(1);
		}
		else { has_root_dir = false; }

		segments = str::split(remaining_path_string, L'\\');
		return RegularPath(volume, has_root_dir, segments);
	}
	RegularPath RegularPath::from_fspath(const std::filesystem::path& fspath)
	{
		return from_str(fspath.native());
	}
	std::wstring RegularPath::to_str() const
	{
		std::wstring result;
		if (volume)
		{
			result += volume.value();
			result += L':';
		}
		if (has_root_dir)
			result += L'\\';
		result += str::join(segments.begin(), segments.end(), L'\\');
		return result;
	}
	std::filesystem::path RegularPath::to_fspath() const
	{
		return to_str();
	}

	RegularPath RegularPath::absolute() const
	{
		if (volume && has_root_dir) return *this;
		else if (!has_root_dir)
		{
			RegularPath cwd = RegularPath::from_fspath(getvcwd(volume));
			RegularPath result;
			result.volume = cwd.volume;
			result.has_root_dir = true;
			std::copy(cwd.segments.begin(), cwd.segments.end(), std::back_inserter(result.segments));
			std::copy(segments.begin(), segments.end(), std::back_inserter(result.segments));
			return result;
		}
		else
		{
			RegularPath result;
			result.volume = getcwd().c_str()[0];
			result.has_root_dir = true;
			result.segments = segments;
			return result;
		}
	}
	RegularPath RegularPath::normalized() const
	{
		RegularPath result = absolute();
		std::vector<std::wstring> simplifiedsegments;
		for (std::wstring& seg : result.segments)
		{
			seg = std::wstring(seg.data());
			if (!seg.empty() && seg != L"." && (seg != L"**" || simplifiedsegments.empty() || simplifiedsegments.back() != L"**"))
			{
				simplifiedsegments.push_back(seg);
			}
		}
		result.segments = simplifiedsegments;

		return result;
	}

	bool match_lexically(const RegularPath& pattern, const RegularPath& path, bool segments_case_sensitive)
	{
		using CharType = wchar_t;
		using StringType = std::wstring;
		using StringViewType = std::wstring_view;

		RegularPath normalized_path = path.normalized();
		auto _pattern = pattern;
		if (pattern.volume.has_value() && (pattern.volume.value() == L'?' || pattern.volume.value() == L'*'))
			_pattern.volume = normalized_path.volume;
		RegularPath normalized_pattern = _pattern.normalized();

		normalized_path.volume = str::to_upper(normalized_path.volume.value());
		normalized_pattern.volume = str::to_upper(normalized_pattern.volume.value());

		if (normalized_pattern.volume.value() != normalized_path.volume.value()) return false;

		auto& pathsegments = normalized_path.segments;
		auto& patternsegments = normalized_pattern.segments;

		if (!segments_case_sensitive)
		{
			for (auto iter = pathsegments.begin(); iter != pathsegments.end(); iter++)
				*iter = str::to_upper(StringViewType(*iter));
			for (auto iter = patternsegments.begin(); iter != patternsegments.end(); iter++)
				*iter = str::to_upper(StringViewType(*iter));
		}

		pathsegments.insert(pathsegments.begin(), StringType());
		using ConstIterType = decltype(normalized_pattern.segments)::const_iterator;
		std::set<ConstIterType> currently_collected;
		std::set<ConstIterType> previously_collected = { pathsegments.begin() };

		for (auto&& subpattern : patternsegments)
		{
			for (auto&& previous_item : previously_collected)
			{
				if (subpattern == L"..")
				{
					if (previous_item == pathsegments.begin())  // root_dir
						currently_collected.insert(previous_item);
					else
						currently_collected.insert(previous_item - 1);
				}
				else if (subpattern == L"**")
				{
					for (auto iter = previous_item; iter != pathsegments.end(); iter++)
						currently_collected.insert(iter);
				}
				else
				{
					if (previous_item != pathsegments.end() - 1 && str::match(StringViewType(*(previous_item + 1)), StringViewType(subpattern)))
						currently_collected.insert(previous_item + 1);
				}
			}
			std::swap(currently_collected, previously_collected);
			currently_collected.clear();
		}

		return previously_collected.find(pathsegments.end() - 1) != previously_collected.end();
	}

	void walk_dir(
		const std::filesystem::path& directory_path, WalkOptions options,
		std::function<bool(const std::filesystem::path& item_path)> ok,
		std::function<bool(const std::filesystem::path& directory_path, const std::exception_ptr& exception)> unable_to_enter_directory,
		std::function<bool(const std::filesystem::path& directory_path, const std::exception_ptr& exception)> unable_to_enumerate_directory
	)
	{
		bool option_recursive = uint64_t(options) & uint64_t(WalkOptions::RECURSIVE);
		bool option_follow_symlink = uint64_t(options) & uint64_t(WalkOptions::FOLLOW_SYMLINK);
		bool option_follow_junction_and_mountpoint = uint64_t(options) & uint64_t(WalkOptions::FOLLOW_JUNCTION_AND_MOUNTPOINT);
		bool option_call_ok_on_enter_the_starting_directory = uint64_t(options) & uint64_t(WalkOptions::CALL_OK_ON_ENTER_THE_STARTING_DIRECTORY);

		struct FrameInfo
		{
			const std::filesystem::path& directory_path;
			DirectoryIterator directory_iterator;
			FrameInfo(const std::filesystem::path& directory_path, const DirectoryIterator& directory_iterator) : directory_path(directory_path), directory_iterator(directory_iterator) {}
		};

		std::stack<FrameInfo> frames;

		try
		{
			frames.emplace(directory_path, DirectoryIterator(directory_path));
			if (option_call_ok_on_enter_the_starting_directory && !ok(directory_path))
				return;
		}
		catch (...)
		{
			unable_to_enter_directory(directory_path, std::current_exception());
			return;
		}

		while (!frames.empty())
		{
			FrameInfo& current_frame = frames.top();
			std::filesystem::path directory_path_backup = current_frame.directory_path;
			try
			{
				while (current_frame.directory_iterator != end(current_frame.directory_iterator))
				{
					const DirectoryEntry& directory_entry = *current_frame.directory_iterator;
					if (!ok(directory_entry.path))
						return;
					std::filesystem::path path_backup = directory_entry.path;
					DWORD file_attributes_backup = directory_entry.find_data.dwFileAttributes;
					++current_frame.directory_iterator;

					try
					{
						if (option_recursive && (file_attributes_backup & FILE_ATTRIBUTE_DIRECTORY))
						{
							UniqueHandle<CommonHandleDeleter> file_handle(CreateFileW(path_backup.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
							if (file_handle.get() == INVALID_HANDLE_VALUE)
								throw std::system_error(GetLastError(), std::system_category(), "Failed to create file object to directory");

							DirectoryType dir_type = get_directory_type(file_handle.get());
							bool is_regular_directory = dir_type == DirectoryType::REGULAR_DIRECTORY;
							bool is_symlink_directory = dir_type == DirectoryType::SYMLINK_DIRECTORY;
							bool is_junction_or_mountpoint = dir_type == DirectoryType::JUNCTION_OR_MOUNTPOINT;
							if (!(is_regular_directory || is_symlink_directory || is_junction_or_mountpoint))
								throw std::runtime_error("Unsupported directory type");
							if (is_regular_directory || (option_follow_symlink && is_symlink_directory) || (option_follow_junction_and_mountpoint && is_junction_or_mountpoint))
							{
								frames.emplace(path_backup, DirectoryIterator(path_backup));
								goto SWITCH_FRAMES;
							}
						}
					}
					catch (...)
					{
						if (!unable_to_enter_directory(path_backup, std::current_exception()))
							return;
					}
				}
			}
			catch (...)
			{
				if (!unable_to_enumerate_directory(directory_path_backup, std::current_exception()))
					return;
			}
			frames.pop();
		SWITCH_FRAMES:
			{}
		}
	}

	void walk_dir_match(
		const RegularPath& pattern, bool follow_symlink, bool follow_junction_or_mountpoint, bool time_over_space, bool case_sensitive,
		std::function<void(const RegularPath& entry)> on_match,
		std::function<void(const std::filesystem::path& directory_path, const std::exception_ptr& exception)> unable_to_enter_directory,
		std::function<void(const std::filesystem::path& directory_path, const std::exception_ptr& exception)> unable_to_enumerate_directory
	)
	{
		std::vector<RegularPath> normalized_pattern;
		if (pattern.volume.has_value() && (pattern.volume == L'*' || pattern.volume == L'?'))
		{
			wchar_t volume_path[4] = { 0, L':', L'\\', 0 };
			for (wchar_t volume_char = L'A'; volume_char != L'Z' + 1; volume_char++)
			{
				volume_path[0] = volume_char;
				if (std::filesystem::exists(volume_path))
				{
					RegularPath ptrn = pattern;
					ptrn.volume = volume_char;
					normalized_pattern.push_back(ptrn.normalized());
				}
			}
		}
		else
		{
			normalized_pattern.push_back(pattern.normalized());
		}

		for (auto&& current_pattern : normalized_pattern)
		{
			std::set<RegularPath> currently_matched;
			std::set<RegularPath> previously_matched;

			wchar_t volume_path[] = { current_pattern.volume.value() , L':', L'\\', 0 };
			if (std::filesystem::exists(volume_path))
				previously_matched.emplace(current_pattern.volume.value(), true);

			auto ok_fn_regular = [&](const std::filesystem::path& p) { currently_matched.insert(RegularPath::from_fspath(p)); return true; };
			auto ok_fn_on_match = [&](const std::filesystem::path& p) { on_match(RegularPath::from_fspath(p)); return true; };

			for (auto iter = current_pattern.segments.begin(); iter != current_pattern.segments.end(); ++iter)
			{
				auto&& ptrn_seg = *iter;
				for (auto&& prev_item : previously_matched)
				{
					if (ptrn_seg == L"..")
					{
						RegularPath p = prev_item;
						if (p.segments.size() != 0)
							p.segments.erase(p.segments.end() - 1);

						if (iter != current_pattern.segments.end() - 1)
							currently_matched.emplace(std::move(p));
						else
							on_match(p);

						continue;
					}

					try
					{
						UniqueHandle<CommonHandleDeleter> hFile{ CreateFileW(prev_item.to_str().c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr) };
						if (hFile.get() == INVALID_HANDLE_VALUE)
							throw std::system_error(GetLastError(), std::system_category(), "Failed to create file object");
						DirectoryType dir_type = get_directory_type(hFile.get());
						bool should_search = dir_type == DirectoryType::REGULAR_DIRECTORY || (follow_symlink && dir_type == DirectoryType::SYMLINK_DIRECTORY) || (follow_junction_or_mountpoint && dir_type == DirectoryType::JUNCTION_OR_MOUNTPOINT);
						if (!should_search)
							continue;
					}
					catch (...)
					{
						unable_to_enter_directory(prev_item.to_fspath(), std::current_exception());
						continue;
					}

					if (ptrn_seg == L"**")
					{
						if (time_over_space)
						{
							WalkOptions walk_options = WalkOptions::RECURSIVE | WalkOptions::CALL_OK_ON_ENTER_THE_STARTING_DIRECTORY;
							if (follow_symlink) walk_options = walk_options | WalkOptions::FOLLOW_SYMLINK;
							if (follow_junction_or_mountpoint) walk_options = walk_options | WalkOptions::FOLLOW_JUNCTION_AND_MOUNTPOINT;


							if (iter != current_pattern.segments.end() - 1)
								walk_dir(prev_item.to_fspath(), walk_options,
									ok_fn_regular,
									[&](const std::filesystem::path& directory_path, const std::exception_ptr& exception) { unable_to_enter_directory(directory_path, exception); return true; },
									[&](const std::filesystem::path& directory_path, const std::exception_ptr& exception) { unable_to_enumerate_directory(directory_path, exception); return true; }
								);
							else
								walk_dir(prev_item.to_fspath(), walk_options,
									ok_fn_on_match,
									[&](const std::filesystem::path& directory_path, const std::exception_ptr& exception) { unable_to_enter_directory(directory_path, exception); return true; },
									[&](const std::filesystem::path& directory_path, const std::exception_ptr& exception) { unable_to_enumerate_directory(directory_path, exception); return true; }
								);
						}

						else
						{
							throw std::invalid_argument("space-over-time not supported yet");
						}
					}
					else
					{
						walk_dir(
							prev_item.to_fspath(), WalkOptions::DEFAULT,
							[&](const std::filesystem::path& p)
							{
								const std::wstring& last_seg = case_sensitive ? p.filename().c_str() : str::to_upper(std::wstring_view(p.filename().c_str()));
								const std::wstring& ptrn_last_seg = case_sensitive ? ptrn_seg : str::to_upper(std::wstring_view(ptrn_seg));
								if (str::match(std::wstring_view(last_seg), std::wstring_view(ptrn_last_seg)))
								{
									if (iter != current_pattern.segments.end() - 1)
										currently_matched.insert(RegularPath::from_fspath(p));
									else
										on_match(RegularPath::from_fspath(p));
								}
								return true;
							},
							[&](const std::filesystem::path& directory_path, const std::exception_ptr& exception) { unable_to_enter_directory(directory_path, exception); return true; },
							[&](const std::filesystem::path& directory_path, const std::exception_ptr& exception) { unable_to_enumerate_directory(directory_path, exception); return true; }
						);
					}
				}

				std::swap(currently_matched, previously_matched);
				currently_matched.clear();
			}
		}
	}
}
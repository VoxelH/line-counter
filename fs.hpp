#pragma once
#include <filesystem>
#include <memory>
#include <windows.h>
#include <fileapi.h>
#include <handleapi.h>
#include <functional>
#include <optional>
#include <set>

namespace vlc
{
	std::filesystem::path getcwd();
	std::filesystem::path getfullpath(const std::filesystem::path& path);
	std::filesystem::path getvcwd(std::optional<wchar_t> volume_name);

	struct CommonHandleDeleter
	{
		void operator()(HANDLE handle)
		{
			CloseHandle(handle);
		}
	};
	struct FindCloseDeleter
	{
		void operator()(HANDLE handle)
		{
			FindClose(handle);
		}
	};

	template <class CloserType>
	class UniqueHandle {
	private:
		HANDLE handle;
		CloserType closer_func;
		void close_current_handle()
		{
			if (handle != INVALID_HANDLE_VALUE && handle != nullptr)
				closer_func(handle);
		}
	public:
		explicit UniqueHandle(HANDLE handle = INVALID_HANDLE_VALUE) : handle(handle), closer_func(CloserType()) {}
		UniqueHandle(HANDLE handle, const CloserType& copyable_closer_func) : handle(handle), closer_func(copyable_closer_func) {}
		UniqueHandle(HANDLE handle, CloserType&& movable_closer_func) : handle(handle), closer_func(std::move(movable_closer_func)) {}
		UniqueHandle(const UniqueHandle& other) = delete;
		UniqueHandle& operator = (const UniqueHandle& other) = delete;
		UniqueHandle(UniqueHandle&& other)
		{
			this->handle = other.handle;
			other.handle = INVALID_HANDLE_VALUE;
			this->closer_func = std::move(other.closer_func);
		}
		UniqueHandle& operator = (UniqueHandle&& other)
		{
			if (this != &other)
			{
				close_current_handle();
				this->handle = other.handle;
				other.handle = INVALID_HANDLE_VALUE;

				this->closer_func = std::move(other.closer_func);
			}
			return *this;

		}
		~UniqueHandle()
		{
			close_current_handle();
		}

		HANDLE get() const { return handle; }
		HANDLE release()
		{
			HANDLE temp = handle;
			handle = INVALID_HANDLE_VALUE;
			return temp;
		}
		void reset(HANDLE handle)
		{
			if (this->handle == handle) return;
			close_current_handle();
			this->handle = handle;
		}
	};

	struct DirectoryEntry
	{
		std::filesystem::path path;
		WIN32_FIND_DATAW find_data;
	};

	class Win32FindWrapper;
	class DirectoryIterator
	{
	private:
		std::shared_ptr<Win32FindWrapper> _shared_find;
		bool _skip_dots;
		bool _is_end_sentinel;

	public:
		using value_type = DirectoryEntry;
		using difference_type = std::ptrdiff_t;
		using reference = const value_type&;
		using pointer = const value_type*;
		using iterator_category = std::input_iterator_tag;

		DirectoryIterator(const std::filesystem::path& path, bool _skip_dots = true);
		DirectoryIterator(const DirectoryIterator& other);
		DirectoryIterator(DirectoryIterator&& other);
		DirectoryIterator& operator = (const DirectoryIterator& other);
		DirectoryIterator& operator = (DirectoryIterator&& other);
		~DirectoryIterator() noexcept;

		reference operator*() const;
		pointer operator->() const;
		DirectoryIterator& operator++();
		DirectoryIterator operator++(int);

		friend bool operator ==(const DirectoryIterator& a, const DirectoryIterator& b) noexcept;
		friend bool operator !=(const DirectoryIterator& a, const DirectoryIterator& b) noexcept;
		friend DirectoryIterator begin(const DirectoryIterator& a) noexcept;
		friend DirectoryIterator end(const DirectoryIterator& a) noexcept;
	};

	enum class DirectoryType
	{
		REGULAR_DIRECTORY,
		SYMLINK_DIRECTORY,
		JUNCTION_OR_MOUNTPOINT,
		UNKNOWN,
		NOT_A_DIRECTORY
	};

	DirectoryType get_directory_type(HANDLE file_handle);

	enum class WalkOptions: uint64_t
	{
		DEFAULT = 0x0,
		RECURSIVE = 0x1,
		FOLLOW_SYMLINK = 0x2,
		FOLLOW_JUNCTION_AND_MOUNTPOINT = 0x4,
		CALL_OK_ON_ENTER_THE_STARTING_DIRECTORY = 0x8
	};
	inline WalkOptions operator |(WalkOptions a, WalkOptions b) 
	{
		return WalkOptions((uint64_t)a | (uint64_t)b);
	}

	class RegularPath
	{
	public:
		std::optional<wchar_t> volume;
		bool has_root_dir;
		std::vector<std::wstring> segments;

		RegularPath(std::optional<wchar_t> volume = std::nullopt, bool has_root_dir = false, const std::vector<std::wstring> & segments = {});
		static RegularPath from_str(std::wstring_view str);
		static RegularPath from_fspath(const std::filesystem::path& fspath);
		std::wstring to_str() const;
		std::filesystem::path to_fspath() const;

		friend int compare(const RegularPath& lhs, const RegularPath& rhs);
		friend bool operator > (const RegularPath& lhs, const RegularPath& rhs);
		friend bool operator < (const RegularPath& lhs, const RegularPath& rhs);
		friend bool operator >= (const RegularPath& lhs, const RegularPath& rhs);
		friend bool operator <= (const RegularPath& lhs, const RegularPath& rhs);
		friend bool operator == (const RegularPath& lhs, const RegularPath& rhs);
		friend bool operator != (const RegularPath& lhs, const RegularPath& rhs);

		RegularPath absolute() const;
		RegularPath normalized() const;
	};

	bool match_lexically(const RegularPath & pattern, const RegularPath & path, bool segments_case_sensitive = false);

	void walk_dir(
		const std::filesystem::path & directory_path, WalkOptions options,
		std::function<bool(const std::filesystem::path& item_path)> ok,
		std::function<bool(const std::filesystem::path& directory_path, const std::exception_ptr & exception)> unable_to_enter_directory,
		std::function<bool(const std::filesystem::path& directory_path, const std::exception_ptr & exception)> unable_to_enumerate_directory
		);

	void walk_dir_match(
		const RegularPath& pattern, bool follow_symlink, bool follow_junction_or_mountpoint, bool time_over_space, bool case_sensitive,
		std::function<void(const RegularPath& entry)> on_match,
		std::function<void(const std::filesystem::path& directory_path, const std::exception_ptr& exception)> unable_to_enter_directory,
		std::function<void(const std::filesystem::path& directory_path, const std::exception_ptr& exception)> unable_to_enumerate_directory
	);
}
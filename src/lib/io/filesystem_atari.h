/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 */

#pragma once

#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace inputleap {
namespace atari_fs {

class path {
public:
    path() {}

    path(const char* text) : value_(text != nullptr ? text : "") {}
    path(const std::string& text) : value_(text) {}

    bool empty() const
    {
        return value_.empty();
    }

    const std::string& native() const
    {
        return value_;
    }

    std::string u8string() const
    {
        return value_;
    }

    path filename() const
    {
        const std::string trimmed = trim_trailing_separators(value_);
        if (trimmed.empty()) {
            return {};
        }

        const std::string::size_type pos = trimmed.find_last_of('/');
        if (pos == std::string::npos) {
            return path(trimmed);
        }
        if (pos == 0 && trimmed.size() == 1) {
            return path("/");
        }
        return path(trimmed.substr(pos + 1));
    }

    path parent_path() const
    {
        const std::string trimmed = trim_trailing_separators(value_);
        if (trimmed.empty()) {
            return {};
        }

        const std::string::size_type pos = trimmed.find_last_of('/');
        if (pos == std::string::npos) {
            return {};
        }
        if (pos == 0) {
            return path("/");
        }
        return path(trimmed.substr(0, pos));
    }

    path& operator/=(const path& other)
    {
        value_ = join(value_, other.value_);
        return *this;
    }

    friend bool operator==(const path& lhs, const path& rhs)
    {
        return lhs.value_ == rhs.value_;
    }

    friend bool operator!=(const path& lhs, const path& rhs)
    {
        return !(lhs == rhs);
    }

    friend bool operator<(const path& lhs, const path& rhs)
    {
        return lhs.value_ < rhs.value_;
    }

private:
    static std::string trim_trailing_separators(const std::string& input)
    {
        if (input.empty()) {
            return input;
        }

        std::string value = input;
        while (value.size() > 1 && value[value.size() - 1] == '/') {
            value.erase(value.size() - 1);
        }
        return value;
    }

    static std::string join(const std::string& lhs, const std::string& rhs)
    {
        if (lhs.empty()) {
            return rhs;
        }
        if (rhs.empty()) {
            return lhs;
        }
        if (rhs[0] == '/') {
            return rhs;
        }
        if (lhs[lhs.size() - 1] == '/') {
            return lhs + rhs;
        }
        return lhs + "/" + rhs;
    }

    std::string value_;
};

inline path operator/(const path& lhs, const path& rhs)
{
    path value(lhs);
    value /= rhs;
    return value;
}

inline path u8path(const char* text)
{
    return path(text);
}

inline path u8path(const std::string& text)
{
    return path(text);
}

enum class copy_options {
    none = 0,
    recursive = 1
};

inline bool stat_path(const path& value, struct stat& info)
{
    return ::stat(value.native().c_str(), &info) == 0;
}

inline bool exists(const path& value)
{
    struct stat info;
    return stat_path(value, info);
}

inline bool is_regular_file(const path& value)
{
    struct stat info;
    return stat_path(value, info) && S_ISREG(info.st_mode);
}

inline bool is_directory(const path& value)
{
    struct stat info;
    return stat_path(value, info) && S_ISDIR(info.st_mode);
}

inline bool create_directories(const path& value)
{
    if (value.empty()) {
        return false;
    }
    if (exists(value)) {
        return false;
    }

    const path parent = value.parent_path();
    if (!parent.empty() && parent != value && !exists(parent)) {
        create_directories(parent);
    }

    if (::mkdir(value.native().c_str(), 0777) == 0) {
        return true;
    }

    if (errno == EEXIST) {
        return is_directory(value);
    }

    throw std::runtime_error(std::strerror(errno));
}

inline void copy_file(const path& from, const path& to)
{
    const path parent = to.parent_path();
    if (!parent.empty() && !exists(parent)) {
        create_directories(parent);
    }

    std::ifstream input(from.native().c_str(), std::ios::binary);
    if (!input) {
        throw std::runtime_error(std::strerror(errno));
    }

    std::ofstream output(to.native().c_str(), std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error(std::strerror(errno));
    }

    output << input.rdbuf();
    if (!output.good()) {
        throw std::runtime_error("failed to copy file contents");
    }
}

inline void copy(const path& from, const path& to, copy_options options = copy_options::none)
{
    struct stat info;
    if (!stat_path(from, info)) {
        throw std::runtime_error(std::strerror(errno));
    }

    if (S_ISDIR(info.st_mode)) {
        if (options != copy_options::recursive) {
            return;
        }

        create_directories(to);

        DIR* dir = ::opendir(from.native().c_str());
        if (dir == nullptr) {
            throw std::runtime_error(std::strerror(errno));
        }

        while (const dirent* entry = ::readdir(dir)) {
            const char* name = entry->d_name;
            if (std::strcmp(name, ".") == 0 || std::strcmp(name, "..") == 0) {
                continue;
            }

            copy(from / name, to / name, options);
        }

        ::closedir(dir);
        return;
    }

    copy_file(from, to);
}

inline void rename(const path& from, const path& to)
{
    const path parent = to.parent_path();
    if (!parent.empty() && !exists(parent)) {
        create_directories(parent);
    }

    if (::rename(from.native().c_str(), to.native().c_str()) != 0) {
        throw std::runtime_error(std::strerror(errno));
    }
}

} // namespace atari_fs
} // namespace inputleap

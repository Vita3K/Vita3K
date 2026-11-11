// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#include <pfsmgr/functions.h>

#include "savedata_transaction.h"

#include <algorithm>
#include <mutex>
#include <stdexcept>
#include <system_error>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace pfsmgr {
namespace fs = std::filesystem;

constexpr uint64_t max_file_size = 64 * 1024 * 1024;

class PfsLock {
public:
    explicit PfsLock(const fs::path &root) {
        const auto pfs = root / "sce_pfs";
        if (!fs::is_directory(fs::symlink_status(root)) || !fs::is_directory(fs::symlink_status(pfs)))
            throw std::runtime_error("PFS: invalid write lock directory");
        const auto path = pfs / ".vita3k-write-lock";
#ifdef _WIN32
        handle_ = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        if (handle_ == INVALID_HANDLE_VALUE)
            throw std::system_error(GetLastError(), std::system_category(), "PFS: cannot acquire write lock");
        BY_HANDLE_FILE_INFORMATION info{};
        if (!GetFileInformationByHandle(handle_, &info) || (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) {
            CloseHandle(handle_);
            handle_ = INVALID_HANDLE_VALUE;
            throw std::runtime_error("PFS: invalid write lock file");
        }

#else
        fd_ = open(path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600);
        if (fd_ < 0)
            throw std::system_error(errno, std::generic_category(), "PFS: cannot open write lock");
        struct stat info{};
        if (fstat(fd_, &info) != 0 || !S_ISREG(info.st_mode)) {
            close(fd_);
            fd_ = -1;
            throw std::runtime_error("PFS: invalid write lock file");
        }
        if (flock(fd_, LOCK_EX | LOCK_NB) != 0) {
            const int error = errno;
            close(fd_);
            fd_ = -1;
            throw std::system_error(error, std::generic_category(), "PFS: cannot acquire write lock");
        }
#endif
    }

    ~PfsLock() {
#ifdef _WIN32
        if (handle_ != INVALID_HANDLE_VALUE)
            CloseHandle(handle_);
#else
        if (fd_ >= 0)
            close(fd_);
#endif
    }

    PfsLock(const PfsLock &) = delete;
    PfsLock &operator=(const PfsLock &) = delete;

private:
#ifdef _WIN32
    HANDLE handle_ = INVALID_HANDLE_VALUE;
#else
    int fd_ = -1;
#endif
};

struct PfsSession::Impl {
    fs::path root;
    std::optional<AesKey> key;
    PfsLock lock;
    mutable std::mutex mutex;
    bool poisoned = false;

    Impl(const fs::path &path, const std::optional<AesKey> &pfs_key)
        : root(fs::absolute(path))
        , key(pfs_key)
        , lock(root) {
        recover_savedata_transaction(root, key);
        layer();
    }

    Layer layer() const {
        if (poisoned)
            throw std::runtime_error("PFS: failed session must be reopened for recovery");
        return key ? Layer(root, *key) : Layer(root);
    }

    template <typename Mutation>
    void mutate(Mutation &&mutation) {
        poisoned = true;
        try {
            mutation();
            poisoned = false;
        } catch (...) {
            std::error_code error;
            const auto status = fs::symlink_status(root / "sce_pfs" / ".vita3k-transaction", error);
            if ((!error || error == std::errc::no_such_file_or_directory) && !fs::exists(status))
                poisoned = false;
            throw;
        }
    }
};

PfsSession::PfsSession(const fs::path &root, const std::optional<AesKey> &key)
    : impl_(std::make_unique<Impl>(root, key)) {}

PfsSession::~PfsSession() = default;

bool PfsSession::is_usable() const {
    const std::lock_guard lock(impl_->mutex);
    return !impl_->poisoned;
}

std::optional<std::vector<uint8_t>> PfsSession::read(const fs::path &path) const {
    const std::lock_guard lock(impl_->mutex);
    return impl_->layer().read(path);
}

uint64_t PfsSession::write(const fs::path &path, uint64_t offset, std::span<const uint8_t> data, bool create, bool append,
    std::optional<uint64_t> truncate_size) {
    const std::lock_guard lock(impl_->mutex);
    const auto layer = impl_->layer();
    auto content = layer.read(path);
    if (!content && !create)
        throw std::runtime_error("PFS: file does not exist");
    if (!content)
        content.emplace();
    if (append)
        offset = content->size();
    if (offset > max_file_size || data.size() > max_file_size - offset || (truncate_size && *truncate_size > max_file_size))
        throw std::runtime_error("PFS: write exceeds file size limit");
    if (!data.empty()) {
        const auto end = static_cast<size_t>(offset + data.size());
        if (end > content->size())
            content->resize(end);
        std::copy(data.begin(), data.end(), content->begin() + static_cast<size_t>(offset));
    }
    if (truncate_size)
        content->resize(static_cast<size_t>(*truncate_size));
    impl_->mutate([&] { layer.replace_savedata_file(path, *content); });
    return offset + data.size();
}

void PfsSession::truncate(const fs::path &path, uint64_t size) {
    const std::lock_guard lock(impl_->mutex);
    if (size > max_file_size)
        throw std::runtime_error("PFS: truncation exceeds file size limit");
    const auto layer = impl_->layer();
    auto content = layer.read(path);
    if (!content)
        throw std::runtime_error("PFS: file does not exist");
    content->resize(static_cast<size_t>(size));
    impl_->mutate([&] { layer.replace_savedata_file(path, *content); });
}

void PfsSession::create_directories(const fs::path &path) {
    const std::lock_guard lock(impl_->mutex);
    if (path.empty())
        return;
    detail::validate_savedata_directory_path(path);
    fs::path current;
    for (const auto &part : path) {
        current /= part;
        const auto layer = impl_->layer();
        const auto *entry = layer.find(current);
        if (entry) {
            if (!(entry->attributes & 0x8000))
                throw std::runtime_error("PFS: path component is not a directory");
            if (!fs::is_directory(fs::symlink_status(layer.savedata_host_path(current))))
                throw std::runtime_error("PFS: directory is missing");
            continue;
        }
        impl_->mutate([&] { layer.create_savedata_directory(current); });
    }
}

void PfsSession::remove(const fs::path &path) {
    const std::lock_guard lock(impl_->mutex);
    const auto layer = impl_->layer();
    impl_->mutate([&] { layer.remove_savedata_file(path); });
}

} // namespace pfsmgr
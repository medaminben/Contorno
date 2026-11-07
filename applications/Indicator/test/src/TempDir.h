#ifndef INDICATOR_TEST_TEMPDIR_H
#define INDICATOR_TEST_TEMPDIR_H
#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <random>
#include <string>
#include <system_error>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

/// A fresh directory under the system temp directory, removed on destruction.
/// With `name`, `path` is a folder of that name (it may be non-ASCII) inside it.
struct TempDir {
    std::filesystem::path path;
    explicit TempDir(std::filesystem::path const& name = {}) : path(root_for_new()), root_(path) {
        if (!name.empty())
            path /= name;
        std::filesystem::create_directories(path);
    }
    TempDir(TempDir const&) = delete;
    TempDir& operator=(TempDir const&) = delete;
    ~TempDir() {
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
    }

private:
    /// A folder this call created itself: the name has the process id, a counter and 64 random
    /// bits, and create_directory reports true only for the process that made it, so parallel test
    /// processes (each test runs in its own, with the same seed and counter) never share or delete
    /// one another's folders.
    static std::filesystem::path root_for_new() {
        static std::atomic<unsigned> counter {0};
        std::random_device random;
        for (;;) {
            std::uint64_t const bits = (std::uint64_t(random()) << 32) | random();
            char hex[17];
            std::snprintf(hex, sizeof hex, "%016llx", static_cast<unsigned long long>(bits));
            auto candidate = std::filesystem::temp_directory_path() /
                             ("indicator_test_" + std::to_string(process_id()) + "_" +
                              std::to_string(counter++) + "_" + hex);
            std::error_code error;
            if (std::filesystem::create_directory(candidate, error))
                return candidate;
        }
    }
    static long process_id() {
#ifdef _WIN32
        return static_cast<long>(::_getpid());
#else
        return static_cast<long>(::getpid());
#endif
    }
    std::filesystem::path root_;
};

#endif  // INDICATOR_TEST_TEMPDIR_H

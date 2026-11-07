#include "Zip_impl.h"

#include <array>
#include <cstdint>
#include <exception>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace Tools {

namespace {

// Record signatures and sizes (PKWARE APPNOTE 4.3.7, 4.3.12, 4.3.16).
constexpr std::uint32_t local_signature = 0x04034b50;
constexpr std::uint32_t central_signature = 0x02014b50;
constexpr std::uint32_t end_signature = 0x06054b50;
constexpr std::size_t local_size = 30;
constexpr std::size_t central_size = 46;
constexpr std::size_t end_size = 22;
constexpr std::uint16_t version = 20;  // 2.0: stored entries and folders
constexpr std::uint16_t stored = 0;
constexpr std::uint16_t dos_date = (0 << 9) | (1 << 5) | 1;  // 1980-01-01
constexpr std::uint16_t encrypted_flag = 1;

std::uint32_t crc32(std::string_view data) {
    static auto const table = [] {
        std::array<std::uint32_t, 256> t {};
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) != 0 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[i] = c;
        }
        return t;
    }();
    std::uint32_t c = 0xFFFFFFFFu;
    for (char const ch : data)
        c = table[(c ^ static_cast<std::uint8_t>(ch)) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

/// A problem about the entry `name`: "entry "<name>"…", the name as the value "name".
Core::Problem entry_problem(char const* code, std::string_view english, std::string_view name,
                            std::vector<Core::Problem::Value> more = {}) {
    more.insert(more.begin(), Core::Problem::Value("name", std::string(name)));
    return Core::Problem(code, english, std::move(more));
}

/// None when the name is safe to extract, else the problem.
std::optional<Core::Problem> check_name(std::string_view name) {
    if (name.empty())
        return Core::Problem("zip.empty_name", "an entry has an empty name");
    if (name.front() == '/')
        return entry_problem("zip.entry_absolute", "entry \"{name}\": absolute name", name);
    if (name.find('\\') != std::string_view::npos) {
        return entry_problem("zip.entry_backslash", "entry \"{name}\": name contains '\\'", name);
    }
    for (std::size_t start = 0; start <= name.size();) {
        std::size_t const end = std::min(name.find('/', start), name.size());
        if (name.substr(start, end - start) == "..") {
            return entry_problem("zip.entry_dotdot", "entry \"{name}\": name has a \"..\" segment",
                                 name);
        }
        start = end + 1;
    }
    return std::nullopt;
}

/// "zip: <reason>".
Core::Problem zip_problem(Core::Problem reason) {
    return Core::wrapped("zip.failed", "zip: {reason}", std::move(reason));
}

Core::Problem too_big() {
    return Core::Problem("zip.too_big", "archive of {size} or more", {{"size", "4 GiB"}});
}

void put16(std::string& out, std::uint16_t v) {
    out.push_back(static_cast<char>(v & 0xFF));
    out.push_back(static_cast<char>(v >> 8));
}

void put32(std::string& out, std::uint32_t v) {
    put16(out, static_cast<std::uint16_t>(v & 0xFFFF));
    put16(out, static_cast<std::uint16_t>(v >> 16));
}

std::uint16_t get16(std::string_view bytes, std::size_t at) {
    return static_cast<std::uint16_t>(static_cast<std::uint8_t>(bytes[at]) |
                                      static_cast<std::uint8_t>(bytes[at + 1]) << 8);
}

std::uint32_t get32(std::string_view bytes, std::size_t at) {
    return static_cast<std::uint32_t>(get16(bytes, at)) |
           static_cast<std::uint32_t>(get16(bytes, at + 2)) << 16;
}

/// Offset of the end of central directory record, or npos.
std::size_t find_end(std::string_view bytes) {
    if (bytes.size() < end_size)
        return std::string_view::npos;
    std::size_t const last = bytes.size() - end_size;
    std::size_t const first =
        last > 0xFFFF ? last - 0xFFFF : 0;  // the comment is at most 65535 bytes
    for (std::size_t at = last + 1; at-- > first;) {
        if (get32(bytes, at) == end_signature &&
            at + end_size + get16(bytes, at + 20) == bytes.size())
            return at;
    }
    return std::string_view::npos;
}

}  // namespace

Result<std::string> to_zip_impl(std::vector<ZipEntry> const& entries) noexcept {
    auto fail = [](Core::Problem problem) {
        return Core::error_result<std::string>([&] { return zip_problem(std::move(problem)); });
    };
    try {
        constexpr std::uint64_t limit = std::numeric_limits<std::uint32_t>::max();
        if (entries.size() > 0xFFFF) {
            return fail(Core::Problem("zip.too_many_entries", "more than {count} entries",
                                      {{"count", "65535"}}));
        }
        std::set<std::string> names;
        std::string out;
        std::string central;
        for (auto const& e : entries) {
            if (auto problem = check_name(e.name))
                return fail(std::move(*problem));
            if (!names.insert(e.name).second) {
                return fail(
                    entry_problem("zip.entry_twice", "entry \"{name}\" appears twice", e.name));
            }
            if (e.name.size() > 0xFFFF) {
                return fail(
                    entry_problem("zip.name_too_long", "entry \"{name}\": name too long", e.name));
            }
            if (e.data.size() >= limit || out.size() >= limit)
                return fail(too_big());
            auto const offset = static_cast<std::uint32_t>(out.size());
            auto const size = static_cast<std::uint32_t>(e.data.size());
            auto const name_size = static_cast<std::uint16_t>(e.name.size());
            std::uint32_t const crc = crc32(e.data);
            put32(out, local_signature);
            put16(out, version);
            put16(out, 0);  // flags
            put16(out, stored);
            put16(out, 0);  // time
            put16(out, dos_date);
            put32(out, crc);
            put32(out, size);
            put32(out, size);
            put16(out, name_size);
            put16(out, 0);  // extra field
            out += e.name;
            out += e.data;

            put32(central, central_signature);
            put16(central, version);  // made by
            put16(central, version);  // needed
            put16(central, 0);
            put16(central, stored);
            put16(central, 0);
            put16(central, dos_date);
            put32(central, crc);
            put32(central, size);
            put32(central, size);
            put16(central, name_size);
            put16(central, 0);  // extra field
            put16(central, 0);  // comment
            put16(central, 0);  // disk
            put16(central, 0);  // internal attributes
            put32(central, 0);  // external attributes
            put32(central, offset);
            central += e.name;
        }
        if (out.size() + central.size() >= limit)
            return fail(too_big());
        auto const count = static_cast<std::uint16_t>(entries.size());
        auto const central_offset = static_cast<std::uint32_t>(out.size());
        out += central;
        put32(out, end_signature);
        put16(out, 0);
        put16(out, 0);
        put16(out, count);
        put16(out, count);
        put32(out, static_cast<std::uint32_t>(central.size()));
        put32(out, central_offset);
        put16(out, 0);  // comment
        return Result<std::string>(std::move(out));
    } catch (std::exception const& e) {
        return Core::error_result<std::string>(
            [&] { return zip_problem(Core::Problem::raw(e.what())); });
    }
}

Result<std::vector<ZipEntry>> parse_zip_impl(std::string_view bytes) noexcept {
    auto fail = [](Core::Problem problem) {
        return Core::error_result<std::vector<ZipEntry>>(
            [&] { return zip_problem(std::move(problem)); });
    };
    auto damaged = [](std::size_t i) {
        return Core::Problem("zip.directory_entry_damaged",
                             "central directory entry {number} is damaged",
                             {{"number", std::to_string(i + 1)}});
    };
    try {
        std::size_t const end = find_end(bytes);
        if (end == std::string_view::npos) {
            return fail(
                Core::Problem("zip.not_a_zip", "not a zip archive (no end of central directory)"));
        }
        std::size_t const count = get16(bytes, end + 10);
        std::size_t const directory_size = get32(bytes, end + 12);
        std::size_t at = get32(bytes, end + 16);
        if (get16(bytes, end + 4) != 0 || get16(bytes, end + 6) != 0 ||
            get16(bytes, end + 8) != count) {
            return fail(
                Core::Problem("zip.split", "archives split over several disks are not supported"));
        }
        if (at > end || directory_size > end - at) {
            return fail(Core::Problem("zip.directory_outside",
                                      "the central directory lies outside the archive"));
        }

        std::vector<ZipEntry> entries;
        std::set<std::string> names;
        for (std::size_t i = 0; i < count; ++i) {
            if (at + central_size > end || get32(bytes, at) != central_signature)
                return fail(damaged(i));
            std::uint16_t const flags = get16(bytes, at + 8);
            std::uint16_t const method = get16(bytes, at + 10);
            std::uint32_t const crc = get32(bytes, at + 16);
            std::size_t const packed = get32(bytes, at + 20);
            std::size_t const size = get32(bytes, at + 24);
            std::size_t const name_size = get16(bytes, at + 28);
            std::size_t const skip = name_size + get16(bytes, at + 30) + get16(bytes, at + 32);
            std::size_t const local = get32(bytes, at + 42);
            if (at + central_size + skip > end)
                return fail(damaged(i));
            std::string name(bytes.substr(at + central_size, name_size));
            at += central_size + skip;

            if (auto problem = check_name(name))
                return fail(std::move(*problem));
            auto entry = [&](char const* code, std::string_view english,
                             std::vector<Core::Problem::Value> more = {}) {
                return fail(entry_problem(code, english, name, std::move(more)));
            };
            if (!names.insert(name).second)
                return entry("zip.entry_twice", "entry \"{name}\" appears twice");
            if ((flags & encrypted_flag) != 0) {
                return entry("zip.entry_encrypted",
                             "entry \"{name}\": encrypted entries are not supported");
            }
            if (method != stored) {
                return entry("zip.entry_compressed",
                             "entry \"{name}\": compressed (method {method}); only stored entries "
                             "are supported",
                             {{"method", std::to_string(method)}});
            }
            if (packed != size)
                return entry("zip.entry_sizes", "entry \"{name}\": stored sizes differ");
            if (local > end || local_size > end - local || get32(bytes, local) != local_signature) {
                return entry("zip.entry_local_damaged",
                             "entry \"{name}\": local header is damaged");
            }
            std::size_t const data =
                local + local_size + get16(bytes, local + 26) + get16(bytes, local + 28);
            if (data > end || size > end - data) {
                return entry("zip.entry_outside",
                             "entry \"{name}\": data lies outside the archive");
            }
            if (bytes.substr(local + local_size, get16(bytes, local + 26)) != name) {
                return entry(
                    "zip.entry_local_name", "entry \"{name}\": local header names \"{other}\"",
                    {{"other",
                      std::string(bytes.substr(local + local_size, get16(bytes, local + 26)))}});
            }
            std::string_view const content = bytes.substr(data, size);
            if (crc32(content) != crc)
                return entry("zip.entry_crc", "entry \"{name}\": CRC-32 mismatch");
            entries.push_back({std::move(name), std::string(content)});
        }
        return Result<std::vector<ZipEntry>>(std::move(entries));
    } catch (std::exception const& e) {
        return Core::error_result<std::vector<ZipEntry>>(
            [&] { return zip_problem(Core::Problem::raw(e.what())); });
    }
}

}  // namespace Tools

#include "model/Language.h"

#include <algorithm>

StringTable indicator_strings_en();
StringTable indicator_strings_de();
StringTable indicator_strings_es();
StringTable indicator_strings_it();
StringTable indicator_strings_fr();
StringTable indicator_strings_nl();
StringTable indicator_strings_ar();

namespace {

struct Language {
    char const* code;
    char const* name;  // UTF-8
    StringTable (*strings)();
};

constexpr Language languages[] = {
    {"en", "English", indicator_strings_en},    {"de", "Deutsch", indicator_strings_de},
    {"es", "Español", indicator_strings_es},    {"it", "Italiano", indicator_strings_it},
    {"fr", "Français", indicator_strings_fr},   {"nl", "Nederlands", indicator_strings_nl},
    {"ar", "العربية", indicator_strings_ar},
};

std::unordered_map<std::string, StringTable>& tables() {
    static std::unordered_map<std::string, StringTable> t;
    return t;
}

std::string& current() {
    static std::string code = "en";
    return code;
}

}  // namespace

std::vector<std::string> language_codes() {
    std::vector<std::string> codes;
    for (auto const& l : languages) codes.push_back(l.code);
    return codes;
}

std::string language_name(std::string const& code) {
    for (auto const& l : languages) {
        if (code == l.code) return l.name;
    }
    return {};
}

std::string current_language() { return current(); }

std::string set_current_language(std::string const& code) {
    auto const codes = language_codes();
    current() = std::find(codes.begin(), codes.end(), code) != codes.end() ? code : "en";
    return current();
}

void add_strings(std::string const& code, StringTable const& table) {
    auto& target = tables()[code];
    for (auto const& [key, text] : table) target.insert_or_assign(key, text);
}

std::string lookup(std::string const& code, std::string const& key) {
    for (std::string const& c : {code, std::string("en")}) {
        auto const t = tables().find(c);
        if (t == tables().end()) continue;
        auto const s = t->second.find(key);
        if (s != t->second.end()) return s->second;
    }
    return key;
}

std::vector<std::string> string_keys(std::string const& code) {
    std::vector<std::string> keys;
    auto const t = tables().find(code);
    if (t != tables().end()) {
        for (auto const& entry : t->second) keys.push_back(entry.first);
    }
    std::sort(keys.begin(), keys.end());
    return keys;
}

void register_indicator_strings() {
    for (auto const& l : languages) {
        // idempotent: the same keys set again to the same text
        add_strings(l.code, l.strings());
    }
}

TextDirection text_direction(std::string const& code) {
    return code == "ar" ? TextDirection::right_to_left : TextDirection::left_to_right;
}

TextDirection text_direction() { return text_direction(current()); }

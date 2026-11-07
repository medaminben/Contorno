#ifndef INDICATOR_LANGUAGE_H
#define INDICATOR_LANGUAGE_H
#include <string>
#include <unordered_map>
#include <vector>

/// The interface languages, in the order of the Set Up dialog's keys:
/// en de es it fr nl ar.
std::vector<std::string> language_codes();
/// A language in its own words, never translated ("Deutsch", "العربية"). UTF-8.
std::string language_name(std::string const& code);
/// The language applied last ("en" before any).
std::string current_language();
/// Makes `code` the current language (unknown: "en") and returns it; the
/// widgets follow when the UI applies it.
std::string set_current_language(std::string const& code);

using StringTable = std::unordered_map<std::string, std::string>;
/// Merges `table` into the language's; tests add their own keys.
void add_strings(std::string const& code, StringTable const& table);
/// The text of `key` in `code`; else in English; else the key itself.
std::string lookup(std::string const& code, std::string const& key);
/// The keys of the language's table, sorted.
std::vector<std::string> string_keys(std::string const& code);
/// The seven tables of the shared strings/; idempotent.
void register_indicator_strings();

enum class TextDirection { left_to_right, right_to_left };
/// Right to left only in Arabic.
TextDirection text_direction();
TextDirection text_direction(std::string const& code);

#endif  // INDICATOR_LANGUAGE_H

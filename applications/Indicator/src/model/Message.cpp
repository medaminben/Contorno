#include "model/Message.h"

#include "model/Language.h"

#include <utility>

namespace {
// U+2066 LEFT-TO-RIGHT ISOLATE and U+2069 POP DIRECTIONAL ISOLATE, as UTF-8
std::string const isolate_begin = "\xE2\x81\xA6";
std::string const isolate_end = "\xE2\x81\xA9";
}  // namespace

Message::Message(std::string key) : key_(std::move(key)) {}

Message& Message::with(std::string name, std::string plain) {
    values_.push_back({std::move(name), std::move(plain), {}});
    return *this;
}

Message& Message::with(std::string name, Message nested) {
    Value value {std::move(name), {}, {}};
    value.nested.push_back(std::move(nested));
    values_.push_back(std::move(value));
    return *this;
}

std::string Message::render(std::string const& code) const {
    if (key_.empty())
        return {};
    std::string const text = lookup(code, key_);
    std::string out;
    std::size_t at = 0;
    // one pass: an inserted value is never searched for placeholders again
    while (at < text.size()) {
        std::size_t const open = text.find('{', at);
        std::size_t const close =
            open == std::string::npos ? std::string::npos : text.find('}', open);
        if (close == std::string::npos)
            break;
        std::string const name = text.substr(open + 1, close - open - 1);
        out += text.substr(at, open - at);
        bool found = false;
        for (auto const& v : values_) {
            if (v.name != name)
                continue;
            if (!v.nested.empty()) {
                out += v.nested.front().render(code);
            } else if (text_direction(code) == TextDirection::right_to_left) {
                // numbers, units and paths read as in English inside right to left text
                out += isolate_begin + v.plain + isolate_end;
            } else {
                out += v.plain;
            }
            found = true;
            break;
        }
        if (!found)
            out += text.substr(open, close - open + 1);
        at = close + 1;
    }
    return out + text.substr(at);
}

std::vector<Message> Message::nested() const {
    std::vector<Message> out;
    for (auto const& v : values_)
        out.insert(out.end(), v.nested.begin(), v.nested.end());
    return out;
}

std::string Message::render() const {
    return render(current_language());
}

std::string Message::english() const {
    return render("en");
}

#ifndef INDICATOR_MESSAGE_H
#define INDICATOR_MESSAGE_H
#include <string>
#include <vector>

/// A sentence of the interface: a key into the language tables and the values
/// of its `{name}` placeholders. Rendered on the GUI thread, in any language,
/// when it is shown. Plain values (numbers with units, paths, library text)
/// are inserted as written; nested messages are rendered in the same language.
class Message {
public:
    Message() = default;  ///< renders ""
    explicit Message(std::string key);

    Message& with(std::string name, std::string plain);
    Message& with(std::string name, Message nested);

    std::string const& key() const { return key_; }
    /// The messages inside the values, in order (for checking a message's parts).
    std::vector<Message> nested() const;
    /// In `code`: nested messages first, then each {name} replaced; an unknown
    /// {name} stays as written. The key itself when no table has it.
    std::string render(std::string const& code) const;
    std::string render() const;   ///< in the current language
    std::string english() const;  ///< render("en")

private:
    struct Value {
        std::string name;
        std::string plain;
        std::vector<Message> nested;  ///< one element at most (a Message cannot contain itself)
    };
    std::string key_;
    std::vector<Value> values_;
};

#endif  // INDICATOR_MESSAGE_H

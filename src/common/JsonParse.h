// EXT-17 — a JSON parser, and nothing more.
//
// The repo already has `Json.h`, which *writes*. This reads. They are deliberately separate
// files: the writer's job is that our output never varies between two identical runs, and the
// reader's job is to be exact about somebody else's bytes. Neither property helps the other.
//
// Two things here are requirements rather than taste:
//
//   - **Objects preserve member order.** `capture-format-v1.md` §8.2 makes the order of a
//     sample's `fields` normative and says a reader using an order-preserving parse can verify
//     it against the header's declaration order. A hash map loses that check silently, so the
//     members of an object are a vector of pairs and lookup is a linear scan. Objects in this
//     format have at most about fifteen keys.
//   - **Never throws** (constraint C3). Every failure is `false` plus a named error carrying the
//     column it happened at. Recursion is depth-limited for the same reason: a hostile or
//     corrupt line must be a returned error, never a blown stack.
//
// Numbers keep their original text as well as a parsed double. The format's doubles are written
// in shortest round-trip form (§8.3) and its integers can exceed what a double represents
// exactly, so a consumer that needs the integer reads it from the text.
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace ext17::json {

enum class Type { Null, Bool, Number, String, Array, Object };

class Value;
using Member = std::pair<std::string, Value>;

class Value {
public:
    Value() = default;

    [[nodiscard]] Type type() const { return type_; }
    [[nodiscard]] bool isNull() const { return type_ == Type::Null; }
    [[nodiscard]] bool isBool() const { return type_ == Type::Bool; }
    [[nodiscard]] bool isNumber() const { return type_ == Type::Number; }
    [[nodiscard]] bool isString() const { return type_ == Type::String; }
    [[nodiscard]] bool isArray() const { return type_ == Type::Array; }
    [[nodiscard]] bool isObject() const { return type_ == Type::Object; }

    [[nodiscard]] bool boolean() const { return bool_; }
    [[nodiscard]] double number() const { return number_; }
    // The number's original text, exactly as it appeared. Empty for non-numbers.
    [[nodiscard]] const std::string& raw() const { return text_; }
    // The string's value, unescaped. Empty for non-strings.
    [[nodiscard]] const std::string& text() const { return text_; }

    [[nodiscard]] const std::vector<Value>& elements() const { return elements_; }
    [[nodiscard]] const std::vector<Member>& members() const { return members_; }

    // Order-preserving lookup. Returns nullptr when the key is absent, which is how the format
    // spells "the publisher did not send this" (§8.2) - distinct from null and from zero.
    [[nodiscard]] const Value* find(const char* key) const;

    // Convenience readers. Each returns the fallback when the key is absent or the wrong type,
    // so a caller that has already decided absence is acceptable does not have to branch twice.
    [[nodiscard]] std::string stringOr(const char* key, const std::string& fallback = {}) const;
    [[nodiscard]] double numberOr(const char* key, double fallback = 0.0) const;
    [[nodiscard]] long long integerOr(const char* key, long long fallback = 0) const;
    [[nodiscard]] bool boolOr(const char* key, bool fallback = false) const;

    // Exact integer, read from the number's text rather than through a double.
    [[nodiscard]] bool asInteger(long long& out) const;
    // True when the number's text carries no '.', 'e' or 'E' - i.e. it was written integrally.
    [[nodiscard]] bool isIntegralText() const;

    friend class Parser;

private:
    Type type_ = Type::Null;
    bool bool_ = false;
    double number_ = 0.0;
    std::string text_;
    std::vector<Value> elements_;
    std::vector<Member> members_;
};

struct ParseError {
    std::size_t column = 0;   // 1-based, into the text handed to parse()
    std::string message;
};

// Parses one complete JSON document. Trailing content after the value is an error: every line
// of a capture is exactly one object (§2), so anything after it is a malformed line.
bool parse(const std::string& text, Value& out, ParseError& err);

// Text to double through the C locale explicitly, never through the ambient one. This is the
// parser's own conversion, exported at M5 so that the axis reads a declared value the same way
// the reader reads a captured one - CR-DET-2's locale hazard has exactly one implementation
// here, and `tools/n8ro-compare/build.cmd` searches for any other spelling of it.
double toDoubleCLocale(const std::string& text);

} // namespace ext17::json

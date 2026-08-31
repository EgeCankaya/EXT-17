// EXT-17 — the JSON parser. See JsonParse.h for why it exists and what it guarantees.
#include "JsonParse.h"

#include <cerrno>
#include <clocale>
#include <cstdlib>
#include <cstring>

namespace ext17::json {
namespace {

// A corrupt line must be a returned error, never a blown stack. Capture records nest five deep
// at their worst (header -> schemas -> entry -> fields -> entry), so this is generous.
constexpr int kMaxDepth = 64;

bool isDigit(char c) { return c >= '0' && c <= '9'; }

// Append one Unicode code point as UTF-8. An unpaired surrogate becomes the replacement
// character rather than an error: the format's strings are the platform's own bytes, and
// losing a whole line over one of them would be worse than carrying it through.
void appendUtf8(std::string& out, unsigned long cp) {
    if (cp > 0x10FFFF) cp = 0xFFFD;
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

// strtod reads the decimal separator from the current locale. A capture is always written with
// '.', and CR-DET-2 requires this project to behave identically under a comma-decimal locale,
// so the conversion goes through the C locale explicitly rather than hopefully.
double parseDoubleCLocale(const std::string& text) {
#ifdef _MSC_VER
    static const _locale_t c = _create_locale(LC_NUMERIC, "C");
    return _strtod_l(text.c_str(), nullptr, c);
#else
    return std::strtod(text.c_str(), nullptr);
#endif
}

} // namespace

class Parser {
public:
    Parser(const std::string& text, ParseError& err) : s_(text), err_(err) {}

    bool run(Value& out) {
        skipSpace();
        if (!parseValue(out, 0)) return false;
        skipSpace();
        if (i_ != s_.size()) return fail("trailing content after the value");
        return true;
    }

private:
    bool fail(const char* what) {
        err_.column = i_ + 1;
        err_.message = what;
        return false;
    }

    void skipSpace() {
        while (i_ < s_.size()) {
            const char c = s_[i_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++i_;
            else break;
        }
    }

    bool literal(const char* word) {
        const std::size_t n = std::strlen(word);
        if (s_.compare(i_, n, word) != 0) return fail("unrecognised literal");
        i_ += n;
        return true;
    }

    bool parseValue(Value& v, int depth) {
        if (depth > kMaxDepth) return fail("nesting too deep");
        if (i_ >= s_.size()) return fail("unexpected end of input");
        switch (s_[i_]) {
            case '{': return parseObject(v, depth);
            case '[': return parseArray(v, depth);
            case '"':
                v.type_ = Type::String;
                return parseString(v.text_);
            case 't':
                if (!literal("true")) return false;
                v.type_ = Type::Bool; v.bool_ = true; return true;
            case 'f':
                if (!literal("false")) return false;
                v.type_ = Type::Bool; v.bool_ = false; return true;
            case 'n':
                if (!literal("null")) return false;
                v.type_ = Type::Null; return true;
            default: return parseNumber(v);
        }
    }

    bool parseObject(Value& v, int depth) {
        v.type_ = Type::Object;
        ++i_;  // opening brace
        skipSpace();
        if (i_ < s_.size() && s_[i_] == '}') { ++i_; return true; }
        for (;;) {
            skipSpace();
            if (i_ >= s_.size() || s_[i_] != '"') return fail("expected a member name");
            std::string key;
            if (!parseString(key)) return false;
            skipSpace();
            if (i_ >= s_.size() || s_[i_] != ':') return fail("expected a colon after a member name");
            ++i_;
            skipSpace();
            // Constructed in place: a member's value can be an arbitrarily large sub-document,
            // and copying it once per member would be the parser's whole cost.
            v.members_.emplace_back(std::move(key), Value{});
            if (!parseValue(v.members_.back().second, depth + 1)) return false;
            skipSpace();
            if (i_ >= s_.size()) return fail("unterminated object");
            if (s_[i_] == ',') { ++i_; continue; }
            if (s_[i_] == '}') { ++i_; return true; }
            return fail("expected a comma or a closing brace in an object");
        }
    }

    bool parseArray(Value& v, int depth) {
        v.type_ = Type::Array;
        ++i_;  // opening bracket
        skipSpace();
        if (i_ < s_.size() && s_[i_] == ']') { ++i_; return true; }
        for (;;) {
            skipSpace();
            v.elements_.emplace_back();
            if (!parseValue(v.elements_.back(), depth + 1)) return false;
            skipSpace();
            if (i_ >= s_.size()) return fail("unterminated array");
            if (s_[i_] == ',') { ++i_; continue; }
            if (s_[i_] == ']') { ++i_; return true; }
            return fail("expected a comma or a closing bracket in an array");
        }
    }

    bool parseHex4(unsigned long& out) {
        if (i_ + 4 > s_.size()) return fail("truncated unicode escape");
        out = 0;
        for (int k = 0; k < 4; ++k) {
            const char c = s_[i_ + k];
            out <<= 4;
            if (c >= '0' && c <= '9') out |= static_cast<unsigned long>(c - '0');
            else if (c >= 'a' && c <= 'f') out |= static_cast<unsigned long>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') out |= static_cast<unsigned long>(c - 'A' + 10);
            else return fail("bad hex digit in a unicode escape");
        }
        i_ += 4;
        return true;
    }

    bool parseString(std::string& out) {
        ++i_;  // opening quote
        out.clear();
        for (;;) {
            if (i_ >= s_.size()) return fail("unterminated string");
            const char c = s_[i_];
            if (c == '"') { ++i_; return true; }
            if (c == '\\') {
                ++i_;
                if (i_ >= s_.size()) return fail("unterminated escape");
                const char e = s_[i_++];
                switch (e) {
                    case '"':  out.push_back('"');  break;
                    case '\\': out.push_back('\\'); break;
                    case '/':  out.push_back('/');  break;
                    case 'b':  out.push_back('\b'); break;
                    case 'f':  out.push_back('\f'); break;
                    case 'n':  out.push_back('\n'); break;
                    case 'r':  out.push_back('\r'); break;
                    case 't':  out.push_back('\t'); break;
                    case 'u': {
                        unsigned long cp = 0;
                        if (!parseHex4(cp)) return false;
                        if (cp >= 0xD800 && cp <= 0xDBFF && i_ + 1 < s_.size()
                            && s_[i_] == '\\' && s_[i_ + 1] == 'u') {
                            const std::size_t save = i_;
                            i_ += 2;
                            unsigned long low = 0;
                            if (!parseHex4(low)) return false;
                            if (low >= 0xDC00 && low <= 0xDFFF) {
                                cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                            } else {
                                i_ = save;  // not a pair after all; leave the next escape alone
                            }
                        }
                        appendUtf8(out, cp);
                        break;
                    }
                    default: return fail("unrecognised string escape");
                }
                continue;
            }
            // Every other byte passes through unaltered, which is what the format promises for
            // the bytes of a multi-byte UTF-8 sequence (§8.3).
            out.push_back(c);
            ++i_;
        }
    }

    bool parseNumber(Value& v) {
        const std::size_t start = i_;
        if (i_ < s_.size() && s_[i_] == '-') ++i_;
        if (i_ >= s_.size() || !isDigit(s_[i_])) return fail("expected a number");
        while (i_ < s_.size() && isDigit(s_[i_])) ++i_;
        if (i_ < s_.size() && s_[i_] == '.') {
            ++i_;
            if (i_ >= s_.size() || !isDigit(s_[i_])) return fail("expected a digit after the point");
            while (i_ < s_.size() && isDigit(s_[i_])) ++i_;
        }
        if (i_ < s_.size() && (s_[i_] == 'e' || s_[i_] == 'E')) {
            ++i_;
            if (i_ < s_.size() && (s_[i_] == '+' || s_[i_] == '-')) ++i_;
            if (i_ >= s_.size() || !isDigit(s_[i_])) return fail("expected a digit in the exponent");
            while (i_ < s_.size() && isDigit(s_[i_])) ++i_;
        }
        v.type_ = Type::Number;
        v.text_.assign(s_, start, i_ - start);
        v.number_ = parseDoubleCLocale(v.text_);
        return true;
    }

    const std::string& s_;
    ParseError& err_;
    std::size_t i_ = 0;
};

const Value* Value::find(const char* key) const {
    if (type_ != Type::Object) return nullptr;
    for (const Member& m : members_) {
        if (m.first == key) return &m.second;
    }
    return nullptr;
}

std::string Value::stringOr(const char* key, const std::string& fallback) const {
    const Value* v = find(key);
    return (v && v->isString()) ? v->text() : fallback;
}

double Value::numberOr(const char* key, double fallback) const {
    const Value* v = find(key);
    return (v && v->isNumber()) ? v->number() : fallback;
}

long long Value::integerOr(const char* key, long long fallback) const {
    const Value* v = find(key);
    long long out = 0;
    return (v && v->asInteger(out)) ? out : fallback;
}

bool Value::boolOr(const char* key, bool fallback) const {
    const Value* v = find(key);
    return (v && v->isBool()) ? v->boolean() : fallback;
}

bool Value::asInteger(long long& out) const {
    if (type_ != Type::Number || !isIntegralText()) return false;
    errno = 0;
    char* end = nullptr;
    const long long v = std::strtoll(text_.c_str(), &end, 10);
    if (errno != 0 || end == text_.c_str() || *end != '\0') return false;
    out = v;
    return true;
}

bool Value::isIntegralText() const {
    if (type_ != Type::Number) return false;
    return text_.find_first_of(".eE") == std::string::npos;
}

bool parse(const std::string& text, Value& out, ParseError& err) {
    out = Value{};
    err = ParseError{};
    Parser p(text, err);
    return p.run(out);
}

} // namespace ext17::json

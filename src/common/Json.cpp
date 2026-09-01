#include "Json.h"

#include <clocale>
#include <cstdio>

namespace ext17::json {
namespace {

// The fixed-point formatters read the decimal separator from the ambient LC_NUMERIC. Under a
// comma-decimal locale that turns `19.500000` into `19,500000`, which is not a JSON number: the
// run record then does not parse, and [B]'s rule 6 - *"a stored run should be re-assertable
// without re-running it"* - fails on a file this project wrote itself. Measured, not reasoned:
// under German_Germany.1252 this project's own parser refuses its own run.json at column 50.
//
// So the conversion goes through the C locale explicitly, exactly as `JsonParse.cpp` already
// does for the reading direction. That file is the only other place a number crosses between
// text and double here, and the two now agree by construction rather than by luck.
void formatFixedCLocale(char* buffer, std::size_t size, int decimals, double v) {
#ifdef _MSC_VER
    static const _locale_t c = _create_locale(LC_NUMERIC, "C");
    _snprintf_s_l(buffer, size, _TRUNCATE, "%.*f", c, decimals, v);
#else
    std::snprintf(buffer, size, "%.*f", decimals, v);
#endif
}

} // namespace

std::string escape(const std::string& in) {
    std::string out;
    out.reserve(in.size() + 8);
    for (const unsigned char c : in) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", static_cast<unsigned>(c));
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

Writer::Writer(int indentWidth) : indentWidth_(indentWidth) {
    firstInScope_.push_back(true);
}

void Writer::newlineIndent() {
    out_ += '\n';
    out_.append(static_cast<std::size_t>(depth_ * indentWidth_), ' ');
}

void Writer::prefix(const char* key) {
    if (firstInScope_.empty()) {
        firstInScope_.push_back(true);
    }
    if (!firstInScope_.back()) {
        out_ += ',';
    }
    firstInScope_.back() = false;
    if (depth_ > 0 || !out_.empty()) {
        newlineIndent();
    }
    if (key) {
        out_ += '"';
        out_ += escape(key);
        out_ += "\": ";
    }
}

void Writer::beginObject(const char* key) {
    prefix(key);
    out_ += '{';
    ++depth_;
    firstInScope_.push_back(true);
}

void Writer::endObject() {
    if (depth_ > 0) { --depth_; }
    if (firstInScope_.size() > 1) {
        const bool empty = firstInScope_.back();
        firstInScope_.pop_back();
        if (!empty) { newlineIndent(); }
    }
    out_ += '}';
}

void Writer::beginArray(const char* key) {
    prefix(key);
    out_ += '[';
    ++depth_;
    firstInScope_.push_back(true);
}

void Writer::endArray() {
    if (depth_ > 0) { --depth_; }
    if (firstInScope_.size() > 1) {
        const bool empty = firstInScope_.back();
        firstInScope_.pop_back();
        if (!empty) { newlineIndent(); }
    }
    out_ += ']';
}

void Writer::member(const char* key, const std::string& v) {
    prefix(key);
    out_ += '"';
    out_ += escape(v);
    out_ += '"';
}

void Writer::member(const char* key, const char* v) {
    member(key, std::string(v ? v : ""));
}

void Writer::member(const char* key, bool v) {
    prefix(key);
    out_ += v ? "true" : "false";
}

void Writer::member(const char* key, std::uint64_t v) {
    prefix(key);
    out_ += std::to_string(v);
}

void Writer::member(const char* key, std::int64_t v) {
    prefix(key);
    out_ += std::to_string(v);
}

void Writer::member(const char* key, double v, int decimals) {
    prefix(key);
    char buf[64] = {0};
    formatFixedCLocale(buf, sizeof buf, decimals, v);
    out_ += buf;
}

void Writer::memberNull(const char* key) {
    prefix(key);
    out_ += "null";
}

void Writer::value(const std::string& v) {
    prefix(nullptr);
    out_ += '"';
    out_ += escape(v);
    out_ += '"';
}

} // namespace ext17::json

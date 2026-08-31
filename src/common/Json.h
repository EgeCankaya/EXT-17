// EXT-17 — a JSON writer, and nothing more.
//
// Why hand-rolled: the per-run record is the one file this milestone authors, its shape is
// closed, and CR-DET-2 requires that nothing in our output vary between identical runs. A
// writer that emits members in the order they were added, formats doubles with an explicit
// format string, and never iterates an unordered container gives that by construction. A
// dependency would have to be audited for the same property.
//
// Never throws. A malformed call produces a malformed-but-safe document, not an exception.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ext17::json {

// An append-only writer over one buffer. Objects and arrays nest; the writer tracks whether a
// separator is needed so callers never have to.
class Writer {
public:
    explicit Writer(int indentWidth = 2);

    void beginObject(const char* key = nullptr);
    void endObject();
    void beginArray(const char* key = nullptr);
    void endArray();

    void member(const char* key, const std::string& value);
    void member(const char* key, const char* value);
    void member(const char* key, bool value);
    void member(const char* key, std::uint64_t value);
    void member(const char* key, std::int64_t value);
    // Doubles are written with an explicit, locale-independent format so two identical runs
    // produce identical text. `decimals` is the fixed precision.
    void member(const char* key, double value, int decimals);
    void memberNull(const char* key);

    void value(const std::string& v);

    [[nodiscard]] const std::string& str() const { return out_; }

private:
    void prefix(const char* key);
    void newlineIndent();

    std::string out_;
    int indentWidth_;
    int depth_ = 0;
    std::vector<bool> firstInScope_;
};

// Escape a string for inclusion in a JSON document. Control characters below 0x20 become \u00XX.
std::string escape(const std::string& in);

} // namespace ext17::json

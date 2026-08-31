// EXT-17 — the JSON writer's own test.
//
// The per-run record is the only file this project authors so far, and CR-DET-2 requires that
// nothing of ours varies between identical runs. The writer earns that by emitting members in
// the order they were added, formatting doubles through an explicit format string, and never
// iterating an unordered container - properties that are easy to state and easy to break.
//
// This test is deliberately narrow: escaping, nesting, empties, and the shape of a document.
// It links nothing. `tests\build.cmd` runs it, and it needs no N8RO install to pass.
//
// Never throws, like everything else here: a failure is a printed line and a non-zero exit.

#include "../src/common/Json.h"

#include <cstdio>
#include <string>

namespace {

int g_failures = 0;

void expect(const char* what, const std::string& actual, const std::string& expected) {
    if (actual == expected) {
        std::printf("  ok   %s\n", what);
    } else {
        ++g_failures;
        std::printf("  FAIL %s\n       expected: %s\n       actual:   %s\n",
                    what, expected.c_str(), actual.c_str());
    }
}

void escaping() {
    std::printf("escaping\n");
    expect("plain text is unchanged", ext17::json::escape("hello"), "hello");
    expect("a quote is escaped", ext17::json::escape("a\"b"), "a\\\"b");
    // The one that matters most here: every path in a run record is a Windows path.
    expect("a backslash is doubled", ext17::json::escape("C:\\N8RO\\bin"), "C:\\\\N8RO\\\\bin");
    expect("newline and tab", ext17::json::escape("a\nb\tc"), "a\\nb\\tc");
    expect("carriage return", ext17::json::escape("a\rb"), "a\\rb");
    expect("backspace and form feed", ext17::json::escape("a\bb\fc"), "a\\bb\\fc");
    expect("a control character becomes \\u00XX",
           ext17::json::escape(std::string("x\x01y")), "x\\u0001y");
    // Non-ASCII passes through as its UTF-8 bytes. Scenario names in this install include
    // "Istanbul KAAN Su-35 BVR Angajmani" with a dotless i, so this is not hypothetical.
    expect("UTF-8 passes through", ext17::json::escape("Angajman\xc4\xb1"),
           "Angajman\xc4\xb1");
}

void documentShape() {
    std::printf("document shape\n");

    {
        ext17::json::Writer w;
        w.beginObject();
        w.endObject();
        expect("an empty object", w.str(), "{}");
    }
    {
        ext17::json::Writer w;
        w.beginObject();
        w.member("a", static_cast<std::uint64_t>(1));
        w.endObject();
        expect("one member", w.str(), "{\n  \"a\": 1\n}");
    }
    {
        ext17::json::Writer w;
        w.beginObject();
        w.beginArray("xs");
        w.endArray();
        w.beginObject("o");
        w.endObject();
        w.memberNull("n");
        w.endObject();
        expect("empty array, empty object, null", w.str(),
               "{\n  \"xs\": [],\n  \"o\": {},\n  \"n\": null\n}");
    }
    {
        ext17::json::Writer w;
        w.beginObject();
        w.member("t", true);
        w.member("f", false);
        w.endObject();
        expect("booleans", w.str(), "{\n  \"t\": true,\n  \"f\": false\n}");
    }
}

void numbers() {
    std::printf("numbers\n");
    {
        ext17::json::Writer w;
        w.beginObject();
        // Fixed precision, explicitly: the same value must render the same way every run.
        w.member("dt", 0.05, 5);
        w.member("t", 60.0, 6);
        w.member("neg", -23.4187047, 5);
        w.member("i", static_cast<std::int64_t>(-7));
        w.member("u", static_cast<std::uint64_t>(1200));
        w.endObject();
        expect("fixed-precision doubles and integers", w.str(),
               "{\n  \"dt\": 0.05000,\n  \"t\": 60.000000,\n"
               "  \"neg\": -23.41870,\n  \"i\": -7,\n  \"u\": 1200\n}");
    }
}

void stability() {
    std::printf("stability\n");
    // CR-DET-2 in miniature: the same inputs twice produce the same bytes. Nothing in the
    // writer reads a clock, a locale or a hash order, and this is the check that says so.
    const auto build = [] {
        ext17::json::Writer w;
        w.beginObject();
        w.member("run_id", std::string("000"));
        w.member("observed_frame", static_cast<std::uint64_t>(1200));
        w.member("observed_sim_time_s", 60.0, 6);
        w.beginArray("waits");
        for (int i = 0; i < 5; ++i) {
            w.beginObject();
            w.member("i", static_cast<std::int64_t>(i));
            w.endObject();
        }
        w.endArray();
        w.endObject();
        return w.str();
    };
    expect("two identical documents are byte-identical", build(), build());
}

} // namespace

int main() {
    std::printf("json_writer_test\n");
    escaping();
    documentShape();
    numbers();
    stability();
    if (g_failures == 0) {
        std::printf("json_writer_test: all checks passed\n");
        return 0;
    }
    std::printf("json_writer_test: %d check(s) FAILED\n", g_failures);
    return 1;
}

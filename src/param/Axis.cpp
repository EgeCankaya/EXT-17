#include "Axis.h"

#include "../common/JsonParse.h"

#include <algorithm>
#include <cstdio>
#include <set>
#include <vector>

namespace ext17::param {
namespace {

// Reads a whole small file. A campaign file is configuration a person wrote; there is no size
// bound worth having beyond refusing something absurd, and a bound stops a mistyped path from
// pulling a capture into memory.
constexpr std::size_t kMaxConfigBytes = 4u * 1024u * 1024u;

bool readWholeFile(const std::string& path, std::string& out, std::string& error) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        error = "campaign file could not be opened: " + path;
        return false;
    }
    out.clear();
    char buffer[8192];
    std::size_t n = 0;
    while ((n = std::fread(buffer, 1, sizeof(buffer), f)) > 0) {
        if (out.size() + n > kMaxConfigBytes) {
            std::fclose(f);
            error = "campaign file is larger than 4 MiB, which a campaign file is not: " + path;
            return false;
        }
        out.append(buffer, n);
    }
    std::fclose(f);
    return true;
}

// A JSON value's declared text, whether it was written as a string or as a number. The parser
// keeps a number's original characters (§8.3's reason, reused here), so `27.5` survives as
// `27.5` and never becomes `27.500000`.
bool declaredText(const json::Value& v, std::string& out) {
    if (v.isString()) { out = v.text(); return true; }
    if (v.isNumber()) { out = v.raw(); return true; }
    return false;
}

// A locale-free syntactic check that the declared text really is a number, in JSON's own
// grammar. It exists because `toDoubleCLocale` returns 0.0 for text it cannot read, and a
// value silently becoming 0 in the middle of a sweep is the shape of mistake this project
// keeps refusing rather than absorbing.
bool looksNumeric(const std::string& t) {
    std::size_t i = 0;
    const auto digits = [&]() {
        const std::size_t start = i;
        while (i < t.size() && t[i] >= '0' && t[i] <= '9') { ++i; }
        return i > start;
    };
    if (i < t.size() && (t[i] == '+' || t[i] == '-')) { ++i; }
    if (!digits()) { return false; }
    if (i < t.size() && t[i] == '.') { ++i; if (!digits()) { return false; } }
    if (i < t.size() && (t[i] == 'e' || t[i] == 'E')) {
        ++i;
        if (i < t.size() && (t[i] == '+' || t[i] == '-')) { ++i; }
        if (!digits()) { return false; }
    }
    return i == t.size();
}

// A key written twice in one object. The JSON parser preserves member order and resolves a
// lookup to the first match, which is exactly right for a capture - §13 says an unrecognised
// key is ignored, and a producer that repeats one is still a producer. A campaign file is not a
// capture. It is ours, a person wrote it, and a key written twice means one of the two lines
// they wrote is not doing anything. That is worth a sentence rather than a silent first-wins.
bool hasDuplicateKey(const json::Value& obj, std::string& duplicate) {
    const auto& m = obj.members();
    for (std::size_t i = 0; i < m.size(); ++i) {
        for (std::size_t j = i + 1; j < m.size(); ++j) {
            if (m[i].first == m[j].first) { duplicate = m[i].first; return true; }
        }
    }
    return false;
}

// A key this file does not know. `contract/capture-format-v1.md` §13 says an unrecognised key
// in a capture is IGNORED, and the reader implements exactly that - it is the whole reason the
// format version has held across three producer releases. **The opposite rule applies here**,
// and the difference is who wrote the file: a producer adds keys and an old reader must survive
// them, whereas a campaign file is written by a person and an unknown key is a typo. `"value"`
// for `"values"` would otherwise be a sweep that silently refused to exist.
//
// A key beginning with `_` is a comment. JSON has none, a configuration file needs them, and
// carving out one prefix costs less than the alternative of accepting everything.
bool unknownKey(const json::Value& obj, const std::vector<std::string>& known,
                std::string& found) {
    for (const auto& m : obj.members()) {
        if (!m.first.empty() && m.first[0] == '_') { continue; }
        bool isKnown = false;
        for (const auto& k : known) {
            if (k == m.first) { isKnown = true; break; }
        }
        if (!isKnown) { found = m.first; return true; }
    }
    return false;
}

bool readDirection(const json::Value& v, std::array<double, 3>& out, std::string& error) {
    if (!v.isArray() || v.elements().size() != 3) {
        error = "\"direction_ned\" must be an array of exactly three numbers";
        return false;
    }
    for (std::size_t i = 0; i < 3; ++i) {
        const json::Value& e = v.elements()[i];
        if (!e.isNumber()) {
            error = "\"direction_ned\" must be an array of exactly three numbers";
            return false;
        }
        out[i] = e.number();
    }
    if (out[0] == 0.0 && out[1] == 0.0 && out[2] == 0.0) {
        error = "\"direction_ned\" is all zeroes, so the value would scale nothing";
        return false;
    }
    return true;
}

} // namespace

const char* toString(Kind kind) {
    switch (kind) {
        case Kind::VelocityNedScaled: return "velocity_ned_scaled";
    }
    return "unknown";
}

std::vector<std::size_t> Axis::sweepOrder() const {
    std::vector<std::size_t> order(values.size());
    for (std::size_t i = 0; i < order.size(); ++i) { order[i] = i; }
    // std::stable_sort, so a repeated value keeps declaration order. Not std::sort: with equal
    // keys its result is unspecified, and an unspecified order in a report is a value that
    // varies between two identical runs of the reporting code (CR-DET-2).
    std::stable_sort(order.begin(), order.end(), [this](std::size_t a, std::size_t b) {
        return values[a].number < values[b].number;
    });
    return order;
}

const Value* Axis::selfTestValue() const {
    for (const auto& v : values) {
        if (v.text == selfTestValueText) { return &v; }
    }
    return nullptr;
}

std::array<double, 3> Axis::velocityFor(const Target& target, const Value& value) const {
    switch (kind) {
        case Kind::VelocityNedScaled:
            return {target.directionNed[0] * value.number,
                    target.directionNed[1] * value.number,
                    target.directionNed[2] * value.number};
    }
    return {0.0, 0.0, 0.0};
}

bool parseCampaignText(const std::string& text, Axis& out, std::string& error) {
    out = Axis{};

    json::Value doc;
    json::ParseError perr;
    if (!json::parse(text, doc, perr)) {
        error = "campaign file is not valid JSON: " + perr.message + " (column "
                + std::to_string(perr.column) + ")";
        return false;
    }
    if (!doc.isObject()) {
        error = "campaign file must be a JSON object";
        return false;
    }

    const json::Value* axis = doc.find("axis");
    if (!axis || !axis->isObject()) {
        error = "campaign file has no \"axis\" object. CR-PAR-1 requires the axis to be declared "
                "in the campaign configuration rather than in code, so there is no default";
        return false;
    }

    {
        std::string duplicate;
        if (hasDuplicateKey(doc, duplicate)) {
            error = "\"" + duplicate + "\" is written twice at the top level of the campaign "
                    "file; the second is doing nothing";
            return false;
        }
        if (hasDuplicateKey(*axis, duplicate)) {
            error = "\"axis." + duplicate + "\" is written twice; the second is doing nothing";
            return false;
        }

        std::string unknown;
        if (unknownKey(doc, {"axis"}, unknown)) {
            error = "\"" + unknown + "\" is not a key this campaign file format has. A key "
                    "beginning with '_' is a comment; anything else is a typo, and a typo that "
                    "was ignored would be a sweep that silently did not happen";
            return false;
        }
        if (unknownKey(*axis, {"name", "kind", "applies_to", "units", "entity_groups", "values",
                               "self_test_value"}, unknown)) {
            error = "\"axis." + unknown + "\" is not a key this campaign file format has. A key "
                    "beginning with '_' is a comment; anything else is a typo";
            return false;
        }
    }

    out.name = axis->stringOr("name");
    if (out.name.empty()) {
        error = "\"axis.name\" is required; it labels the parameter in every run record and in "
                "the report";
        return false;
    }
    out.appliesTo = axis->stringOr("applies_to");
    out.units = axis->stringOr("units");

    const std::string kind = axis->stringOr("kind", "velocity_ned_scaled");
    if (kind == "velocity_ned_scaled") {
        out.kind = Kind::VelocityNedScaled;
    } else {
        error = "\"axis.kind\" is \"" + kind + "\", which is not implemented. One kind is: "
                "\"velocity_ned_scaled\". [B] settles the count at one axis and OQ-4 settles "
                "which; see docs/m5-oq4.md";
        return false;
    }

    // Targets, declared in groups so that a raid of thirty entities on one heading is two lines
    // rather than thirty. Groups are flattened here; nothing downstream knows they existed.
    const json::Value* groups = axis->find("entity_groups");
    if (!groups || !groups->isArray() || groups->elements().empty()) {
        error = "\"axis.entity_groups\" must be a non-empty array. Each group carries a "
                "\"direction_ned\" and the \"names\" it applies to - names, never patterns, "
                "because nothing here reads the roster (see src/param/Axis.h)";
        return false;
    }
    std::set<std::string> seen;
    for (const json::Value& g : groups->elements()) {
        if (!g.isObject()) {
            error = "every entry of \"axis.entity_groups\" must be an object";
            return false;
        }
        {
            std::string unknown;
            if (unknownKey(g, {"direction_ned", "names"}, unknown)) {
                error = "\"" + unknown + "\" is not a key an entity group has. A key beginning "
                        "with '_' is a comment; anything else is a typo";
                return false;
            }
            std::string duplicate;
            if (hasDuplicateKey(g, duplicate)) {
                error = "\"" + duplicate + "\" is written twice in one entity group; the second "
                        "is doing nothing";
                return false;
            }
        }
        std::array<double, 3> dir{0.0, 0.0, 0.0};
        const json::Value* d = g.find("direction_ned");
        if (!d) {
            error = "an entity group has no \"direction_ned\"";
            return false;
        }
        if (!readDirection(*d, dir, error)) { return false; }

        const json::Value* names = g.find("names");
        if (!names || !names->isArray() || names->elements().empty()) {
            error = "an entity group has no non-empty \"names\" array";
            return false;
        }
        for (const json::Value& n : names->elements()) {
            if (!n.isString() || n.text().empty()) {
                error = "\"names\" must contain non-empty strings";
                return false;
            }
            if (n.text().find('*') != std::string::npos
                || n.text().find('?') != std::string::npos) {
                error = "\"" + n.text() + "\" looks like a pattern, and there is no pattern "
                        "matching here. Resolving one would mean subscribing the control path to "
                        "sim/entity/state, which would perturb the publication schedule the "
                        "determinism gate measures. Name the entities";
                return false;
            }
            if (!seen.insert(n.text()).second) {
                error = "\"" + n.text() + "\" appears in more than one entity group, so two "
                        "directions would be published for it and the last would win silently";
                return false;
            }
            out.targets.push_back(Target{n.text(), dir});
        }
    }

    const json::Value* values = axis->find("values");
    if (!values || !values->isArray() || values->elements().empty()) {
        error = "\"axis.values\" must be a non-empty array. A sweep with no values is not a "
                "sweep";
        return false;
    }
    for (const json::Value& v : values->elements()) {
        std::string t;
        if (!declaredText(v, t) || t.empty()) {
            error = "every entry of \"axis.values\" must be a number or a string holding one";
            return false;
        }
        // The number is derived from the text through the C locale, and the text is what is
        // reported. A value whose text is not a number is refused here rather than becoming a
        // silent zero in the middle of a sweep. Note "1,5": under a comma-decimal locale a
        // hopeful conversion would read 1, and this reads neither - it refuses.
        if (!looksNumeric(t)) {
            error = "\"" + t + "\" in \"axis.values\" is not a number. Values are written in "
                    "JSON's own number grammar with a '.' separator, whatever the machine's "
                    "locale, and are carried into the report as the text you wrote";
            return false;
        }
        out.values.push_back(Value{t, json::toDoubleCLocale(t)});
    }

    // Which value the gate runs at. Defaulting to the first DECLARED value rather than to the
    // lowest swept one keeps it something the author chose by writing it first.
    const json::Value* st = axis->find("self_test_value");
    if (st) {
        std::string t;
        if (!declaredText(*st, t) || t.empty()) {
            error = "\"axis.self_test_value\" must be a number or a string holding one";
            return false;
        }
        out.selfTestValueText = t;
        // Matched on TEXT, not on the double: "55" and "55.0" are two ways of writing one
        // number and two different labels, and the report prints the label.
        if (!out.selfTestValue()) {
            error = "\"axis.self_test_value\" is \"" + t + "\", which is not one of "
                    "\"axis.values\". The gate runs at a value the campaign actually sweeps, so "
                    "that what it establishes is about a run the campaign performs";
            return false;
        }
    } else {
        out.selfTestValueText = out.values.front().text;
    }

    return true;
}

bool readCampaignFile(const std::string& path, Axis& out, std::string& error) {
    std::string text;
    if (!readWholeFile(path, text, error)) { return false; }
    if (!parseCampaignText(text, out, error)) {
        error = path + ": " + error;
        return false;
    }
    return true;
}

} // namespace ext17::param

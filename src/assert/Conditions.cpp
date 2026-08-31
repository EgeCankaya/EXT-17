#include "Conditions.h"

#include "../common/JsonParse.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace ext17::assertion {
namespace {

// A condition file is configuration a person wrote. The bound exists so that a mistyped path
// pointing at a capture is a named refusal rather than 24 MB pulled into memory.
constexpr std::size_t kMaxConditionBytes = 4u * 1024u * 1024u;

bool readWholeFile(const std::string& path, std::string& out, ParseError& error) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        error.code = ParseCode::FileUnreadable;
        error.detail = "condition file could not be opened: " + path;
        return false;
    }
    out.clear();
    char buffer[8192];
    std::size_t n = 0;
    while ((n = std::fread(buffer, 1, sizeof(buffer), f)) > 0) {
        if (out.size() + n > kMaxConditionBytes) {
            std::fclose(f);
            error.code = ParseCode::FileTooLarge;
            error.detail = "condition file is larger than 4 MiB, which a condition file is "
                           "not: " + path;
            return false;
        }
        out.append(buffer, n);
    }
    std::fclose(f);
    return true;
}

// A key beginning with `_` is a comment. JSON has none and a configuration file needs them, and
// carving out one prefix costs less than accepting everything — which is the rule this file
// deliberately does not inherit. See the header, superseding rule 1.
bool isComment(const std::string& key) { return !key.empty() && key[0] == '_'; }

bool unknownKey(const json::Value& obj, const std::vector<const char*>& known,
                std::string& found) {
    for (const auto& m : obj.members()) {
        if (isComment(m.first)) { continue; }
        bool isKnown = false;
        for (const char* k : known) {
            if (m.first == k) { isKnown = true; break; }
        }
        if (!isKnown) { found = m.first; return true; }
    }
    return false;
}

// A key written twice. The JSON parser preserves member order and resolves a lookup to the first
// match — right for a capture, wrong here, where the second line the author wrote is doing
// nothing and nobody is told. F-23 is this exact defect, found in a campaign file at M5.
bool duplicateKey(const json::Value& obj, std::string& found) {
    const auto& m = obj.members();
    for (std::size_t i = 0; i < m.size(); ++i) {
        for (std::size_t j = i + 1; j < m.size(); ++j) {
            if (m[i].first == m[j].first) { found = m[i].first; return true; }
        }
    }
    return false;
}

// The declared text of a value, whether written as a string or as a number. M5's rule, applied
// to a threshold: `3000` reaches the report as `3000`, and the double exists to compare with.
bool declaredText(const json::Value& v, std::string& out) {
    if (v.isString()) { out = v.text(); return true; }
    if (v.isNumber()) { out = v.raw(); return true; }
    return false;
}

bool readNumber(const json::Value& obj, const char* key, double& value, std::string& text) {
    const json::Value* v = obj.find(key);
    if (!v || !v->isNumber()) { return false; }
    value = v->number();
    return declaredText(*v, text);
}

bool readNonEmptyString(const json::Value& obj, const char* key, std::string& out) {
    const json::Value* v = obj.find(key);
    if (!v || !v->isString() || v->text().empty()) { return false; }
    out = v->text();
    return true;
}

// A latitude/longitude pair, in the platform's own order and units (§15). Never converted.
bool readLatLon(const json::Value& v, std::array<double, 2>& out) {
    if (!v.isArray() || v.elements().size() != 2) { return false; }
    for (std::size_t i = 0; i < 2; ++i) {
        if (!v.elements()[i].isNumber()) { return false; }
        out[i] = v.elements()[i].number();
    }
    return true;
}

bool plausibleLatLon(double lat, double lon) {
    return lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0;
}

// A polygon crossing the antimeridian or containing a pole is not supported, and a loader that
// answered one anyway would answer it wrongly — the ray-cast treats longitude as a plane
// coordinate. Refused by name rather than mis-evaluated. The test is deliberately blunt: a span
// wider than 180° in longitude cannot be a scenario-scale figure and is the shape a seam
// crossing takes.
bool spansSeam(const std::vector<std::array<double, 2>>& vertices) {
    double minLon = vertices[0][1];
    double maxLon = vertices[0][1];
    for (const auto& v : vertices) {
        if (v[1] < minLon) { minLon = v[1]; }
        if (v[1] > maxLon) { maxLon = v[1]; }
    }
    return (maxLon - minLon) > 180.0;
}

bool parseProximity(const json::Value& obj, Condition& c, ParseError& error) {
    static const std::vector<const char*> known =
        {"id", "kind", "expect", "entities", "within_m"};
    std::string found;
    if (unknownKey(obj, known, found)) {
        error.code = ParseCode::UnknownKey;
        error.detail = "\"" + found + "\" is not a key of a proximity condition. A condition "
                       "file is ours and a person wrote it, so an unrecognised key is refused "
                       "rather than ignored - it is how \"within_meters\" for \"within_m\" "
                       "would otherwise become a threshold that silently did not apply. A key "
                       "beginning with '_' is a comment";
        return false;
    }

    const json::Value* entities = obj.find("entities");
    if (!entities || !entities->isArray() || entities->elements().size() != 2) {
        error.code = ParseCode::MissingKey;
        error.detail = "a proximity condition needs \"entities\": exactly two entity names";
        return false;
    }
    for (std::size_t i = 0; i < 2; ++i) {
        if (!entities->elements()[i].isString() || entities->elements()[i].text().empty()) {
            error.code = ParseCode::BadValue;
            error.detail = "\"entities\" must be exactly two non-empty entity names";
            return false;
        }
    }
    c.entityA = entities->elements()[0].text();
    c.entityB = entities->elements()[1].text();
    if (c.entityA == c.entityB) {
        error.code = ParseCode::EntityNamedTwice;
        error.detail = "\"" + c.entityA + "\" is named twice. An entity is at distance zero "
                       "from itself, so the condition is met at every sample and can never be "
                       "evidence of anything";
        return false;
    }

    if (!readNumber(obj, "within_m", c.withinM, c.withinMText) || !(c.withinM > 0.0)) {
        error.code = ParseCode::MissingKey;
        error.detail = "a proximity condition needs \"within_m\": a positive threshold in "
                       "metres. Units are the platform's own and are never converted";
        return false;
    }
    return true;
}

bool parseRegion(const json::Value& region, Condition& c, ParseError& error) {
    static const std::vector<const char*> known =
        {"shape", "centre", "center", "radius_m", "vertices"};
    std::string found;
    if (unknownKey(region, known, found)) {
        error.code = ParseCode::UnknownKey;
        error.detail = "\"" + found + "\" is not a key of a region";
        return false;
    }
    std::string dup;
    if (duplicateKey(region, dup)) {
        error.code = ParseCode::DuplicateKey;
        error.detail = "\"" + dup + "\" is written twice in one region";
        return false;
    }

    std::string shape;
    if (!readNonEmptyString(region, "shape", shape)) {
        error.code = ParseCode::MissingKey;
        error.detail = "a region needs \"shape\": \"circle\" or \"polygon\"";
        return false;
    }
    if (shape == "circle") {
        c.shape = AreaShape::Circle;
        // `centre` and `center` are both accepted, as the vendored shape says. Both are
        // recognised keys, so the refuse-unknown rule does not break a file written either way.
        const json::Value* centre = region.find("centre");
        if (!centre) { centre = region.find("center"); }
        if (!centre || !centre->isArray() ||
            centre->elements().size() < 2 || centre->elements().size() > 3) {
            error.code = ParseCode::MissingKey;
            error.detail = "a circle needs \"centre\": [latitude, longitude] or "
                           "[latitude, longitude, altitude]. Altitude may be omitted and "
                           "defaults to 0";
            return false;
        }
        c.centre = {0.0, 0.0, 0.0};
        for (std::size_t i = 0; i < centre->elements().size(); ++i) {
            if (!centre->elements()[i].isNumber()) {
                error.code = ParseCode::BadValue;
                error.detail = "\"centre\" must be numbers in the platform's own "
                               "[lat, lon, alt] order and units";
                return false;
            }
            c.centre[i] = centre->elements()[i].number();
        }
        if (!plausibleLatLon(c.centre[0], c.centre[1])) {
            error.code = ParseCode::BadValue;
            error.detail = "\"centre\" is not a latitude and longitude in degrees. The order is "
                           "the platform's own, [lat, lon, alt], and is never converted";
            return false;
        }
        if (!readNumber(region, "radius_m", c.radiusM, c.radiusMText) || !(c.radiusM > 0.0)) {
            error.code = ParseCode::MissingKey;
            error.detail = "a circle needs \"radius_m\": a positive radius in metres";
            return false;
        }
        if (region.find("vertices")) {
            error.code = ParseCode::UnknownKey;
            error.detail = "\"vertices\" belongs to a polygon, not to a circle";
            return false;
        }
        return true;
    }
    if (shape == "polygon") {
        c.shape = AreaShape::Polygon;
        const json::Value* vertices = region.find("vertices");
        if (!vertices || !vertices->isArray() || vertices->elements().size() < 3) {
            error.code = ParseCode::MissingKey;
            error.detail = "a polygon needs \"vertices\": at least three "
                           "[latitude, longitude] points";
            return false;
        }
        for (const auto& v : vertices->elements()) {
            std::array<double, 2> point{0.0, 0.0};
            if (!readLatLon(v, point) || !plausibleLatLon(point[0], point[1])) {
                error.code = ParseCode::BadValue;
                error.detail = "every vertex must be [latitude, longitude] in degrees, in the "
                               "platform's own order";
                return false;
            }
            c.vertices.push_back(point);
        }
        if (spansSeam(c.vertices)) {
            error.code = ParseCode::PolygonSpansSeam;
            error.detail = "this polygon spans more than 180 degrees of longitude. Polygons are "
                           "plane figures in latitude/longitude, which is accurate at scenario "
                           "scale and is not defended across the antimeridian or a pole. "
                           "Refused rather than answered wrongly";
            return false;
        }
        if (region.find("radius_m") || region.find("centre") || region.find("center")) {
            error.code = ParseCode::UnknownKey;
            error.detail = "\"centre\" and \"radius_m\" belong to a circle, not to a polygon";
            return false;
        }
        return true;
    }
    error.code = ParseCode::BadValue;
    error.detail = "\"" + shape + "\" is not a region shape. The shapes are \"circle\" and "
                   "\"polygon\"";
    return false;
}

bool parseArea(const json::Value& obj, Condition& c, ParseError& error) {
    static const std::vector<const char*> known =
        {"id", "kind", "expect", "entity", "test", "region"};
    std::string found;
    if (unknownKey(obj, known, found)) {
        error.code = ParseCode::UnknownKey;
        error.detail = "\"" + found + "\" is not a key of an area condition";
        return false;
    }
    if (!readNonEmptyString(obj, "entity", c.entity)) {
        error.code = ParseCode::MissingKey;
        error.detail = "an area condition needs \"entity\": one entity name";
        return false;
    }
    if (const json::Value* test = obj.find("test")) {
        if (!test->isString()) {
            error.code = ParseCode::BadValue;
            error.detail = "\"test\" must be \"inside\" or \"outside\"";
            return false;
        }
        if (test->text() == "inside") {
            c.test = AreaTest::Inside;
        } else if (test->text() == "outside") {
            c.test = AreaTest::Outside;
        } else {
            error.code = ParseCode::BadValue;
            error.detail = "\"" + test->text() + "\" is not an area test. The tests are "
                           "\"inside\" (the default) and \"outside\"";
            return false;
        }
    }
    const json::Value* region = obj.find("region");
    if (!region || !region->isObject()) {
        error.code = ParseCode::MissingKey;
        error.detail = "an area condition needs \"region\": an object with a \"shape\"";
        return false;
    }
    return parseRegion(*region, c, error);
}

bool parseTerminalState(const json::Value& obj, Condition& c, ParseError& error) {
    static const std::vector<const char*> known =
        {"id", "kind", "expect", "entity", "removal_reason", "field", "equals"};
    std::string found;
    if (unknownKey(obj, known, found)) {
        error.code = ParseCode::UnknownKey;
        error.detail = "\"" + found + "\" is not a key of a terminal_state condition";
        return false;
    }
    if (!readNonEmptyString(obj, "entity", c.entity)) {
        error.code = ParseCode::MissingKey;
        error.detail = "a terminal_state condition needs \"entity\": one entity name";
        return false;
    }

    const bool hasRemoval = obj.find("removal_reason") != nullptr;
    const bool hasField = obj.find("field") != nullptr || obj.find("equals") != nullptr;
    if (hasRemoval && hasField) {
        error.code = ParseCode::BothTerminalForms;
        error.detail = "a terminal_state condition uses \"removal_reason\", or \"field\" with "
                       "\"equals\", never both. The two are classified differently under "
                       "CR-AS-4 and a condition that is both has no single soundness";
        return false;
    }
    if (hasRemoval) {
        c.form = TerminalForm::RemovalReason;
        if (!readNonEmptyString(obj, "removal_reason", c.removalReason)) {
            error.code = ParseCode::BadValue;
            error.detail = "\"removal_reason\" must be a non-empty string, matched verbatim "
                           "against entity_remove.reason";
            return false;
        }
        if (c.removalReason == kTeardownRemovalReason) {
            error.code = ParseCode::TeardownIsNotTerminal;
            error.detail = "\"" + c.removalReason + "\" is the removal the engine's stop path "
                           "writes for every surviving entity at teardown - measured, 267 of "
                           "385 removals across the committed sweep, every one at sim_time_s 0. "
                           "A condition on it is met in every run for every entity and its "
                           "deciding time points at the wrong end of the run. The brief asks "
                           "whether anything reached a terminal state it SHOULD NOT have; being "
                           "unloaded at the end is not that";
            return false;
        }
        return true;
    }
    if (hasField) {
        c.form = TerminalForm::FieldEquals;
        if (!readNonEmptyString(obj, "field", c.field)) {
            error.code = ParseCode::MissingKey;
            error.detail = "\"field\" must be a non-empty sample field name, used with "
                           "\"equals\"";
            return false;
        }
        const json::Value* equals = obj.find("equals");
        if (!equals) {
            error.code = ParseCode::MissingKey;
            error.detail = "\"field\" is used with \"equals\": the value the field must take";
            return false;
        }
        if (!declaredText(*equals, c.equals) && !equals->isBool()) {
            error.code = ParseCode::BadValue;
            error.detail = "\"equals\" must be a string, a number or a boolean";
            return false;
        }
        if (equals->isBool()) { c.equals = equals->boolean() ? "true" : "false"; }
        return true;
    }
    error.code = ParseCode::MissingKey;
    error.detail = "a terminal_state condition needs either \"removal_reason\", or \"field\" "
                   "with \"equals\"";
    return false;
}

bool parseOne(const json::Value& obj, std::size_t index, Condition& c, ParseError& error) {
    error.index = index;
    if (!obj.isObject()) {
        error.code = ParseCode::BadValue;
        error.detail = "every element of \"conditions\" must be an object";
        return false;
    }
    std::string dup;
    if (duplicateKey(obj, dup)) {
        error.code = ParseCode::DuplicateKey;
        error.detail = "\"" + dup + "\" is written twice in one condition. One of the two lines "
                       "is doing nothing, and a first-wins resolution would never say so";
        return false;
    }
    if (!readNonEmptyString(obj, "id", c.id)) {
        error.code = ParseCode::MissingId;
        error.detail = "every condition needs an \"id\": it is what a verdict is traced by";
        return false;
    }
    error.conditionId = c.id;

    // The one key added to the vendored shape. Absent means `met`, which is how a file written
    // for EXT-08's referee — which has no such key — reads here.
    if (const json::Value* expect = obj.find("expect")) {
        if (!expect->isString()) {
            error.code = ParseCode::BadValue;
            error.detail = "\"expect\" must be \"met\" or \"not_met\"";
            return false;
        }
        if (expect->text() == "met") {
            c.expect = Expect::Met;
        } else if (expect->text() == "not_met") {
            c.expect = Expect::NotMet;
        } else {
            error.code = ParseCode::BadValue;
            error.detail = "\"" + expect->text() + "\" is not an expectation. It is \"met\" "
                           "(the default) or \"not_met\" - the latter is how the brief's "
                           "\"did anything reach a terminal state it SHOULD NOT have\" is "
                           "written, and it is the one key this project adds to the vendored "
                           "condition shape";
            return false;
        }
    }

    std::string kind;
    if (!readNonEmptyString(obj, "kind", kind)) {
        error.code = ParseCode::MissingKey;
        error.detail = "every condition needs a \"kind\": \"proximity\", \"area\" or "
                       "\"terminal_state\"";
        return false;
    }
    if (kind == "proximity") {
        c.kind = Kind::Proximity;
        return parseProximity(obj, c, error);
    }
    if (kind == "area") {
        c.kind = Kind::Area;
        return parseArea(obj, c, error);
    }
    if (kind == "terminal_state") {
        c.kind = Kind::TerminalState;
        return parseTerminalState(obj, c, error);
    }
    error.code = ParseCode::UnrecognisedKind;
    error.detail = "\"" + kind + "\" is not a condition kind. The vocabulary is CLOSED at three "
                   "- \"proximity\", \"area\" and \"terminal_state\" - and an unrecognised kind "
                   "is this error rather than a silently skipped condition, because a campaign "
                   "that judged everything except the one that mattered would report all passed. "
                   "A fourth kind is a change to the requirements, not to a file";
    return false;
}

} // namespace

const char* toString(Kind kind) {
    switch (kind) {
        case Kind::Proximity: return "proximity";
        case Kind::Area: return "area";
        case Kind::TerminalState: return "terminal_state";
    }
    return "unknown";
}

const char* toString(Expect e) {
    switch (e) {
        case Expect::Met: return "met";
        case Expect::NotMet: return "not_met";
    }
    return "unknown";
}

const char* toString(ParseCode code) {
    switch (code) {
        case ParseCode::Ok: return "ok";
        case ParseCode::FileUnreadable: return "file_unreadable";
        case ParseCode::FileTooLarge: return "file_too_large";
        case ParseCode::MalformedJson: return "malformed_json";
        case ParseCode::NotAnObject: return "not_an_object";
        case ParseCode::MissingConditions: return "missing_conditions";
        case ParseCode::ConditionsNotAnArray: return "conditions_not_an_array";
        case ParseCode::NoConditionsDeclared: return "no_conditions_declared";
        case ParseCode::UnknownKey: return "unknown_key";
        case ParseCode::DuplicateKey: return "duplicate_key";
        case ParseCode::DuplicateId: return "duplicate_id";
        case ParseCode::UnrecognisedKind: return "unrecognised_kind";
        case ParseCode::MissingId: return "missing_id";
        case ParseCode::MissingKey: return "missing_key";
        case ParseCode::BadValue: return "bad_value";
        case ParseCode::EntityNamedTwice: return "entity_named_twice";
        case ParseCode::BothTerminalForms: return "both_terminal_forms";
        case ParseCode::TeardownIsNotTerminal: return "teardown_is_not_a_terminal_state";
        case ParseCode::PolygonSpansSeam: return "polygon_spans_seam";
    }
    return "unknown";
}

std::string ParseError::message() const {
    std::string out = toString(code);
    if (!conditionId.empty()) { out += " [" + conditionId + "]"; }
    if (!detail.empty()) { out += ": " + detail; }
    return out;
}

std::vector<std::string> Condition::namedEntities() const {
    std::vector<std::string> out;
    switch (kind) {
        case Kind::Proximity:
            out.push_back(entityA);
            out.push_back(entityB);
            break;
        case Kind::Area:
        case Kind::TerminalState:
            out.push_back(entity);
            break;
    }
    return out;
}

bool parseConditionText(const std::string& text,
                        const std::string& label,
                        ConditionFile& out,
                        ParseError& error) {
    out = ConditionFile{};
    out.path = label;
    error = ParseError{};

    json::Value root;
    json::ParseError perr;
    if (!json::parse(text, root, perr)) {
        error.code = ParseCode::MalformedJson;
        error.detail = "column " + std::to_string(perr.column) + ": " + perr.message;
        return false;
    }
    if (!root.isObject()) {
        error.code = ParseCode::NotAnObject;
        error.detail = "a condition file is one JSON object with a \"conditions\" array";
        return false;
    }
    std::string dup;
    if (duplicateKey(root, dup)) {
        error.code = ParseCode::DuplicateKey;
        error.detail = "\"" + dup + "\" is written twice at the top level";
        return false;
    }
    static const std::vector<const char*> known = {"conditions"};
    std::string found;
    if (unknownKey(root, known, found)) {
        error.code = ParseCode::UnknownKey;
        error.detail = "\"" + found + "\" is not a top-level key of a condition file. The only "
                       "one is \"conditions\"; a key beginning with '_' is a comment";
        return false;
    }

    const json::Value* conditions = root.find("conditions");
    if (!conditions) {
        error.code = ParseCode::MissingConditions;
        error.detail = "a condition file needs a \"conditions\" array";
        return false;
    }
    if (!conditions->isArray()) {
        error.code = ParseCode::ConditionsNotAnArray;
        error.detail = "\"conditions\" must be an array";
        return false;
    }
    if (conditions->elements().empty()) {
        // CR-AS-1's third acceptance criterion. A campaign never runs with zero conditions
        // loaded unless zero were declared, and that case is reported explicitly rather than as
        // a pass — twenty confident passes that checked nothing is the failure the rule exists
        // to prevent.
        error.code = ParseCode::NoConditionsDeclared;
        error.detail = "\"conditions\" is empty. A campaign that judged nothing would report "
                       "every run as passing, so an empty condition file is refused here rather "
                       "than discovered in the summary";
        return false;
    }

    for (std::size_t i = 0; i < conditions->elements().size(); ++i) {
        Condition c;
        if (!parseOne(conditions->elements()[i], i, c, error)) { return false; }
        for (const auto& existing : out.conditions) {
            if (existing.id == c.id) {
                error.code = ParseCode::DuplicateId;
                error.conditionId = c.id;
                error.index = i;
                error.detail = "\"" + c.id + "\" is declared twice. An id is what a verdict is "
                               "traced by, so two conditions sharing one produce two verdicts "
                               "nobody can tell apart";
                return false;
            }
        }
        out.conditions.push_back(c);
    }
    return true;
}

bool readConditionFile(const std::string& path, ConditionFile& out, ParseError& error) {
    out = ConditionFile{};
    error = ParseError{};
    std::string text;
    if (!readWholeFile(path, text, error)) { return false; }
    if (!parseConditionText(text, path, out, error)) { return false; }
    out.path = path;
    return true;
}

} // namespace ext17::assertion

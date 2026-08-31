// EXT-17 — the conformant reader. See CaptureReader.h.
#include "CaptureReader.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <tuple>
#include <vector>

namespace ext17::capture {
namespace {

std::string num(long long v) { return std::to_string(v); }

// Doubles appear only in diagnostic detail text here. %.17g round-trips every double, which is
// what a message about a time value has to do to be checkable against the file.
std::string dbl(double v) {
    char buf[40];
    std::snprintf(buf, sizeof buf, "%.17g", v);
    return buf;
}

const char* const kSegmentCloseReasons[] = {"scenario_unloaded", "host_lost", "shutdown", "size_limit"};
const char* const kEndReasons[] = {"shutdown", "host_lost", "size_limit", "replay_end"};

template <std::size_t N>
bool inClosedSet(const std::string& v, const char* const (&set)[N]) {
    for (std::size_t i = 0; i < N; ++i) {
        if (v == set[i]) return true;
    }
    return false;
}

void readOptional(const json::Value& obj, const char* key, OptionalCount& out) {
    const json::Value* v = obj.find(key);
    long long n = 0;
    if (v && v->asInteger(n)) {
        out.present = true;
        out.value = n;
    }
}

// The identity of a thing being sampled: the segment it is in, and the pair (§8.1). Never the
// name alone — the engine re-creates entities under names it has already used, both mid-run and
// at teardown, and keying on name merges two different bodies.
using EntityKey = std::tuple<long long, std::string, long long>;

struct EntityState {
    bool counted = false;          // already counted into its segment's distinct-key tally
    bool removed = false;
    bool haveTime = false;
    double lastTime = 0.0;
    long long runAtLastTime = 0;   // consecutive samples for THIS key at `lastTime`
    long long maxRun = 0;
    long long samples = 0;
};

class Reader {
public:
    Reader(const ReadOptions& opt, RecordSink* sink) : opt_(opt), sink_(sink) {}

    ReadResult& result() { return r_; }

    // Returns false once the file has been rejected, at which point the caller stops feeding
    // lines. That is the whole of "no partial parse": the loop ends, nothing after line 1 is
    // looked at, and `tallies.lines` says so.
    bool line(const std::string& raw, std::size_t lineNo);

    void finish();

private:
    void note(Code code, std::size_t lineNo, const std::string& detail) {
        const long long seen = ++r_.diagnosticCounts[code];
        if (static_cast<std::size_t>(seen) <= opt_.maxDiagnosticsPerCode) {
            r_.diagnostics.push_back(Diagnostic{code, lineNo, detail});
        }
    }

    void reject(Code code, const std::string& detail) {
        r_.rejected = true;
        r_.rejectCode = code;
        r_.rejectDetail = detail;
    }

    bool header(const std::string& raw, std::size_t lineNo);
    void readSchemas(const json::Value& obj, std::size_t lineNo);
    void dispatch(const json::Value& obj, const std::string& type, std::size_t lineNo);
    void segmentOpen(const json::Value& obj, std::size_t lineNo);
    void segmentClose(const json::Value& obj, std::size_t lineNo);
    void sample(const json::Value& obj, std::size_t lineNo);
    void entityAdd(const json::Value& obj, std::size_t lineNo);
    void entityRemove(const json::Value& obj, std::size_t lineNo);
    void verdict(const json::Value& obj, std::size_t lineNo);
    void trailer(const json::Value& obj, std::size_t lineNo);
    void verifyFields(const json::Value& obj, const std::string& message, std::size_t lineNo);
    void verifyScalar(const FieldDecl& decl, const json::Value& v, std::size_t lineNo,
                      const std::string& where);

    // Common envelope (§5). Returns false and reports when a required key is missing or is the
    // wrong shape; the caller then skips the record rather than guessing at it.
    bool envelope(const json::Value& obj, const char* type, std::size_t lineNo,
                  double& simTime, long long& segment);

    SegmentStats& segmentFor(long long ordinal);

    ReadOptions opt_;
    RecordSink* sink_ = nullptr;
    ReadResult r_;

    bool sawHeader_ = false;
    bool sawTrailer_ = false;
    long long openSegment_ = -1;
    long long highestOpened_ = -1;

    // Ordered containers throughout. CR-DET-2's third named hazard is an unordered container
    // iterated where the output depends on the order; nothing here is an unordered_map, and
    // that is a requirement rather than a preference.
    std::map<long long, SegmentStats> segments_;
    std::map<long long, bool> segmentHasTime_;
    std::map<long long, double> segmentLastTime_;
    std::map<EntityKey, EntityState> entities_;

    json::Value parsed_;   // reused across lines: this is what keeps the reader streaming
};

SegmentStats& Reader::segmentFor(long long ordinal) {
    auto it = segments_.find(ordinal);
    if (it != segments_.end()) return it->second;
    SegmentStats s;
    s.key.part = r_.header.part;
    s.key.segment = ordinal;
    return segments_.emplace(ordinal, s).first->second;
}

bool Reader::envelope(const json::Value& obj, const char* type, std::size_t lineNo,
                      double& simTime, long long& segment) {
    const json::Value* t = obj.find("sim_time_s");
    const json::Value* s = obj.find("segment");
    if (!t || !t->isNumber()) {
        note(Code::MissingRequiredKey, lineNo,
             std::string(type) + " has no numeric sim_time_s");
        return false;
    }
    if (!s || !s->asInteger(segment)) {
        note(Code::MissingRequiredKey, lineNo,
             std::string(type) + " has no integer segment ordinal");
        return false;
    }
    simTime = t->number();
    return true;
}

bool Reader::header(const std::string& raw, std::size_t lineNo) {
    // Step 2 before step 1's remainder: the version is checked from the first key of the first
    // line, before any other parsing. A file that fails here is never parsed further, which is
    // what CR-CAP-3 means by "no partial parse" and what the booby-trapped mutant proves.
    std::string version;
    if (peekFormatVersion(raw, version)) {
        if (version != kFormatVersion) {
            reject(Code::UnsupportedFormatVersion,
                   "found \"" + version + "\", this reader implements \"" + kFormatVersion + "\"");
            return false;
        }
    }

    json::ParseError err;
    if (!json::parse(raw, parsed_, err)) {
        reject(Code::NotACapture,
               "the first line is not valid JSON (column " + num(static_cast<long long>(err.column))
                   + ": " + err.message + ")");
        return false;
    }
    if (!parsed_.isObject()) {
        reject(Code::NotACapture, "the first line is not a JSON object");
        return false;
    }
    if (parsed_.stringOr("type") != "header") {
        reject(Code::NotACapture,
               "the first record's type is \"" + parsed_.stringOr("type") + "\", not \"header\"");
        return false;
    }
    // The slow path, for a header whose first key is not `format_version`: the value is still
    // authoritative, and the ordering violation is itself worth reporting.
    if (version.empty()) {
        version = parsed_.stringOr("format_version");
        if (version != kFormatVersion) {
            reject(Code::UnsupportedFormatVersion,
                   "found \"" + version + "\", this reader implements \"" + kFormatVersion + "\"");
            return false;
        }
        note(Code::FormatVersionNotFirst, lineNo,
             "format_version is not the first key of the header (§6 requires it first so an "
             "unknown version can be rejected before anything else is parsed)");
    }

    Header& h = r_.header;
    h.formatVersion = version;
    if (const json::Value* p = parsed_.find("producer")) {
        h.producerName = p->stringOr("name");
        h.producerVersion = p->stringOr("version");
    }
    if (const json::Value* p = parsed_.find("platform")) {
        h.engineConfig = p->stringOr("engine_config");
        h.modelPath = p->stringOr("model_path");
        h.schemaFile = p->stringOr("schema_file");
        h.schemaVersion = p->stringOr("schema_version");
        h.runtimeVersion = p->stringOr("runtime_version");
    }
    h.attachedMidRun = parsed_.boolOr("attached_mid_run", false);
    if (const json::Value* v = parsed_.find("sample_form"); v && v->isString()) {
        h.hasSampleForm = true;
        h.sampleForm = v->text();
    }
    if (const json::Value* p = parsed_.find("subscription")) {
        h.topic = p->stringOr("topic");
        h.backpressurePolicy = p->stringOr("backpressure_policy");
        h.queueSize = p->integerOr("queue_size");
    }
    if (const json::Value* p = parsed_.find("limits"); p && p->isObject()) {
        h.hasLimits = true;
        h.maxBytes = p->integerOr("max_bytes");
        h.maxSamples = p->integerOr("max_samples");
        h.onSizeLimit = p->stringOr("on_size_limit");
    }
    h.part = parsed_.integerOr("part", 0);
    if (const json::Value* v = parsed_.find("continues_from"); v && v->isString()) {
        h.hasContinuesFrom = true;
        h.continuesFrom = v->text();
    }
    readSchemas(parsed_, lineNo);

    sawHeader_ = true;
    if (sink_) {
        RecordView view;
        view.type = "header";
        view.record = &parsed_;
        view.line = lineNo;
        view.segment = SegmentKey{h.part, -1};
        sink_->onRecord(view);
    }
    return true;
}

void Reader::readSchemas(const json::Value& obj, std::size_t lineNo) {
    const json::Value* arr = obj.find("schemas");
    if (!arr || !arr->isArray()) {
        note(Code::MissingRequiredKey, lineNo, "header has no schemas array");
        return;
    }
    for (const json::Value& e : arr->elements()) {
        SchemaDecl s;
        s.messageName = e.stringOr("message_name");
        s.topic = e.stringOr("topic");
        if (const json::Value* h = e.find("schema_hash"); h && h->isNumber()) {
            s.schemaHashText = h->raw();
            (void)h->asInteger(s.schemaHash);
        }
        s.messageId = e.integerOr("message_id");
        s.wireVersion = e.integerOr("wire_version");
        if (const json::Value* f = e.find("fields"); f && f->isArray()) {
            for (const json::Value& fd : f->elements()) {
                FieldDecl d;
                d.name = fd.stringOr("name");
                d.type = fd.stringOr("type");
                d.size = fd.integerOr("size", 1);
                s.fieldIndex.emplace(d.name, s.fields.size());
                s.fields.push_back(d);
            }
        }
        r_.header.schemaIndex.emplace(s.messageName, r_.header.schemas.size());
        r_.header.schemas.push_back(std::move(s));
    }
}

bool Reader::line(const std::string& rawIn, std::size_t lineNo) {
    // §2: LF, never CRLF. A stray CR is stripped rather than made fatal — the record itself is
    // perfectly readable, and refusing a whole capture over a line terminator would be a reader
    // being pedantic about the producer's job at the cost of the analyst's.
    std::string raw = rawIn;
    while (!raw.empty() && (raw.back() == '\r' || raw.back() == '\n')) raw.pop_back();

    ++r_.tallies.lines;

    if (!sawHeader_) {
        return header(raw, lineNo);
    }

    if (raw.empty()) {
        note(Code::BlankLine, lineNo, "blank line; the format has none (§2)");
        return true;
    }
    if (sawTrailer_) {
        note(Code::RecordsAfterTrailer, lineNo,
             "a record follows the trailer, which is the last line of a complete capture (§11)");
        return true;
    }

    json::ParseError err;
    if (!json::parse(raw, parsed_, err) || !parsed_.isObject()) {
        note(Code::MalformedLine, lineNo,
             "column " + num(static_cast<long long>(err.column)) + ": "
                 + (err.message.empty() ? "not a JSON object" : err.message));
        return true;
    }
    const json::Value* t = parsed_.find("type");
    if (!t || !t->isString()) {
        note(Code::MissingRequiredKey, lineNo, "record has no string type key");
        return true;
    }
    dispatch(parsed_, t->text(), lineNo);
    return true;
}

void Reader::dispatch(const json::Value& obj, const std::string& type, std::size_t lineNo) {
    if (type == "sample")             sample(obj, lineNo);
    else if (type == "segment_open")  segmentOpen(obj, lineNo);
    else if (type == "segment_close") segmentClose(obj, lineNo);
    else if (type == "entity_add")    entityAdd(obj, lineNo);
    else if (type == "entity_remove") entityRemove(obj, lineNo);
    else if (type == "verdict")       verdict(obj, lineNo);
    else if (type == "trailer")       trailer(obj, lineNo);
    else if (type == "header") {
        note(Code::UnknownRecordType, lineNo, "a second header record; there is exactly one (§4)");
    } else {
        // §3: a reader must ignore an unrecognised type rather than fail on it — but the record
        // vocabulary is closed within a version (§4), so meeting one in a version-matched file
        // is a producer defect and is reported. Reported, and skipped: both halves matter.
        note(Code::UnknownRecordType, lineNo,
             "type \"" + type + "\" is not one of the eight types of " + kFormatVersion);
        return;
    }

    if (sink_) {
        RecordView view;
        view.type = type;
        view.record = &obj;
        view.line = lineNo;
        view.segment = SegmentKey{r_.header.part, obj.integerOr("segment", -1)};
        sink_->onRecord(view);
    }
}

void Reader::segmentOpen(const json::Value& obj, std::size_t lineNo) {
    double t = 0.0;
    long long ordinal = 0;
    if (!envelope(obj, "segment_open", lineNo, t, ordinal)) return;

    if (openSegment_ >= 0) {
        note(Code::UnbalancedSegment, lineNo,
             "segment " + num(ordinal) + " opens while segment " + num(openSegment_)
                 + " is still open; there is exactly one segment_close per segment_open (§7)");
    }
    if (ordinal <= highestOpened_) {
        note(Code::SegmentOrdinalNotIncreasing, lineNo,
             "segment ordinal " + num(ordinal) + " does not exceed " + num(highestOpened_)
                 + "; ordinals strictly increase and are never reused within a file (§7)");
    }
    highestOpened_ = ordinal > highestOpened_ ? ordinal : highestOpened_;
    openSegment_ = ordinal;

    SegmentStats& s = segmentFor(ordinal);
    s.opened = true;
    s.scenario = obj.stringOr("scenario");
    ++r_.tallies.segmentsOpened;
}

void Reader::segmentClose(const json::Value& obj, std::size_t lineNo) {
    double t = 0.0;
    long long ordinal = 0;
    if (!envelope(obj, "segment_close", lineNo, t, ordinal)) return;

    SegmentStats& s = segmentFor(ordinal);
    if (!s.opened) {
        note(Code::UnbalancedSegment, lineNo,
             "segment_close for segment " + num(ordinal) + " with no matching segment_open (§7)");
    }
    if (openSegment_ != ordinal) {
        note(Code::UnbalancedSegment, lineNo,
             "segment_close for segment " + num(ordinal) + " while the open segment is "
                 + (openSegment_ < 0 ? std::string("none") : num(openSegment_)));
    }
    s.closed = true;
    s.closeReason = obj.stringOr("reason");
    if (!s.closeReason.empty() && !inClosedSet(s.closeReason, kSegmentCloseReasons)) {
        // Unlike entity_remove.reason, this vocabulary IS closed (§7), and changing it is a
        // version bump (§13). So an unexpected value in a version-matched file is a producer
        // defect, not an open enumeration.
        note(Code::ClosedVocabularyViolation, lineNo,
             "segment_close.reason \"" + s.closeReason + "\" is not in the closed set "
             "{scenario_unloaded, host_lost, shutdown, size_limit} (§7)");
    }
    ++r_.tallies.segmentsClosed;
    openSegment_ = -1;
}

void Reader::sample(const json::Value& obj, std::size_t lineNo) {
    double t = 0.0;
    long long seg = 0;
    if (!envelope(obj, "sample", lineNo, t, seg)) return;

    const json::Value* e = obj.find("entity");
    const json::Value* m = obj.find("message");
    long long occupancy = 0;
    const json::Value* o = obj.find("occupancy");
    if (!e || !e->isString() || !m || !m->isString() || !o || !o->asInteger(occupancy)) {
        note(Code::MissingRequiredKey, lineNo,
             "sample needs entity, occupancy and message (§8)");
        return;
    }

    // §7: no sample record appears outside an open segment. If a reader sees one, the file is
    // malformed and the reader should say so.
    if (openSegment_ < 0) {
        note(Code::SampleOutsideSegment, lineNo,
             "sample in segment " + num(seg) + " with no segment open (§7)");
    } else if (seg != openSegment_) {
        note(Code::SampleOutsideSegment, lineNo,
             "sample claims segment " + num(seg) + " while segment " + num(openSegment_)
                 + " is the open one");
    }

    SegmentStats& s = segmentFor(seg);
    ++s.samples;
    ++r_.tallies.samples;

    // The segment's time extent is [first sample, last sample], never the boundary records —
    // both of those read 0.0 on a reloaded scenario, so a duration computed from them is zero
    // for a run of any length (§5.1).
    if (!s.hasSamples) {
        s.hasSamples = true;
        s.firstSampleTimeS = t;
        s.lastSampleTimeS = t;
        s.distinctSimTimes = 1;
        segmentHasTime_[seg] = true;
        segmentLastTime_[seg] = t;
    } else {
        const double prev = segmentLastTime_[seg];
        if (t < prev) {
            // §5.1's positive rule: within a segment, sample records are non-decreasing. It is
            // checked rather than assumed, because the distinct-time count below is derived
            // from it — a reader that assumes an invariant and computes from it should say so
            // when the invariant fails rather than quietly report a smaller number.
            note(Code::SampleTimeDecreased, lineNo,
                 "sim_time_s " + dbl(t) + " follows " + dbl(prev) + " in segment " + num(seg)
                     + "; sample records are non-decreasing within a segment (§5.1)");
        } else if (t > prev) {
            ++s.distinctSimTimes;
        }
        segmentLastTime_[seg] = t;
        s.lastSampleTimeS = t;
    }

    const EntityKey key{seg, e->text(), occupancy};
    EntityState& st = entities_[key];
    if (!st.counted) { st.counted = true; ++s.distinctEntityKeys; }
    ++st.samples;
    // §8.1: within one (entity, occupancy) pair, no sample ever appears after that pair's
    // entity_remove. That invariant does hold, and it is the one worth asserting. Samples
    // resuming under a *higher* occupancy are legitimate and are a different key entirely.
    if (st.removed) {
        note(Code::SampleAfterRemove, lineNo,
             "sample for (" + e->text() + ", occupancy " + num(occupancy) + ") in segment "
                 + num(seg) + " after that pair's entity_remove (§8.1)");
    }
    if (st.haveTime && st.lastTime == t) {
        ++st.runAtLastTime;
    } else {
        st.haveTime = true;
        st.lastTime = t;
        st.runAtLastTime = 1;
    }
    if (st.runAtLastTime > st.maxRun) st.maxRun = st.runAtLastTime;
    if (st.maxRun > s.maxSamplesPerEntityPerSimTime) {
        s.maxSamplesPerEntityPerSimTime = st.maxRun;
    }

    if (opt_.verifyFields) verifyFields(obj, m->text(), lineNo);
}

void Reader::verifyFields(const json::Value& obj, const std::string& message, std::size_t lineNo) {
    const json::Value* fields = obj.find("fields");
    if (!fields || !fields->isObject()) {
        note(Code::MissingRequiredKey, lineNo, "sample has no fields object (§8.2)");
        return;
    }
    auto si = r_.header.schemaIndex.find(message);
    if (si == r_.header.schemaIndex.end()) {
        note(Code::UnknownMessage, lineNo,
             "message \"" + message + "\" is not declared in header.schemas (§6.5)");
        return;
    }
    const SchemaDecl& schema = r_.header.schemas[si->second];

    bool first = true;
    std::size_t lastIndex = 0;
    for (const json::Member& mem : fields->members()) {
        auto fi = schema.fieldIndex.find(mem.first);
        if (fi == schema.fieldIndex.end()) {
            // §8.2: a reader should reject a `fields` key the schema does not declare as a
            // producer defect. It is the one of the three sparse cases that is an error — the
            // other two, declared-and-present and declared-and-never-sent, are both normal.
            note(Code::UndeclaredField, lineNo,
                 "field \"" + mem.first + "\" in a " + message
                     + " sample is not declared in that message's schema (§8.2)");
            continue;
        }
        const std::size_t index = fi->second;
        if (!first && index <= lastIndex) {
            // §8.2: the keys appear in the order header.schemas[].fields declares them,
            // restricted to the fields actually present. It is a guarantee about the bytes on
            // disk, and it is checkable only with an order-preserving parse — which is why this
            // project has one.
            note(Code::FieldOrderMismatch, lineNo,
                 "field \"" + mem.first + "\" (declared at index " + num(static_cast<long long>(index))
                     + ") follows a field declared at index " + num(static_cast<long long>(lastIndex))
                     + "; sample.fields follows the schema's declaration order (§8.2)");
        }
        lastIndex = index;
        first = false;

        const FieldDecl& decl = schema.fields[index];
        if (decl.size > 1) {
            if (!mem.second.isArray()) {
                note(Code::FieldTypeMismatch, lineNo,
                     "field \"" + mem.first + "\" is declared " + decl.type + "[" + num(decl.size)
                         + "] and is not an array (§8.3)");
                continue;
            }
            const long long len = static_cast<long long>(mem.second.elements().size());
            if (len != decl.size) {
                // §8.3: the length is the publisher's, so a reader must accept an array of any
                // length and should report a mismatch rather than assume one.
                note(Code::ArrayLengthMismatch, lineNo,
                     "field \"" + mem.first + "\" is declared size " + num(decl.size)
                         + " and carries " + num(len) + " elements (§8.3)");
            }
            for (const json::Value& el : mem.second.elements()) {
                verifyScalar(decl, el, lineNo, mem.first + "[]");
            }
        } else {
            verifyScalar(decl, mem.second, lineNo, mem.first);
        }
    }
}

void Reader::verifyScalar(const FieldDecl& decl, const json::Value& v, std::size_t lineNo,
                          const std::string& where) {
    if (decl.type == "double") {
        if (v.isNumber()) return;   // `5` is a valid encoding of 5.0; type from the schema (§8.3)
        // §8.3: non-finite doubles have no JSON number spelling and are written as one of three
        // quoted tokens. A reader must accept a string in a double-typed field when and only
        // when it is one of those three.
        if (v.isString() && (v.text() == "nan" || v.text() == "inf" || v.text() == "-inf")) return;
        note(Code::FieldTypeMismatch, lineNo,
             "field \"" + where + "\" is declared double and is neither a number nor one of "
             "\"nan\", \"inf\", \"-inf\" (§8.3)");
        return;
    }
    if (decl.type == "int") {
        if (v.isNumber() && v.isIntegralText()) return;
        note(Code::FieldTypeMismatch, lineNo,
             "field \"" + where + "\" is declared int and is not an integral number (§8.3)");
        return;
    }
    if (decl.type == "bool") {
        if (v.isBool()) return;
        note(Code::FieldTypeMismatch, lineNo,
             "field \"" + where + "\" is declared bool and is not true or false (§8.3)");
        return;
    }
    if (decl.type == "string") {
        if (v.isString()) return;
        note(Code::FieldTypeMismatch, lineNo,
             "field \"" + where + "\" is declared string and is not a string (§8.3)");
        return;
    }
    // A declared type this reader does not know is not an error in the sample: the header
    // describes the fields, and a platform release may add a type. Nothing is checked, and
    // nothing is invented.
}

void Reader::entityAdd(const json::Value& obj, std::size_t lineNo) {
    double t = 0.0;
    long long seg = 0;
    if (!envelope(obj, "entity_add", lineNo, t, seg)) return;
    const json::Value* e = obj.find("entity");
    long long occupancy = 0;
    const json::Value* o = obj.find("occupancy");
    if (!e || !e->isString() || !o || !o->asInteger(occupancy)) {
        note(Code::MissingRequiredKey, lineNo, "entity_add needs entity and occupancy (§9)");
        return;
    }
    SegmentStats& s = segmentFor(seg);
    ++s.entityAdds;
    ++r_.tallies.entityAdds;

    const EntityKey key{seg, e->text(), occupancy};
    EntityState& st = entities_[key];
    if (!st.counted) { st.counted = true; ++s.distinctEntityKeys; }
    st.removed = false;
}

void Reader::entityRemove(const json::Value& obj, std::size_t lineNo) {
    double t = 0.0;
    long long seg = 0;
    if (!envelope(obj, "entity_remove", lineNo, t, seg)) return;
    const json::Value* e = obj.find("entity");
    long long occupancy = 0;
    const json::Value* o = obj.find("occupancy");
    if (!e || !e->isString() || !o || !o->asInteger(occupancy)) {
        note(Code::MissingRequiredKey, lineNo, "entity_remove needs entity and occupancy (§9)");
        return;
    }
    // §9: `reason` is verbatim from the platform and the list is explicitly NOT closed. A reader
    // must accept any string here. So nothing checks it — deliberately, and this comment is the
    // record of that being a decision rather than an omission.

    SegmentStats& s = segmentFor(seg);
    ++s.entityRemoves;
    ++r_.tallies.entityRemoves;

    const EntityKey key{seg, e->text(), occupancy};
    auto it = entities_.find(key);
    if (it == entities_.end()) {
        // In a late-attached capture the roster's origin is not in the file at all, so adds and
        // removes legitimately do not balance and a remove without an add is expected rather
        // than wrong (§6.3). The file says which case it is; this reader believes it.
        if (!r_.header.attachedMidRun) {
            note(Code::UnmatchedEntityRemove, lineNo,
                 "entity_remove for (" + e->text() + ", occupancy " + num(occupancy)
                     + ") in segment " + num(seg) + " with no matching entity_add, in a capture "
                       "whose header says attached_mid_run is false (§9)");
        }
        EntityState fresh;
        fresh.removed = true;
        entities_.emplace(key, fresh);
        return;
    }
    it->second.removed = true;
}

void Reader::verdict(const json::Value& obj, std::size_t lineNo) {
    double t = 0.0;
    long long seg = 0;
    if (!envelope(obj, "verdict", lineNo, t, seg)) return;
    const json::Value* id = obj.find("condition_id");
    const json::Value* met = obj.find("met");
    if (!id || !id->isString() || !met || !met->isBool()) {
        note(Code::MissingRequiredKey, lineNo, "verdict needs condition_id and met (§10)");
        return;
    }
    ++segmentFor(seg).verdicts;
    ++r_.tallies.verdicts;
}

void Reader::trailer(const json::Value& obj, std::size_t lineNo) {
    Trailer& tr = r_.trailer;
    if (const json::Value* v = obj.find("sim_time_s"); v && v->isNumber()) tr.simTimeS = v->number();
    tr.endReason = obj.stringOr("end_reason");
    if (!tr.endReason.empty() && !inClosedSet(tr.endReason, kEndReasons)) {
        note(Code::ClosedVocabularyViolation, lineNo,
             "trailer.end_reason \"" + tr.endReason + "\" is not in the closed set "
             "{shutdown, host_lost, size_limit, replay_end} (§11)");
    }
    if (const json::Value* c = obj.find("counts"); c && c->isObject()) {
        tr.counts.segments = c->integerOr("segments");
        tr.counts.samples = c->integerOr("samples");
        tr.counts.entitiesAdded = c->integerOr("entities_added");
        tr.counts.entitiesRemoved = c->integerOr("entities_removed");
        tr.counts.verdicts = c->integerOr("verdicts");
    } else {
        note(Code::MissingRequiredKey, lineNo, "trailer has no counts object (§11)");
    }
    if (const json::Value* d = obj.find("drops"); d && d->isObject()) {
        readOptional(*d, "samples_not_recorded", tr.drops.samplesNotRecorded);
        readOptional(*d, "events_not_recorded", tr.drops.eventsNotRecorded);
        readOptional(*d, "samples_orphaned", tr.drops.samplesOrphaned);
        readOptional(*d, "samples_unnamed", tr.drops.samplesUnnamed);
        readOptional(*d, "samples_untimed", tr.drops.samplesUntimed);
    }
    if (const json::Value* b = obj.find("bus_metrics"); b && b->isObject()) {
        readOptional(*b, "schema_hash_drops", tr.busMetrics.schemaHashDrops);
        readOptional(*b, "message_id_drops", tr.busMetrics.messageIdDrops);
        readOptional(*b, "decode_failures", tr.busMetrics.decodeFailures);
        readOptional(*b, "missing_schema_passthrough", tr.busMetrics.missingSchemaPassthrough);
        readOptional(*b, "legacy_payload_passthrough", tr.busMetrics.legacyPayloadPassthrough);
        readOptional(*b, "messages_dropped", tr.busMetrics.messagesDropped);
        readOptional(*b, "dropped_by_backpressure", tr.busMetrics.droppedByBackpressure);
        readOptional(*b, "dropped_by_queue_overflow", tr.busMetrics.droppedByQueueOverflow);
        readOptional(*b, "dropped_by_rate_limiting", tr.busMetrics.droppedByRateLimiting);
    }
    if (const json::Value* v = obj.find("continued_in"); v && v->isString()) {
        tr.hasContinuedIn = true;
        tr.continuedIn = v->text();
    }
    r_.hasTrailer = true;
    sawTrailer_ = true;
}

void Reader::finish() {
    if (r_.rejected) return;

    if (!sawHeader_) {
        reject(Code::NotACapture, "the file is empty; a capture is at least a header and a trailer");
        return;
    }

    // §3 step 5: a file whose last line is not a trailer was truncated. Everything before the
    // truncation point is still valid and may be used — so this is a defect and not a rejection,
    // and the statistics below are still filled in.
    if (!sawTrailer_) {
        note(Code::TruncatedNoTrailer, static_cast<std::size_t>(r_.tallies.lines),
             "the last line is not a trailer; the capture is truncated. The "
                 + num(r_.tallies.lines) + " lines read remain valid (§3)");
    }
    if (openSegment_ >= 0) {
        note(Code::UnbalancedSegment, static_cast<std::size_t>(r_.tallies.lines),
             "segment " + num(openSegment_) + " was never closed; there is exactly one "
                 "segment_close per segment_open (§7)");
    }

    // The segment list is built from `segment_open` and `segment_close` as well as from the
    // records that carry samples. A list built from sample records alone loses a segment that
    // was opened and closed with nothing in it — measured at M2 in 15 of 20 runs — and then
    // disagrees with trailer.counts.segments for a reason that has nothing to do with the file.
    for (auto& kv : segments_) {
        SegmentStats& s = kv.second;
        s.key.part = r_.header.part;
        if (!s.hasSamples) {
            // The frozen-clock test needs more than one sample for one key at one sim_time_s
            // before it can fire. With no samples it cannot, and calling such a segment
            // "running" would be asserting the result of a test that never ran.
            s.clock = ClockClass::Indeterminate;
        } else if (s.maxSamplesPerEntityPerSimTime > 1) {
            s.clock = ClockClass::Frozen;
        } else if (s.distinctSimTimes > 1) {
            s.clock = ClockClass::Running;
        } else {
            // Samples exist but they all share one sim_time_s and no key repeats it: one frame,
            // or a teardown tail too short to trip the test. Either way the test did not fire.
            s.clock = ClockClass::Indeterminate;
        }
        r_.segments.push_back(s);
    }

    if (r_.hasTrailer) {
        const Counts& c = r_.trailer.counts;
        const Tallies& t = r_.tallies;
        // §11: a reader should count the records itself and compare. A disagreement means the
        // file was truncated, or the producer is defective; either way it is worth reporting.
        if (c.segments != t.segmentsOpened || c.samples != t.samples
            || c.entitiesAdded != t.entityAdds || c.entitiesRemoved != t.entityRemoves
            || c.verdicts != t.verdicts) {
            note(Code::CountsDisagree, static_cast<std::size_t>(r_.tallies.lines),
                 "trailer.counts says segments=" + num(c.segments) + " samples=" + num(c.samples)
                     + " entities_added=" + num(c.entitiesAdded) + " entities_removed="
                     + num(c.entitiesRemoved) + " verdicts=" + num(c.verdicts)
                     + "; this reader counted segments=" + num(t.segmentsOpened) + " samples="
                     + num(t.samples) + " entities_added=" + num(t.entityAdds)
                     + " entities_removed=" + num(t.entityRemoves) + " verdicts="
                     + num(t.verdicts));
        }
    }
}

} // namespace

bool peekFormatVersion(const std::string& firstLine, std::string& versionOut) {
    static const char kPrefix[] = "{\"format_version\":\"";
    const std::size_t n = sizeof kPrefix - 1;
    if (firstLine.compare(0, n, kPrefix) != 0) return false;
    const std::size_t end = firstLine.find('"', n);
    if (end == std::string::npos) return false;
    versionOut.assign(firstLine, n, end - n);
    return true;
}

ReadResult readLines(const std::vector<std::string>& lines, const std::string& label,
                     const ReadOptions& options, RecordSink* sink) {
    Reader reader(options, sink);
    reader.result().path = label;
    long long bytes = 0;
    for (const std::string& l : lines) bytes += static_cast<long long>(l.size()) + 1;
    reader.result().bytes = bytes;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (!reader.line(lines[i], i + 1)) break;
    }
    reader.finish();
    return std::move(reader.result());
}

ReadResult readFile(const std::string& path, const ReadOptions& options, RecordSink* sink) {
    Reader reader(options, sink);
    reader.result().path = path;

    // Binary, because §2 fixes the line terminator at LF on every platform and a text-mode read
    // on Windows would silently translate what the format says never to translate.
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        reader.result().rejected = true;
        reader.result().rejectCode = Code::FileUnreadable;
        reader.result().rejectDetail = "could not open " + path;
        return std::move(reader.result());
    }
    std::fseek(f, 0, SEEK_END);
    reader.result().bytes = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);

    std::string line;
    line.reserve(4096);
    std::size_t lineNo = 0;
    char buf[65536];
    bool stopped = false;
    std::size_t got = 0;
    while (!stopped && (got = std::fread(buf, 1, sizeof buf, f)) > 0) {
        for (std::size_t i = 0; i < got; ++i) {
            if (buf[i] == '\n') {
                if (!reader.line(line, ++lineNo)) { stopped = true; break; }
                line.clear();
            } else {
                line.push_back(buf[i]);
            }
        }
    }
    // §2: every record line, including the last, is LF-terminated. A tail with no terminator is
    // a record all the same — it is what a capture cut off by a full disk looks like — so it is
    // read rather than discarded, and the missing trailer is what reports the truncation.
    if (!stopped && !line.empty()) reader.line(line, ++lineNo);

    std::fclose(f);
    reader.finish();
    return std::move(reader.result());
}

} // namespace ext17::capture

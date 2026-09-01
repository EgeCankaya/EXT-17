// EXT-17 â€” comparing two captures of one configuration. See Compare.h for what this gets right
// on purpose and why each of those is a way to be wrong.
//
// Links nothing. Never throws.
#include "Compare.h"

#include "../capture/CaptureSet.h"
#include "../common/JsonParse.h"

#include <cctype>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace ext17::compare {
namespace {

// --- A digest over one sample's values --------------------------------------------------------
//
// Two independent FNV-1a-64 streams over a canonical, type-tagged encoding of `sample.fields`.
// 128 bits, so that "the digests agree" and "the values agree" are the same statement for any
// capture this project will ever meet; the alternative is retaining every sample's text, which
// costs about 24 MB per run against this one's 3 MB and buys nothing, because a divergence is
// re-read from the files by name and line anyway (see secondPass below).
//
// It is a pure function of the bytes. No clock, no address, no container with unspecified
// iteration order, no locale-dependent conversion â€” CR-DET-2, all three hazards, by construction.
struct Digest {
    std::uint64_t a = 0xcbf29ce484222325ull;
    std::uint64_t b = 0x9ae16a3b2f90404full;

    void byte(unsigned char c) {
        a = (a ^ c) * 0x100000001b3ull;
        b = (b + c) * 0x9ddfea08eb382d69ull;
        b ^= (b >> 29);
    }
    void feed(const char* s, std::size_t n) {
        for (std::size_t i = 0; i < n; ++i) { byte(static_cast<unsigned char>(s[i])); }
    }
    void feed(const std::string& s) { feed(s.data(), s.size()); }
    bool operator==(const Digest& o) const { return a == o.a && b == o.b; }
    bool operator!=(const Digest& o) const { return !(*this == o); }
};

// The canonical encoding. Member order is the file's own, which Â§8.2 makes normative, so the
// encoding preserves it rather than sorting â€” a sort here would silently make two captures with
// differently-ordered fields compare equal, and field order is a thing the reader already
// reports as a defect.
//
// Numbers are fed as their **verbatim text** (`raw()`), never as a formatted double. Strings are
// fed unescaped, tagged, so that two different escapings of one string are one value and a
// string that happens to look like a number is not one.
void encodeValue(const json::Value& v, Digest& d) {
    switch (v.type()) {
        case json::Type::Null:   d.byte('0'); break;
        case json::Type::Bool:   d.byte('B'); d.byte(v.boolean() ? 1 : 0); break;
        case json::Type::Number: d.byte('N'); d.feed(v.raw()); break;
        case json::Type::String: d.byte('S'); d.feed(v.text()); break;
        case json::Type::Array:
            d.byte('[');
            for (const json::Value& e : v.elements()) { encodeValue(e, d); d.byte(0x1F); }
            d.byte(']');
            break;
        case json::Type::Object:
            d.byte('{');
            for (const json::Member& m : v.members()) {
                d.feed(m.first);
                d.byte(0x1D);
                encodeValue(m.second, d);
                d.byte(0x1F);
            }
            d.byte('}');
            break;
    }
}

// --- One capture, indexed for comparison ------------------------------------------------------

struct SampleRef {
    std::string simText;      // verbatim `sim_time_s`, exactly as the file wrote it
    double simTime = 0.0;     // for ordering the merge only. Never for deciding a match
    Digest digest;
    std::size_t line = 0;     // 1-based, so a reader can go and look
    long long part = 0;
};

using KeySamples = std::map<EntityKey, std::vector<SampleRef>>;

struct CaptureIndex {
    capture::SetResult set;
    std::map<capture::SegmentKey, KeySamples> samples;
    long long indexedSamples = 0;
    std::vector<std::string> partPaths;
};

class IndexSink : public capture::RecordSink {
public:
    explicit IndexSink(CaptureIndex& index) : index_(index) {}

    void onRecord(const capture::RecordView& view) override {
        if (view.type != "sample" || view.record == nullptr) { return; }
        const json::Value* t = view.record->find("sim_time_s");
        const json::Value* e = view.record->find("entity");
        const json::Value* o = view.record->find("occupancy");
        const json::Value* f = view.record->find("fields");
        if (t == nullptr || e == nullptr || o == nullptr) { return; }

        SampleRef ref;
        ref.simText = t->raw();
        ref.simTime = t->number();
        ref.line = view.line;
        ref.part = view.segment.part;
        // The message type participates in the digest: two samples of different message types at
        // one instant for one entity are different values, and a capture may legitimately carry
        // more than one message type (Â§6.5).
        if (const json::Value* m = view.record->find("message")) {
            ref.digest.byte('M');
            ref.digest.feed(m->text());
            ref.digest.byte(0x1E);
        }
        if (f != nullptr) { encodeValue(*f, ref.digest); }

        EntityKey key;
        key.entity = e->text();
        key.occupancy = o->integerOr("", 0);
        if (const json::Value* oc = view.record->find("occupancy")) {
            long long n = 0;
            if (oc->asInteger(n)) { key.occupancy = n; }
        }
        index_.samples[view.segment][key].push_back(std::move(ref));
        ++index_.indexedSamples;
    }

private:
    CaptureIndex& index_;
};

CaptureIndex indexCapture(const std::string& path) {
    CaptureIndex index;
    IndexSink sink(index);
    index.set = capture::readSet(path, {}, &sink);
    for (const capture::ReadResult& part : index.set.parts) { index.partPaths.push_back(part.path); }
    return index;
}

// --- Preconditions ----------------------------------------------------------------------------

// covers_whole_run, by Â§6.7's own rule for telling a stopped capture from a finished one.
bool coversWholeRun(const capture::SetResult& set) {
    if (set.parts.empty()) { return false; }
    const capture::ReadResult& last = set.parts.back();
    return last.hasTrailer && last.trailer.endReason != "size_limit";
}

// Non-zero is a refusal, and **absent is also a refusal** â€” the format is explicit that a
// missing counter is unknown rather than zero (Â§11), and tenet 3 is that absence is not
// evidence. A capture whose completeness cannot be established is not one to diff.
enum class DropState { Zero, NonZero, Unknown };

DropState samplesNotRecordedState(const capture::SetResult& set, long long& total) {
    total = 0;
    bool anyUnknown = false;
    for (const capture::ReadResult& part : set.parts) {
        if (!part.hasTrailer) { anyUnknown = true; continue; }
        const capture::OptionalCount& c = part.trailer.drops.samplesNotRecorded;
        if (!c.present) { anyUnknown = true; continue; }
        total += c.value;
    }
    if (total > 0) { return DropState::NonZero; }
    return anyUnknown ? DropState::Unknown : DropState::Zero;
}

std::string schemaFingerprint(const capture::Header& h) {
    std::string s;
    for (const capture::SchemaDecl& d : h.schemas) {
        s += d.messageName;
        s += "|" + d.topic + "|" + d.schemaHashText + "|" + std::to_string(d.messageId) + "|"
             + std::to_string(d.wireVersion) + "|";
        for (const capture::FieldDecl& f : d.fields) {
            s += f.name + ":" + f.type + ":" + std::to_string(f.size) + ",";
        }
        s += ";";
    }
    return s;
}

std::string subscriptionFingerprint(const capture::Header& h) {
    return h.topic + "|" + h.backpressurePolicy + "|" + std::to_string(h.queueSize);
}

std::string limitsFingerprint(const capture::Header& h) {
    if (!h.hasLimits) { return "(absent)"; }
    return std::to_string(h.maxBytes) + "|" + std::to_string(h.maxSamples) + "|" + h.onSizeLimit;
}

// --- The byte comparison ------------------------------------------------------------------------

// Â§14: `platform.model_path` is "the one host-dependent field". This masks its value and nothing
// else. It is a textual mask rather than a re-serialisation on purpose: re-writing the header
// through a JSON writer would normalise things the producer wrote deliberately, and a byte
// comparison whose two sides have both been through our formatter is not a byte comparison.
bool maskModelPath(std::string& line) {
    const char* kKey = "\"model_path\":\"";
    const std::size_t at = line.find(kKey);
    if (at == std::string::npos) { return false; }
    const std::size_t vstart = at + std::strlen(kKey);
    std::size_t i = vstart;
    while (i < line.size()) {
        if (line[i] == '\\') { i += 2; continue; }
        if (line[i] == '"') { break; }
        ++i;
    }
    if (i >= line.size()) { return false; }
    line.replace(vstart, i - vstart, "<model_path excluded per format-v1 s14>");
    return true;
}

bool readFirstLine(const std::string& path, std::string& out, std::size_t& bytesConsumed) {
    std::ifstream in(path, std::ios::binary);
    if (!in) { return false; }
    out.clear();
    char c = 0;
    while (in.get(c)) {
        out.push_back(c);
        if (c == '\n') { break; }
    }
    bytesConsumed = out.size();
    return !out.empty();
}

void compareOnePair(const std::string& pathA, const std::string& pathB, ByteResult& r,
                    long long partIndex) {
    std::string headA, headB;
    std::size_t offA = 0, offB = 0;
    const bool gotA = readFirstLine(pathA, headA, offA);
    const bool gotB = readFirstLine(pathB, headB, offB);
    if (!gotA || !gotB) {
        r.identical = false;
        if (r.firstDifferingOffset < 0) {
            r.firstDifferingOffset = 0;
            r.firstDifferingLine = 1;
            r.firstDifferingPart = partIndex;
        }
        return;
    }

    const bool rawHeadersEqual = (headA == headB);
    std::string maskedA = headA, maskedB = headB;
    const bool maskedOk = maskModelPath(maskedA) && maskModelPath(maskedB);
    r.modelPathExcluded = r.modelPathExcluded || maskedOk;
    if (partIndex == 0) { r.headersIdentical = rawHeadersEqual; }
    if (!rawHeadersEqual && maskedA == maskedB) { r.modelPathDiffered = true; }

    // Line 1, with the one excluded field masked in both.
    if (maskedA != maskedB) {
        std::size_t k = 0;
        while (k < maskedA.size() && k < maskedB.size() && maskedA[k] == maskedB[k]) { ++k; }
        r.identical = false;
        if (r.firstDifferingOffset < 0) {
            r.firstDifferingOffset = static_cast<long long>(k);
            r.firstDifferingLine = 1;
            r.firstDifferingPart = partIndex;
        }
        return;
    }

    // Everything after line 1, byte for byte. Nothing is excluded here and nothing ever will be:
    // ADR-1 forbids normalising a comparison until it passes, and the exclusion above is the one
    // the specification itself names.
    std::ifstream a(pathA, std::ios::binary), b(pathB, std::ios::binary);
    if (!a || !b) { r.identical = false; return; }
    a.seekg(static_cast<std::streamoff>(offA));
    b.seekg(static_cast<std::streamoff>(offB));

    constexpr std::size_t kChunk = 65536;
    std::vector<char> ba(kChunk), bb(kChunk);
    long long offset = static_cast<long long>(offA);   // absolute offset into A
    long long line = 2;
    for (;;) {
        a.read(ba.data(), static_cast<std::streamsize>(kChunk));
        b.read(bb.data(), static_cast<std::streamsize>(kChunk));
        const std::streamsize na = a.gcount(), nb = b.gcount();
        const std::streamsize n = na < nb ? na : nb;
        for (std::streamsize i = 0; i < n; ++i) {
            if (ba[static_cast<std::size_t>(i)] != bb[static_cast<std::size_t>(i)]) {
                r.identical = false;
                if (r.firstDifferingOffset < 0) {
                    r.firstDifferingOffset = offset + i;
                    r.firstDifferingLine = line;
                    r.firstDifferingPart = partIndex;
                }
                return;
            }
            if (ba[static_cast<std::size_t>(i)] == '\n') { ++line; }
        }
        if (na != nb) {
            // One ran out first: the files differ in length, which the sizes already say.
            r.identical = false;
            if (r.firstDifferingOffset < 0) {
                r.firstDifferingOffset = offset + n;
                r.firstDifferingLine = line;
                r.firstDifferingPart = partIndex;
            }
            return;
        }
        offset += n;
        if (na == 0) { break; }
    }
}

long long fileSize(const std::string& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) { return -1; }
    return static_cast<long long>(in.tellg());
}

// --- Pass two: the text of the records that actually differed ------------------------------------
//
// Only ever runs when there IS a difference, which on this platform is never, so the normal cost
// of CR-DET-3's detail is zero. It re-reads by line number rather than retaining 50 000 samples
// against the possibility.
std::string lineAt(const std::string& path, std::size_t wanted) {
    std::ifstream in(path, std::ios::binary);
    if (!in) { return {}; }
    std::string line;
    std::size_t n = 0;
    while (std::getline(in, line)) {
        if (++n == wanted) {
            if (!line.empty() && line.back() == '\r') { line.pop_back(); }
            return line;
        }
    }
    return {};
}

// One field's value as text, VERBATIM. A number renders as the characters the capture carried
// (§8.3), never through a conversion; an array renders as its elements in brackets, each of them
// verbatim in turn.
//
// **Arrays are the reason this function exists.** Until M6 this was
// `v.isNumber() ? v.raw() : v.text()`, and `Value::text()` is empty for an array — so every
// difference in `positionGeodetic`, `velocityNed` or `orientationYprRad` reported the field name
// correctly and then printed two empty values. Those are three of the four fields a divergence
// is most likely to be in, and CR-DET-3 and CR-REP-4 both require a failure to name the deciding
// VALUES and not only the field.
//
// It was never caught because it takes a real content-gate failure on a real pair to see it, and
// until M6 the content gate had never failed on one: M4's failing-gate evidence came from
// forcing `--gate-basis bytes`, which reports a byte offset and never reaches this code.
// Recorded as F-31.
std::string valueText(const json::Value& v) {
    if (v.isNumber()) { return v.raw(); }
    if (v.isString()) { return v.text(); }
    if (v.isBool()) { return v.boolean() ? "true" : "false"; }
    if (v.isNull()) { return "null"; }
    if (v.isArray()) {
        std::string out = "[";
        for (std::size_t i = 0; i < v.elements().size(); ++i) {
            if (i != 0) { out += ", "; }
            out += valueText(v.elements()[i]);
        }
        return out + "]";
    }
    if (v.isObject()) {
        std::string out = "{";
        bool first = true;
        for (const json::Member& m : v.members()) {
            if (!first) { out += ", "; }
            first = false;
            out += m.first + ": " + valueText(m.second);
        }
        return out + "}";
    }
    return "(unrenderable)";
}

// Name the first field whose value differs, so a failure attributes to a value rather than to a
// line. Falls back to naming the record itself when the difference is structural.
void nameFirstDifferingField(const std::string& lineA, const std::string& lineB, Difference& d) {
    json::Value va, vb;
    json::ParseError ea, eb;
    if (!json::parse(lineA, va, ea) || !json::parse(lineB, vb, eb)) {
        d.field = "(record)";
        d.valueA = lineA.substr(0, 200);
        d.valueB = lineB.substr(0, 200);
        return;
    }
    const json::Value* fa = va.find("fields");
    const json::Value* fb = vb.find("fields");
    if (fa == nullptr || fb == nullptr || !fa->isObject() || !fb->isObject()) {
        d.field = "(record)";
        d.valueA = lineA.substr(0, 200);
        d.valueB = lineB.substr(0, 200);
        return;
    }
    for (const json::Member& m : fa->members()) {
        const json::Value* other = fb->find(m.first.c_str());
        Digest da, db;
        encodeValue(m.second, da);
        if (other != nullptr) { encodeValue(*other, db); }
        if (other == nullptr || !(da == db)) {
            d.field = m.first;
            d.valueA = valueText(m.second);
            d.valueB = (other == nullptr) ? std::string("(absent)") : valueText(*other);
            return;
        }
    }
    for (const json::Member& m : fb->members()) {
        if (fa->find(m.first.c_str()) == nullptr) {
            d.field = m.first;
            d.valueA = "(absent)";
            d.valueB = valueText(m.second);
            return;
        }
    }
    d.field = "(envelope)";
}

const char* clockName(capture::ClockClass c) { return capture::name(c); }

// Count instants a `(entity, occupancy)` published more than once inside one segment, and how
// many of those repeats carried the same values. This is what tells a duplicated publication
// from a reset clock, and Â§5.1's exact test cannot: both make the segment `Frozen`.
void countDuplicatedInstants(const KeySamples& keys, long long& duplicated, long long& identical) {
    duplicated = 0;
    identical = 0;
    for (const auto& kv : keys) {
        const std::vector<SampleRef>& v = kv.second;
        // The reader has already checked that sim_time_s is non-decreasing within a segment
        // (Â§5.1), so a repeat is always adjacent and this needs no map and no sort.
        for (std::size_t i = 1; i < v.size(); ++i) {
            if (v[i].simText == v[i - 1].simText) {
                ++duplicated;
                if (v[i].digest == v[i - 1].digest) { ++identical; }
            }
        }
    }
}

std::string percent(double v) {
    // Fixed, locale-independent formatting of a percentage, done by integer arithmetic so that
    // no number in this project's compared output ever goes through a locale-sensitive
    // conversion. CR-DET-2, tested under a comma-decimal locale.
    long long scaled = static_cast<long long>(v * 1000000.0 + 0.5);   // 4 decimal places of %
    if (scaled < 0) { scaled = 0; }
    const long long whole = scaled / 10000;
    const long long frac = scaled % 10000;
    std::string f = std::to_string(frac);
    while (f.size() < 4) { f.insert(f.begin(), '0'); }
    return std::to_string(whole) + "." + f + "%";
}

}  // namespace

// --- Names -----------------------------------------------------------------------------------

const char* name(Verdict v) {
    switch (v) {
        case Verdict::Pass: return "pass";
        case Verdict::Fail: return "fail";
        case Verdict::Indeterminate: return "indeterminate";
        case Verdict::Refused: return "refused";
    }
    return "unknown";
}

const char* name(Refusal r) {
    switch (r) {
        case Refusal::None: return "none";
        case Refusal::CaptureRejected: return "capture_rejected";
        case Refusal::CaptureNotConformant: return "capture_not_conformant";
        case Refusal::CoverageIncomplete: return "capture_does_not_cover_whole_run";
        case Refusal::SamplesNotRecorded: return "samples_not_recorded";
        case Refusal::SamplesNotRecordedUnknown: return "samples_not_recorded_unknown";
        case Refusal::ProducerMismatch: return "producer_mismatch";
        case Refusal::SubscriptionMismatch: return "subscription_mismatch";
        case Refusal::LimitsMismatch: return "limits_mismatch";
        case Refusal::SchemaMismatch: return "schema_mismatch";
        case Refusal::ScenarioMismatch: return "scenario_mismatch";
        case Refusal::SegmentSetMismatch: return "segment_set_mismatch";
        case Refusal::NoComparableSegment: return "no_comparable_segment";
    }
    return "unknown";
}

const char* name(Purpose p) {
    switch (p) {
        case Purpose::SelfTest: return "self-test";
        case Purpose::ChangedInput: return "changed-input diff";
    }
    return "unknown";
}

const char* name(GateBasis b) {
    switch (b) {
        case GateBasis::Content: return "content";
        case GateBasis::Bytes: return "bytes";
    }
    return "unknown";
}

bool parseGateBasis(const std::string& text, GateBasis& out) {
    if (text == "content") { out = GateBasis::Content; return true; }
    if (text == "bytes") { out = GateBasis::Bytes; return true; }
    return false;
}

bool ComparisonResult::passed() const {
    if (gate != Verdict::Pass) { return false; }
    if (results.ran && results.outcomesGiven && !results.outcomesAgree) { return false; }
    if (results.ran && !results.verdictsAgree) { return false; }
    return true;
}

// --- The comparison ------------------------------------------------------------------------------

ComparisonResult compareCaptures(const std::string& pathA,
                                 const std::string& pathB,
                                 const std::string& labelA,
                                 const std::string& labelB,
                                 const std::string& outcomeA,
                                 const std::string& outcomeB,
                                 const CompareOptions& options) {
    ComparisonResult out;
    out.pathA = pathA;
    out.pathB = pathB;
    out.labelA = labelA.empty() ? pathA : labelA;
    out.labelB = labelB.empty() ? pathB : labelB;
    out.gateBasis = options.gateBasis;
    out.purpose = options.purpose;
    out.content.coverageFloor = options.coverageFloor;

    const auto refuse = [&out](Refusal r, const std::string& detail) {
        out.refusal = r;
        out.refusalDetail = detail;
        out.content.verdict = Verdict::Refused;
        out.content.verdictReason = detail;
        out.gate = Verdict::Refused;
        out.gateReason = detail;
    };

    CaptureIndex A = indexCapture(pathA);
    CaptureIndex B = indexCapture(pathB);

    // --- Preconditions. Each is a named refusal, because a refusal nobody can act on is the
    // --- same as no answer at all.
    for (int which = 0; which < 2; ++which) {
        const CaptureIndex& I = (which == 0) ? A : B;
        const std::string& label = (which == 0) ? out.labelA : out.labelB;
        if (I.set.parts.empty()) {
            refuse(Refusal::CaptureRejected, label + ": nothing could be read from " +
                                                 ((which == 0) ? pathA : pathB));
            return out;
        }
        for (const capture::ReadResult& part : I.set.parts) {
            if (part.rejected) {
                refuse(Refusal::CaptureRejected,
                       label + ": " + part.path + " was rejected â€” " + capture::name(part.rejectCode)
                           + ": " + part.rejectDetail);
                return out;
            }
        }
        if (!I.set.conformant()) {
            std::string d;
            for (const capture::ReadResult& part : I.set.parts) {
                for (const auto& kv : part.diagnosticCounts) {
                    d += (d.empty() ? "" : ", ") + std::string(capture::name(kv.first)) + " x"
                         + std::to_string(kv.second);
                }
            }
            for (const auto& kv : I.set.diagnosticCounts) {
                d += (d.empty() ? "" : ", ") + std::string(capture::name(kv.first)) + " x"
                     + std::to_string(kv.second);
            }
            refuse(Refusal::CaptureNotConformant,
                   label + ": the capture is not conformant (" + d
                       + "). A comparison over a file this reader has found faults in would be "
                         "reporting the faults, not the simulation.");
            return out;
        }
        if (!coversWholeRun(I.set)) {
            refuse(Refusal::CoverageIncomplete,
                   label + ": end_reason is size_limit with no continuation â€” this capture covers "
                           "PART of its run. Two runs are comparable because both were bounded at "
                           "the same frame, not because their captures ended alike (OQ-6).");
            return out;
        }
        long long dropped = 0;
        switch (samplesNotRecordedState(I.set, dropped)) {
            case DropState::NonZero:
                refuse(Refusal::SamplesNotRecorded,
                       label + ": trailer.drops.samples_not_recorded is " + std::to_string(dropped)
                           + ". It is already an incomplete record of its run; the self-test says "
                             "so rather than diffing it (CR-DET-1).");
                return out;
            case DropState::Unknown:
                refuse(Refusal::SamplesNotRecordedUnknown,
                       label + ": trailer.drops.samples_not_recorded is ABSENT. Absent means "
                               "unknown, never zero (format Â§11) â€” this capture's completeness "
                               "cannot be established, so it is not diffed.");
                return out;
            case DropState::Zero:
                break;
        }
    }

    // --- Like for like (Â§6.4 says to check before concluding; Â§14 says why) ---------------------
    const capture::Header& ha = A.set.parts.front().header;
    const capture::Header& hb = B.set.parts.front().header;
    if (ha.producerName != hb.producerName || ha.producerVersion != hb.producerVersion) {
        refuse(Refusal::ProducerMismatch,
               "producer " + ha.producerName + " " + ha.producerVersion + " against "
                   + hb.producerName + " " + hb.producerVersion
                   + ": two producer versions are not like for like.");
        return out;
    }
    if (subscriptionFingerprint(ha) != subscriptionFingerprint(hb)) {
        refuse(Refusal::SubscriptionMismatch,
               "subscription " + subscriptionFingerprint(ha) + " against "
                   + subscriptionFingerprint(hb)
                   + ": a different subscription records a different stream.");
        return out;
    }
    if (limitsFingerprint(ha) != limitsFingerprint(hb)) {
        refuse(Refusal::LimitsMismatch,
               "limits " + limitsFingerprint(ha) + " against " + limitsFingerprint(hb)
                   + ": two runs recorded under different bounds cut in different places (Â§14).");
        return out;
    }
    if (schemaFingerprint(ha) != schemaFingerprint(hb)) {
        refuse(Refusal::SchemaMismatch,
               "the two captures declare different schemas, so their field values are not the "
               "same values.");
        return out;
    }

    // --- Segment structure ------------------------------------------------------------------------
    std::map<capture::SegmentKey, const capture::SegmentStats*> segA, segB;
    for (const capture::SegmentStats& s : A.set.segments) { segA[s.key] = &s; }
    for (const capture::SegmentStats& s : B.set.segments) { segB[s.key] = &s; }
    if (segA.size() != segB.size()) {
        refuse(Refusal::SegmentSetMismatch,
               "run " + out.labelA + " has " + std::to_string(segA.size()) + " segment key(s) and "
                   + out.labelB + " has " + std::to_string(segB.size())
                   + ". Two runs with different segment structure did not do the same thing.");
        return out;
    }
    for (const auto& kv : segA) {
        if (segB.find(kv.first) == segB.end()) {
            refuse(Refusal::SegmentSetMismatch,
                   "segment (part " + std::to_string(kv.first.part) + ", segment "
                       + std::to_string(kv.first.segment) + ") is present in " + out.labelA
                       + " and absent from " + out.labelB + ".");
            return out;
        }
        if (kv.second->scenario != segB[kv.first]->scenario) {
            refuse(Refusal::ScenarioMismatch,
                   "segment (part " + std::to_string(kv.first.part) + ", segment "
                       + std::to_string(kv.first.segment) + ") is scenario \"" + kv.second->scenario
                       + "\" in " + out.labelA + " and \"" + segB[kv.first]->scenario + "\" in "
                       + out.labelB + ".");
            return out;
        }
    }

    // --- The content comparison, segment by segment -------------------------------------------------
    bool anyComparable = false;
    for (const auto& kv : segA) {
        const capture::SegmentKey key = kv.first;
        const capture::SegmentStats& sa = *kv.second;
        const capture::SegmentStats& sb = *segB[key];

        SegmentComparison sc;
        sc.key = key;
        sc.clockA = sa.clock;
        sc.clockB = sb.clock;
        sc.samplesA = sa.samples;
        sc.samplesB = sb.samples;

        {
            const KeySamples empty;
            const auto ea = A.samples.find(key);
            const auto eb = B.samples.find(key);
            countDuplicatedInstants(ea != A.samples.end() ? ea->second : empty,
                                    sc.duplicatedInstantsA, sc.duplicatedIdenticalA);
            countDuplicatedInstants(eb != B.samples.end() ? eb->second : empty,
                                    sc.duplicatedInstantsB, sc.duplicatedIdenticalB);
        }

        // CR-DET-1: "running segments only". `Indeterminate` is not `Running` â€” the format's
        // exact test could not fire, which is not the claim that it fired and passed.
        if (sa.clock != capture::ClockClass::Running || sb.clock != capture::ClockClass::Running) {
            sc.compared = false;
            sc.exclusionReason = std::string("clock is ") + clockName(sa.clock) + " in "
                                 + out.labelA + " and " + clockName(sb.clock) + " in " + out.labelB
                                 + "; only a running segment can be aligned across two runs";
            // CR-DET-3: name the SHAPE of the freeze, not only its existence. A duplicated
            // publication and a reset clock both satisfy Â§5.1's test and mean different things.
            for (int which = 0; which < 2; ++which) {
                const capture::ClockClass c = (which == 0) ? sa.clock : sb.clock;
                if (c != capture::ClockClass::Frozen) { continue; }
                const long long dup = (which == 0) ? sc.duplicatedInstantsA : sc.duplicatedInstantsB;
                const long long same = (which == 0) ? sc.duplicatedIdenticalA
                                                    : sc.duplicatedIdenticalB;
                const std::string who = (which == 0) ? out.labelA : out.labelB;
                sc.exclusionReason +=
                    ". In " + who + ", " + std::to_string(dup)
                    + " instant(s) were published more than once for one (entity, occupancy), "
                    + std::to_string(same) + " of them carrying IDENTICAL values";
                if (dup > 0 && dup == same) {
                    sc.exclusionReason +=
                        " â€” so this is a DUPLICATED PUBLICATION rather than a reset clock, and "
                        "the segment is excluded anyway because that is what the format's test "
                        "says to do (Â§5.1) and working around it is not this project's to do";
                } else if (dup > 0) {
                    sc.exclusionReason +=
                        " â€” a repeat carrying DIFFERENT values is a reset clock, which is the "
                        "case Â§5.1 describes";
                }
            }
            out.content.segments.push_back(sc);
            continue;
        }

        anyComparable = true;
        sc.compared = true;

        const KeySamples emptyKeys;
        const auto itA = A.samples.find(key);
        const auto itB = B.samples.find(key);
        const KeySamples& ka = (itA != A.samples.end()) ? itA->second : emptyKeys;
        const KeySamples& kb = (itB != B.samples.end()) ? itB->second : emptyKeys;
        sc.keysA = static_cast<long long>(ka.size());
        sc.keysB = static_cast<long long>(kb.size());

        // An ordered walk over the union of the two key sets. Ordered, never unordered: an
        // unordered container iterated on a path that produces compared output is the third
        // hazard [B] names, and it is the one that is easiest to reintroduce by accident.
        auto ia = ka.begin();
        auto ib = kb.begin();
        while (ia != ka.end() || ib != kb.end()) {
            const bool haveA = ia != ka.end();
            const bool haveB = ib != kb.end();
            if (haveA && (!haveB || ia->first < ib->first)) {
                sc.onlyInA += static_cast<long long>(ia->second.size());
                ++sc.keysOnlyInA;
                ++ia;
                continue;
            }
            if (haveB && (!haveA || ib->first < ia->first)) {
                sc.onlyInB += static_cast<long long>(ib->second.size());
                ++sc.keysOnlyInB;
                ++ib;
                continue;
            }

            // The same (entity, occupancy) in both. Merge the two sequences on sim_time_s â€”
            // ordering by the double the reader parsed for us, matching on the verbatim text.
            const std::vector<SampleRef>& va = ia->second;
            const std::vector<SampleRef>& vb = ib->second;
            std::size_t i = 0, j = 0;
            while (i < va.size() && j < vb.size()) {
                if (va[i].simText == vb[j].simText) {
                    ++sc.comparedSamples;
                    if (va[i].digest == vb[j].digest) {
                        ++sc.agree;
                    } else {
                        ++sc.differ;
                        if (out.content.differences.size() < options.maxDifferences) {
                            Difference d;
                            d.segment = key;
                            d.key = ia->first;
                            d.simTimeText = va[i].simText;
                            d.lineA = va[i].line;
                            d.lineB = vb[j].line;
                            nameFirstDifferingField(
                                lineAt(A.partPaths[static_cast<std::size_t>(va[i].part)], va[i].line),
                                lineAt(B.partPaths[static_cast<std::size_t>(vb[j].part)], vb[j].line),
                                d);
                            out.content.differences.push_back(std::move(d));
                        }
                    }
                    ++i;
                    ++j;
                } else if (va[i].simTime < vb[j].simTime) {
                    ++sc.onlyInA;
                    ++i;
                } else if (vb[j].simTime < va[i].simTime) {
                    ++sc.onlyInB;
                    ++j;
                } else {
                    // Equal doubles, different text. Â§8.3 makes doubles shortest-round-trip and
                    // uniquely determined, so this cannot happen â€” and if it ever does it is a
                    // finding about the producer, not something to smooth over.
                    ++sc.comparedSamples;
                    ++sc.differ;
                    if (out.content.differences.size() < options.maxDifferences) {
                        Difference d;
                        d.segment = key;
                        d.key = ia->first;
                        d.simTimeText = va[i].simText;
                        d.lineA = va[i].line;
                        d.lineB = vb[j].line;
                        d.field = "sim_time_s";
                        d.valueA = va[i].simText;
                        d.valueB = vb[j].simText;
                        out.content.differences.push_back(std::move(d));
                    }
                    ++i;
                    ++j;
                }
            }
            sc.onlyInA += static_cast<long long>(va.size() - i);
            sc.onlyInB += static_cast<long long>(vb.size() - j);
            ++ia;
            ++ib;
        }

        out.content.comparedSamples += sc.comparedSamples;
        out.content.agree += sc.agree;
        out.content.differ += sc.differ;
        out.content.onlyInA += sc.onlyInA;
        out.content.onlyInB += sc.onlyInB;
        out.content.comparableA += sc.samplesA;
        out.content.comparableB += sc.samplesB;
        out.content.segments.push_back(sc);
    }

    // --- The byte comparison, always, whatever the content one said ---------------------------------
    out.bytes.ran = true;
    out.bytes.identical = true;
    out.bytes.partsA = static_cast<long long>(A.partPaths.size());
    out.bytes.partsB = static_cast<long long>(B.partPaths.size());
    for (const std::string& p : A.partPaths) { out.bytes.bytesA += fileSize(p); }
    for (const std::string& p : B.partPaths) { out.bytes.bytesB += fileSize(p); }
    if (out.bytes.partsA != out.bytes.partsB) {
        out.bytes.identical = false;
        out.bytes.firstDifferingPart = 0;
    } else {
        for (std::size_t p = 0; p < A.partPaths.size(); ++p) {
            compareOnePair(A.partPaths[p], B.partPaths[p], out.bytes, static_cast<long long>(p));
            if (!out.bytes.identical) { break; }
        }
    }

    // --- Result equality: CR-DET-1's second half, [B] paragraph 9 -------------------------------------
    out.results.ran = true;
    out.results.outcomesGiven = !outcomeA.empty() && !outcomeB.empty();
    out.results.outcomeA = outcomeA.empty() ? "(not supplied)" : outcomeA;
    out.results.outcomeB = outcomeB.empty() ? "(not supplied)" : outcomeB;
    out.results.outcomesAgree = out.results.outcomesGiven && outcomeA == outcomeB;
    out.results.verdictsA = A.set.counts.verdicts;
    out.results.verdictsB = B.set.counts.verdicts;
    out.results.verdictsAgree = out.results.verdictsA == out.results.verdictsB;
    out.results.verdictsVacuous = (out.results.verdictsA == 0 && out.results.verdictsB == 0);

    // --- The content verdict ---------------------------------------------------------------------------
    if (!anyComparable) {
        std::string why =
            "no segment is running in BOTH runs, so there is nothing that can be aligned. "
            "Excluding a frozen or indeterminate segment is correct; having none left is not "
            "a pass.";
        for (const SegmentComparison& s : out.content.segments) {
            why += " (part " + std::to_string(s.key.part) + ", segment "
                   + std::to_string(s.key.segment) + "): " + s.exclusionReason + ".";
        }
        refuse(Refusal::NoComparableSegment, why);
        return out;
    }

    const long long denom = out.content.comparableA < out.content.comparableB
                                ? out.content.comparableA
                                : out.content.comparableB;
    out.content.coverage =
        denom > 0 ? static_cast<double>(out.content.comparedSamples) / static_cast<double>(denom)
                  : 0.0;

    long long keysOnly = 0;
    for (const SegmentComparison& s : out.content.segments) {
        keysOnly += s.keysOnlyInA + s.keysOnlyInB;
    }

    if (out.content.differ > 0) {
        out.content.verdict = Verdict::Fail;
        out.content.verdictReason =
            std::to_string(out.content.differ)
            + " sample(s) present in both runs at the same simulation instant carry DIFFERENT "
              "values. This is not the publication schedule; it is the simulation.";
    } else if (keysOnly > 0) {
        out.content.verdict = Verdict::Fail;
        out.content.verdictReason =
            std::to_string(keysOnly)
            + " (entity, occupancy) key(s) appear in one run and not the other. That is a "
              "difference in the roster, not in which frames were published.";
    } else if (out.content.comparedSamples == 0) {
        out.content.verdict = Verdict::Indeterminate;
        out.content.verdictReason =
            "no sample was present in both runs at the same simulation instant, so nothing was "
            "compared. Zero differences out of zero comparisons is not a pass.";
    } else if (out.content.coverage < options.coverageFloor) {
        out.content.verdict = Verdict::Indeterminate;
        out.content.verdictReason =
            "only " + percent(out.content.coverage) + " of the smaller run's comparable samples "
            "were present in both, against a floor of " + percent(options.coverageFloor)
            + ". Every one of them agreed, and an unmatched sample is not evidence of a "
              "difference â€” but an intersection this thin cannot support a verdict either.";
    } else {
        out.content.verdict = Verdict::Pass;
        out.content.verdictReason =
            "every sample present in BOTH runs at the same simulation instant agrees. This is not "
            "a claim that the two captures are identical: " + std::to_string(out.content.onlyInA)
            + " sample(s) were present only in " + out.labelA + " and "
            + std::to_string(out.content.onlyInB) + " only in " + out.labelB
            + ", which is the publication schedule and not the simulation (Â§14).";
    }

    // --- The gate ---------------------------------------------------------------------------------------
    if (options.gateBasis == GateBasis::Content) {
        out.gate = out.content.verdict;
        out.gateReason = out.content.verdictReason;
    } else {
        out.gate = out.bytes.identical ? Verdict::Pass : Verdict::Fail;
        out.gateReason =
            out.bytes.identical
                ? std::string("the two captures are byte-identical.")
                : std::string("the two captures are not byte-identical. Measured on this platform "
                              "and expected: the host publishes a slightly different subset of "
                              "frames every run (Â§14). Under this gate basis the campaign stops.");
    }
    return out;
}

// --- The report ----------------------------------------------------------------------------------------

std::string renderReport(const ComparisonResult& r) {
    std::string s;
    const auto row = [&s](const std::string& label, const std::string& text) {
        std::string l = label;
        while (l.size() < 22) { l.push_back(' '); }
        s += "  " + l + text + "\n";
    };
    const auto cont = [&s](const std::string& text) { s += "                        " + text + "\n"; };

    // The machinery below is identical for both purposes. Only the framing differs, and it has
    // to: for a changed-input diff a divergence is the ANSWER, not a failure, and calling it a
    // gate would be wrong in both directions. [B] asks for both halves — *"run the same
    // configuration twice and show that the results are identical; change one input and show
    // exactly where the two runs diverged"* — and they are different questions.
    if (r.purpose == Purpose::ChangedInput) {
        s += "run-to-run diff - two runs at DIFFERENT inputs\n";
        row("runs", r.labelA + "  against  " + r.labelB);
        row("what this is", "the brief's second diffing half: \"change one input and show exactly");
        cont("where the two runs diverged\". A divergence here is the ANSWER, not a");
        cont("failure - and it is not a determinism finding, because these are two");
        cont("configurations rather than one configuration run twice.");
        row("what it is NOT", "a gate. n8ro-campaign can only ever produce a self-test pair - two");
        cont("copies of one RunConfig - so nothing in a campaign can reach this");
        cont("framing by accident. It is asked for on purpose.");
        row("agreement here", "would mean the changed input did not take effect, and is worth");
        cont("more attention than a divergence.");
        s += "\n";
    } else {
        s += "determinism self-test\n";
        row("runs", r.labelA + "  against  " + r.labelB);
        row("gate basis", std::string(name(r.gateBasis))
                              + "   (ADR-1: the content basis is THIS PROJECT'S decision, not the "
                                "client's)");
        // OQ-2 is DECIDED and was never ANSWERED, and those are different words on purpose.
        // The DRI authorised the implementer to decide it from [B]'s own words on 2026-09-01;
        // [B]'s author never replied and a ruling from them would still be worth having. This
        // line says both, because a report that said only "content" would be hiding which.
        row("OQ-2", "DECIDED (DRI, 2026-09-01) - content. Decided from [B]'s own words, NOT");
        cont("answered by [B]'s author, who has not replied. The deciding sentence is");
        cont("\"if it ever fails, you have found either a defect in your harness or");
        cont("something far more interesting, and you must be able to tell which\" - a byte");
        cont("gate fails 100% of the time here and so distinguishes neither. See");
        cont("docs/m7-oq2-oq3.md. Both comparisons still run and are still reported below.");
        s += "\n";
    }

    if (r.refusal != Refusal::None) {
        row("REFUSED", std::string(name(r.refusal)));
        cont(r.refusalDetail);
        s += "\n";
        return s;
    }

    // --- content ---
    // Under ChangedInput the verdict is computed the same way and is not printed as a *verdict*,
    // because "fail" is the wrong word for two runs that were supposed to differ.
    if (r.purpose == Purpose::ChangedInput) {
        row("content comparison", r.content.differ > 0 ? "DIVERGED" : "AGREED EVERYWHERE");
    } else {
        const std::string cv = name(r.content.verdict);
        std::string cvUpper = cv;
        for (char& c : cvUpper) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        row("content comparison", cvUpper);
    }
    for (const SegmentComparison& sc : r.content.segments) {
        const std::string key = "(part " + std::to_string(sc.key.part) + ", segment "
                                + std::to_string(sc.key.segment) + ")";
        if (sc.compared) {
            row("  compared", key + "  running in both, " + std::to_string(sc.keysA)
                                  + " entity key(s)");
        } else {
            row("  excluded", key + "  " + sc.exclusionReason);
            cont("  " + std::to_string(sc.samplesA) + " and " + std::to_string(sc.samplesB)
                 + " sample(s) in it were not compared");
            if (sc.duplicatedInstantsA > 0 || sc.duplicatedInstantsB > 0) {
                cont("  duplicated instants: " + r.labelA + " "
                     + std::to_string(sc.duplicatedInstantsA) + " ("
                     + std::to_string(sc.duplicatedIdenticalA) + " identical), " + r.labelB + " "
                     + std::to_string(sc.duplicatedInstantsB) + " ("
                     + std::to_string(sc.duplicatedIdenticalB) + " identical)");
            }
        }
    }
    row("  compared", std::to_string(r.content.comparedSamples) + " sample(s)");
    row("  agree", std::to_string(r.content.agree));
    row("  differ", std::to_string(r.content.differ));
    row("  present in one only", r.labelA + ": " + std::to_string(r.content.onlyInA) + "   "
                                     + r.labelB + ": " + std::to_string(r.content.onlyInB)
                                     + "   (not differences - see verdict)");
    row("  coverage", percent(r.content.coverage) + " of the smaller run's comparable samples ("
                          + std::to_string(r.content.comparedSamples) + " of "
                          + std::to_string(r.content.comparableA < r.content.comparableB
                                               ? r.content.comparableA
                                               : r.content.comparableB)
                          + "), floor " + percent(r.content.coverageFloor));
    row("  verdict", r.content.verdictReason);

    // CR-DET-3 and CR-REP-4 ask for **the first** point of divergence, not merely that two runs
    // differ. A handful more are printed as context and are labelled as context — eight blocks
    // all headed FIRST DIFFERENCE reads as eight findings, when everything after the first is
    // downstream of it.
    bool firstDifference = true;
    for (const Difference& d : r.content.differences) {
        s += "\n";
        row(firstDifference ? "  FIRST DIFFERENCE" : "  then (context only)",
            "segment (part " + std::to_string(d.segment.part) + ", segment "
                + std::to_string(d.segment.segment) + ")  entity " + d.key.str());
        firstDifference = false;
        cont("  sim_time_s " + d.simTimeText);
        cont("  field \"" + d.field + "\": " + d.valueA + "   against   " + d.valueB);
        cont("  " + r.labelA + " line " + std::to_string(d.lineA) + ", " + r.labelB + " line "
             + std::to_string(d.lineB));
    }
    s += "\n";

    // Under ChangedInput the byte comparison and the result-equality check are both omitted, and
    // the omission is stated rather than left as a gap. Neither answers a question worth asking
    // here: two runs at different inputs are of course not byte-identical, and their outcomes are
    // of course allowed to differ — that is what a sweep is. Printing them under this framing
    // would put two "DIFFER" lines beside a divergence that is the intended answer, and invite
    // exactly the reading this mode exists to prevent.
    const bool changedInput = (r.purpose == Purpose::ChangedInput);
    if (changedInput) {
        row("byte comparison", "not run - see below");
        row("result equality", "not checked - see below");
        cont("Neither answers a question worth asking about two DIFFERENT inputs. Two such");
        cont("runs are of course not byte-identical, and their outcomes are of course allowed");
        cont("to differ - that is what a sweep is. Both belong to the self-test, where");
        cont("agreement is the expectation; here they would read as two more failures sitting");
        cont("beside an answer.");
        s += "\n";
    }

    // --- bytes ---
    if (!changedInput) {
    row("byte comparison", r.bytes.identical ? "IDENTICAL" : "DIFFER");
    row("  sizes", std::to_string(r.bytes.bytesA) + " and " + std::to_string(r.bytes.bytesB)
                       + " byte(s)");
    if (!r.bytes.identical && r.bytes.firstDifferingOffset >= 0) {
        row("  first difference", "byte " + std::to_string(r.bytes.firstDifferingOffset) + ", line "
                                      + std::to_string(r.bytes.firstDifferingLine)
                                      + (r.bytes.partsA > 1
                                             ? ", part " + std::to_string(r.bytes.firstDifferingPart)
                                             : std::string()));
    }
    row("  headers", r.bytes.headersIdentical ? "byte-identical"
                                              : "differ (see model_path below)");
    row("  model_path", r.bytes.modelPathExcluded
                            ? (r.bytes.modelPathDiffered
                                   ? std::string("EXCLUDED per format Â§14, and it differed â€” the "
                                                 "exclusion changed the result")
                                   : std::string("EXCLUDED per format Â§14; it was identical here, "
                                                 "so the exclusion changed nothing"))
                            : std::string("could not be located in the header to exclude"));
    row("  status", r.bytes.identical
                        ? std::string("byte-identical, which this platform has never produced.")
                        : std::string("EXPECTED TO FAIL on this platform, and not a defect. The "
                                      "host publishes a"));
    if (!r.bytes.identical) {
        cont("different subset of frames every run (Â§14). Nothing here is normalised,");
        cont("filtered or masked beyond the one field Â§14 names â€” a comparison made to");
        cont("pass by construction would measure nothing (ADR-1).");
    }
    s += "\n";

    // --- result equality ---
    row("result equality", std::string("outcomes ") + r.results.outcomeA + " and "
                              + r.results.outcomeB + " â€” "
                              + (!r.results.outcomesGiven
                                     ? "not supplied, so not checked"
                                     : (r.results.outcomesAgree ? "AGREE" : "DISAGREE")));
    row("  verdicts", std::to_string(r.results.verdictsA) + " and "
                          + std::to_string(r.results.verdictsB) + " â€” "
                          + (r.results.verdictsAgree ? "agree" : "DISAGREE")
                          + (r.results.verdictsVacuous
                                 ? ". Vacuous: neither capture carries a producer verdict, so "
                                   "there is nothing here to agree about. A campaign's own "
                                   "verdicts live in each run's verdicts.jsonl, not in the "
                                   "capture."
                                 : ""));
    s += "\n";
    }   // if (!changedInput) — the byte and result-equality sections

    if (changedInput) {
        // No gate line, and no pass/fail word anywhere. The answer to "where did they diverge"
        // is the FIRST DIFFERENCE block above; this states what was and was not established.
        if (r.content.differ > 0) {
            row("DIVERGED", "at the point named above - segment, (entity, occupancy), sim_time_s");
            cont("and field. That is the brief's \"exactly where\", and it is the answer");
            cont("this diff exists to give.");
            cont(std::to_string(r.content.differ) + " of " + std::to_string(r.content.comparedSamples)
                 + " compared sample(s) differ. The count is context; the FIRST one is");
            cont("the finding, because everything after it is downstream of it.");
        } else {
            row("DID NOT DIVERGE", "the two runs agree on every sample compared, which for two");
            cont("DIFFERENT inputs is the surprising outcome. Either the changed input");
            cont("did not take effect, or it does not influence anything recorded. Check");
            cont("the run records' parameter values before concluding anything else -");
            cont("a sweep that silently varied nothing is the failure this project keeps");
            cont("finding.");
        }
        cont("");
        cont("Samples present in one run only are NOT divergences here either: this");
        cont("platform publishes a different subset of frames every run, and that is");
        cont("true whether or not an input changed.");
        return s;
    }

    std::string gv = name(r.gate);
    for (char& c : gv) { c = static_cast<char>(std::toupper(static_cast<unsigned char>(c))); }
    row("GATE", gv + "   on the " + name(r.gateBasis) + " basis");
    cont(r.gateReason);
    if (r.gateBasis == GateBasis::Content) {
        cont("This discharges [B]'s acceptance criterion 2 under the CONTENT reading,");
        cont("which is this project's named deviation (ADR-1) and is now RULED - by the");
        cont("DRI on 2026-09-01, from [B]'s own words, and NOT by [B]'s author, who has");
        cont("not replied. It does not discharge criterion 2 under a byte reading, and");
        cont("nothing here claims it does: --gate-basis bytes fails on this platform and");
        cont("has been run (campaigns/m4-bytes/).");
    }
    return s;
}

}  // namespace ext17::compare

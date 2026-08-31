// EXT-17 — the model of an `n8ro-capture/1` capture, and the closed set of named things that
// can be wrong with one.
//
// **Written from `contract/capture-format-v1.md` alone.** No EXT-08 source was read, no EXT-08
// identifier appears here, and nothing in `src/capture/` links the N8RO SDK. That boundary is
// CR-CAP-2's first acceptance criterion and ADR-2's M3 gate; `tools/n8ro-capture/build.cmd` is
// where it is visible in one file, because that build script names no include path and no
// library at all.
//
// Never throws (constraint C3). Everything that can go wrong is a `Code` with a stable name.
//
// ## The two severities, and why the format has two
//
// `Reject` means stop and produce nothing. There are exactly three: the file could not be read,
// the first line is not a header, or the `format_version` is one this reader does not implement.
// The format is explicit that the third of these is a full stop — §3 step 2: *"reject the file
// with a named error naming the version found and the versions supported, and stop. Do not
// attempt a partial parse."*
//
// `Defect` means a named finding, recorded with the line it was found on, while the records
// already read stay available. That is equally the format's own instruction, from the other
// end: §3 step 5 says a file whose last line is not a trailer was truncated and *"everything
// before the truncation point is still valid and may be used."* Collapsing the two severities
// into one would either throw away a usable prefix or keep parsing a file whose version we
// cannot interpret, and each of those is a way to produce a confidently wrong number.
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace ext17::capture {

// The one version this reader implements. §13: the version string appears in exactly two
// places, the specification's title and a capture's `format_version`.
constexpr const char* kFormatVersion = "n8ro-capture/1";

enum class Code {
    Ok = 0,

    // --- Reject: stop, produce nothing --------------------------------------------------
    FileUnreadable,
    NotACapture,
    UnsupportedFormatVersion,

    // --- Defect: named, located, and the prefix stays usable -----------------------------
    MalformedLine,
    BlankLine,
    TruncatedNoTrailer,
    RecordsAfterTrailer,
    UnknownRecordType,
    MissingRequiredKey,
    FormatVersionNotFirst,
    ClosedVocabularyViolation,
    UnknownMessage,
    UndeclaredField,
    FieldOrderMismatch,
    FieldTypeMismatch,
    ArrayLengthMismatch,
    SampleOutsideSegment,
    SampleTimeDecreased,
    SegmentOrdinalNotIncreasing,
    UnbalancedSegment,
    SampleAfterRemove,
    UnmatchedEntityRemove,
    CountsDisagree,
    PartLinkBroken,
};

enum class Severity { Reject, Defect };

// A stable snake_case name for each code. This is what "a named error" means: the tests assert
// on these names, and a report prints them, so they are as much a part of the interface as the
// numbers beside them.
const char* name(Code code);
Severity severity(Code code);

struct Diagnostic {
    Code code = Code::Ok;
    std::size_t line = 0;      // 1-based line in the file; 0 when the finding is about the file
    std::string detail;        // always names the values involved, never just the problem
};

// --- The header -------------------------------------------------------------------------

struct FieldDecl {
    std::string name;
    std::string type;          // "int" | "double" | "string" | "bool"
    long long size = 1;        // 1 is a scalar; >1 is a fixed-length array (§6.5)
};

struct SchemaDecl {
    std::string messageName;
    std::string topic;
    // Kept as text as well as a number: `schema_hash` exceeds what a double holds exactly, and
    // a reader that compares two captures' schemas compares identities, not magnitudes.
    std::string schemaHashText;
    long long schemaHash = 0;
    long long messageId = 0;
    long long wireVersion = 0;
    std::vector<FieldDecl> fields;
    // name -> index into `fields`. Ordered, never unordered: CR-DET-2 forbids iterating a
    // container with unspecified order anywhere our output depends on it.
    std::map<std::string, std::size_t> fieldIndex;
};

struct Header {
    std::string formatVersion;
    std::string producerName;
    std::string producerVersion;

    std::string engineConfig;
    std::string modelPath;
    std::string schemaFile;
    std::string schemaVersion;     // may legitimately be empty (§6.2)
    std::string runtimeVersion;    // may legitimately be the literal "unknown" (§6.2)

    bool attachedMidRun = false;

    // Optional since producer 0.8.0. Absent means *unknown*, never "predicted" (§6.3a).
    bool hasSampleForm = false;
    std::string sampleForm;

    std::string topic;
    std::string backpressurePolicy;
    long long queueSize = 0;

    // Optional since producer 0.9.0. Absent means *unknown*, never "unbounded" (§6.6).
    bool hasLimits = false;
    long long maxBytes = 0;
    long long maxSamples = 0;
    std::string onSizeLimit;

    // Optional since producer 0.9.0. Absent `part` means 0 (§6.7).
    long long part = 0;
    bool hasContinuesFrom = false;
    std::string continuesFrom;

    std::vector<SchemaDecl> schemas;
    std::map<std::string, std::size_t> schemaIndex;   // message_name -> index
};

// --- The trailer ------------------------------------------------------------------------

struct Counts {
    long long segments = 0;
    long long samples = 0;
    long long entitiesAdded = 0;
    long long entitiesRemoved = 0;
    long long verdicts = 0;
};

// Every counter is tracked as present-or-absent, because the format says so repeatedly and for
// a reason: the delivery-side `bus_metrics` keys arrived at producer 0.4.2 and
// `events_not_recorded` at 0.5.0, and §11 requires a reader to treat a missing one as
// *unknown* rather than as zero. "All zeros" and "we were not told" are different claims, and
// tenet 3 is that absence is not evidence.
struct OptionalCount {
    bool present = false;
    long long value = 0;
};

struct Drops {
    OptionalCount samplesNotRecorded;
    OptionalCount eventsNotRecorded;
    OptionalCount samplesOrphaned;
    OptionalCount samplesUnnamed;
    OptionalCount samplesUntimed;
};

struct BusMetrics {
    OptionalCount schemaHashDrops;
    OptionalCount messageIdDrops;
    OptionalCount decodeFailures;
    OptionalCount missingSchemaPassthrough;
    OptionalCount legacyPayloadPassthrough;
    OptionalCount messagesDropped;
    OptionalCount droppedByBackpressure;
    OptionalCount droppedByQueueOverflow;
    OptionalCount droppedByRateLimiting;
};

struct Trailer {
    double simTimeS = 0.0;
    std::string endReason;         // shutdown | host_lost | size_limit | replay_end
    Counts counts;
    Drops drops;
    BusMetrics busMetrics;
    bool hasContinuedIn = false;
    std::string continuedIn;
};

// --- Segments ---------------------------------------------------------------------------

// A segment's ordinal is unique *within a file*; in a rotated set it restarts at 0 in every
// part (§6.7). So the key is the pair, everywhere, even for the overwhelmingly common case of
// an unrotated capture where `part` is always 0 — because a key that is only sometimes right
// is a key that is wrong in exactly the case nobody tested.
struct SegmentKey {
    long long part = 0;
    long long segment = 0;
    bool operator<(const SegmentKey& o) const {
        return part != o.part ? part < o.part : segment < o.segment;
    }
    bool operator==(const SegmentKey& o) const { return part == o.part && segment == o.segment; }
};

// The format's frozen-clock test is exact where it can fire and silent where it cannot, so this
// is deliberately three-valued rather than a boolean.
//
//   Running        — the maximum number of samples any one (entity, occupancy) carries at a
//                    single `sim_time_s` is 1. §5.1's exact test, satisfied.
//   Frozen         — that maximum is greater than 1. The engine's stop path reset the clock;
//                    the segment cannot be aligned against another run at all.
//   Indeterminate  — the test could not fire: too few samples for any key to repeat a time.
//                    Measured at M2 in 15 of 20 runs, where the teardown segment was opened and
//                    closed with no sample in it whatsoever. A boolean would have to call that
//                    segment "running", which is a claim the data does not support.
//
// Frozen *and* Indeterminate are excluded from any cross-run comparison. Only Running is
// comparable, and it is comparable because the test fired and passed.
enum class ClockClass { Running, Frozen, Indeterminate };

const char* name(ClockClass c);

struct SegmentStats {
    SegmentKey key;
    bool opened = false;           // a `segment_open` was seen
    bool closed = false;           // its matching `segment_close` was seen
    std::string scenario;
    std::string closeReason;       // scenario_unloaded | host_lost | shutdown | size_limit

    long long samples = 0;
    long long entityAdds = 0;
    long long entityRemoves = 0;
    long long verdicts = 0;

    // A segment's time extent is [first sample, last sample] — never the boundary records,
    // which both read 0.0 on a reloaded scenario (§5.1). Valid only when `samples > 0`.
    bool hasSamples = false;
    double firstSampleTimeS = 0.0;
    double lastSampleTimeS = 0.0;
    long long distinctSimTimes = 0;

    long long maxSamplesPerEntityPerSimTime = 0;
    ClockClass clock = ClockClass::Indeterminate;

    long long distinctEntityKeys = 0;   // distinct (entity, occupancy) pairs seen in this segment
};

// --- What one read produced --------------------------------------------------------------

struct Tallies {
    long long lines = 0;
    long long segmentsOpened = 0;
    long long segmentsClosed = 0;
    long long samples = 0;
    long long entityAdds = 0;
    long long entityRemoves = 0;
    long long verdicts = 0;
};

struct ReadResult {
    std::string path;
    long long bytes = 0;

    bool rejected = false;
    Code rejectCode = Code::Ok;
    std::string rejectDetail;

    Header header;
    bool hasTrailer = false;
    Trailer trailer;

    std::vector<SegmentStats> segments;   // ordered by key; built from `segment_open` too
    Tallies tallies;

    std::vector<Diagnostic> diagnostics;              // capped; see kMaxDiagnosticsPerCode
    std::map<Code, long long> diagnosticCounts;       // never capped

    // A file is conformant when nothing at all was found wrong with it. That is stricter than
    // "usable": a truncated capture is usable up to its truncation point and is not conformant.
    [[nodiscard]] bool conformant() const { return !rejected && diagnosticCounts.empty(); }
    [[nodiscard]] long long diagnosticTotal() const;
};

// A rotated run's capture is a set of files, not a file (§6.7). Reading one part is complete
// and correct on its own; reading the set is what makes a per-run statistic right.
struct SetResult {
    std::vector<ReadResult> parts;        // in `part` order
    std::vector<SegmentStats> segments;   // every part's, keyed on (part, segment)
    Counts counts;                        // summed across parts; no single part states this

    // **`counts.segments` is not the run's segment count, and the specification's §6.7 says
    // otherwise.** It states, of a trailer's `counts`, that "the run's totals are the sum across
    // parts". That holds for samples, adds, removes and verdicts. It does not hold for
    // `segments`: a segment cut by a rotation is closed in one part and opened in the next, so
    // the sum counts it twice — measured at M3 on a real four-part capture whose two-segment run
    // summed to five.
    //
    // Both numbers are reported. `counts.segments` is what the specification says to compute;
    // `runSegments` is what is true, derived from §6.7's own rule for recognising a cut — a part
    // whose last segment closed with `size_limit` and which carries a `continued_in`. The gap is
    // an imprecision in the contract and has gone back to EXT-08 as E-3 rather than being
    // quietly papered over here.
    long long runSegments = 0;
    long long segmentsCutByRotation = 0;
    std::vector<Diagnostic> diagnostics;  // set-level only: the part links
    std::map<Code, long long> diagnosticCounts;

    [[nodiscard]] bool conformant() const;
    [[nodiscard]] long long diagnosticTotal() const;
};

} // namespace ext17::capture

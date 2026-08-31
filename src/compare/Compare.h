// EXT-17 — comparing two captures of one configuration. CR-DET-1, CR-DET-3, ADR-1.
//
// **Links nothing.** Not EXT-08, not the N8RO SDK, not a third-party library — the same rule
// `src/capture/` lives under, and for the same reason: this is the code whose verdict the whole
// project rests on, and a boundary anyone can check by reading `tools/n8ro-compare/build.cmd`
// is worth more than an argument about translation units. It links `src/capture/` and
// `src/common/JsonParse`, both of which link nothing either.
//
// Never throws (constraint C3).
//
// ## The four things this file gets right on purpose, because each is a way to be wrong
//
// **1. `sim_time_s` is an alignment, never a key.** Two runs are aligned by matching the
// *verbatim text* of `sim_time_s` within one `(segment, entity, occupancy)` sequence. Ordering
// for the merge uses the double the reader already parsed for us — locale-independently, which
// is where CR-DET-2's locale hazard would otherwise live — but a *match* is decided on the text.
// The format writes doubles in shortest round-trip form (§8.3), so equal text and equal double
// are the same relation; deciding equality on the text means the verdict never depends on a
// numeric conversion at all. Nothing here formats a number for comparison, ever.
//
// **2. Only `Running` segments are compared.** `Frozen` cannot be aligned (§5.1). And
// `Indeterminate` is not `Running`: the format's exact test *could not fire*, which is not the
// same claim as "it fired and passed". Measured here — across M2's twenty runs the teardown
// segment carries samples in only five, at 42 / 16 / 30 / 37 / 14 entity keys. Comparing it
// would report 26 "present in one run only" between runs 000 and 001, every one an artifact of
// comparing two teardowns that were never the same event.
//
// **3. A sample present in one run and absent from the other is NOT a difference.** It is
// `onlyInA` / `onlyInB`, and it is the *expected* case: the host publishes a slightly different
// subset of frames every run (§14). Measured over all 190 pairs of M2's twenty runs — worst
// 0.4192%, best 0.0000%, and **zero differing values in 9 573 667 samples compared**.
//
// **4. …and it is still bounded, because "they all agree" over three samples is a wrong
// number.** The intersection must reach `coverageFloor` of the smaller run's comparable
// samples, or the verdict is `Indeterminate` — not `Pass` and not `Fail`. Unmatched samples are
// evidence about the publication schedule, never about the simulation (tenet 3), so they may
// not fail the gate; but a collapsed intersection may not silently pass it either (tenet 1).
#pragma once

#include "../capture/Capture.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace ext17::compare {

// --- What a comparison concluded ----------------------------------------------------------

enum class Verdict {
    Pass,           // every compared sample agreed, and enough of them were compared
    Fail,           // at least one compared sample disagreed
    Indeterminate,  // the comparison ran but cannot support a verdict — see the reason
    Refused,        // the comparison was never made — see Refusal
};

const char* name(Verdict v);

// Why two captures were not compared at all. Every one of these is a precondition the format or
// the PRD states, and each is reported by name rather than as a general failure — a refusal
// nobody can act on is the same as no answer.
enum class Refusal {
    None = 0,
    CaptureRejected,            // the reader would not read it at all
    CaptureNotConformant,       // it read, and something was wrong with it
    CoverageIncomplete,         // covers_whole_run false: it is part of its run (OQ-6, §6.6)
    SamplesNotRecorded,         // trailer.drops.samples_not_recorded is non-zero (CR-DET-1)
    SamplesNotRecordedUnknown,  // …or absent. Absent is unknown, never zero (§11, tenet 3)
    ProducerMismatch,           // different producer versions are not like for like (§6.4)
    SubscriptionMismatch,       // different subscription policy: likewise
    LimitsMismatch,             // one bounded and one not: they cut in different places (§14)
    SchemaMismatch,             // different schema identities: the values are not the same values
    ScenarioMismatch,
    SegmentSetMismatch,         // the two runs do not even have the same segment keys
    NoComparableSegment,        // no segment is Running in BOTH runs
};

const char* name(Refusal r);

// --- Identity ------------------------------------------------------------------------------

// An entity's identity is the pair, never the name (§8.1). The engine re-creates entities under
// names it has already used, both mid-run and at teardown.
struct EntityKey {
    std::string entity;
    long long occupancy = 0;
    bool operator<(const EntityKey& o) const {
        return entity != o.entity ? entity < o.entity : occupancy < o.occupancy;
    }
    bool operator==(const EntityKey& o) const {
        return entity == o.entity && occupancy == o.occupancy;
    }
    [[nodiscard]] std::string str() const {
        return entity + "@" + std::to_string(occupancy);
    }
};

// --- CR-DET-3: a failure that names where and in what shape the runs parted -----------------

struct Difference {
    capture::SegmentKey segment;
    EntityKey key;
    std::string simTimeText;      // verbatim, straight out of the file. Never reformatted
    std::size_t lineA = 0;        // 1-based line in each capture, so a reader can go and look
    std::size_t lineB = 0;
    std::string field;            // the first field whose value differed, by name
    std::string valueA;           // and its two values, as text
    std::string valueB;
};

// --- Per-segment -----------------------------------------------------------------------------

struct SegmentComparison {
    capture::SegmentKey key;
    capture::ClockClass clockA = capture::ClockClass::Indeterminate;
    capture::ClockClass clockB = capture::ClockClass::Indeterminate;

    bool compared = false;
    std::string exclusionReason;   // set when !compared, and always says which clock class

    long long samplesA = 0;        // what the segment holds in each run
    long long samplesB = 0;

    long long comparedSamples = 0; // present in both, at the same sim_time_s
    long long agree = 0;
    long long differ = 0;
    long long onlyInA = 0;
    long long onlyInB = 0;

    long long keysA = 0;           // distinct (entity, occupancy) in each run
    long long keysB = 0;
    long long keysOnlyInA = 0;
    long long keysOnlyInB = 0;

    // Why a segment is `Frozen`, which the format's exact test does not distinguish and which a
    // reader has to know to tell a harness defect from a platform finding (CR-DET-3).
    //
    // §5.1's test is "more than one sample for one `(entity, occupancy)` at one `sim_time_s`",
    // and §5.1 reads that as the engine's stop path having reset the clock. Measured here in 2
    // of 42 captures, it also fires on a **duplicated publication**: the same instant published
    // twice with byte-identical values, inside a segment whose clock plainly did not reset —
    // 1 200 distinct `sim_time_s` spanning 0 to 60. Those are two different phenomena with two
    // different consequences, and a refusal that cannot say which is not actionable.
    //
    // Both are still excluded. This does not change the rule; it explains the exclusion.
    long long duplicatedInstantsA = 0;
    long long duplicatedInstantsB = 0;
    long long duplicatedIdenticalA = 0;   // ...of which carried the SAME values in that run
    long long duplicatedIdenticalB = 0;
};

// --- The content comparison ------------------------------------------------------------------

struct ContentResult {
    Verdict verdict = Verdict::Refused;
    std::string verdictReason;

    std::vector<SegmentComparison> segments;   // every segment key, compared or excluded

    long long comparedSamples = 0;
    long long agree = 0;
    long long differ = 0;
    long long onlyInA = 0;
    long long onlyInB = 0;

    // The denominator the coverage floor is measured against: the comparable samples of
    // whichever run has fewer of them. Stated so the percentage is checkable by hand.
    long long comparableA = 0;
    long long comparableB = 0;
    double coverage = 0.0;         // comparedSamples / min(comparableA, comparableB)
    double coverageFloor = 0.0;

    // Capped: a systematic divergence would otherwise produce one entry per sample. The counts
    // above are never capped.
    std::vector<Difference> differences;
};

// --- The byte comparison ---------------------------------------------------------------------

// Run and reported on every self-test, and **never engineered to pass** (ADR-1). The one
// exclusion is the one §14 names — `platform.model_path`, "the one host-dependent field" — and
// the result says both that it was excluded and whether excluding it changed anything. Nothing
// else is normalised, filtered or masked: a comparison made to pass by construction measures
// nothing, and that is the rabbit hole this project's PRD names by name.
struct ByteResult {
    bool ran = false;
    bool identical = false;

    long long bytesA = 0;
    long long bytesB = 0;
    long long partsA = 0;
    long long partsB = 0;

    bool headersIdentical = false;
    bool modelPathExcluded = false;   // the exclusion was applied
    bool modelPathDiffered = false;   // …and it actually differed, i.e. the exclusion mattered

    long long firstDifferingOffset = -1;   // absolute byte offset, -1 if none
    long long firstDifferingLine = -1;     // 1-based line it falls in
    long long firstDifferingPart = -1;     // which part of a rotated set it fell in
};

// --- Result equality: CR-DET-1's second half, [B] paragraph 9 ---------------------------------
//
// "Run the same configuration twice and show that the **results** are identical." This is a
// distinct check from the capture comparison and is reported as its own line: it is what a
// campaign author actually depends on, and a disagreement here is more serious than a byte
// difference, because it means one configuration was judged two different ways.
struct ResultEquality {
    bool ran = false;
    bool outcomesGiven = false;
    std::string outcomeA;
    std::string outcomeB;
    bool outcomesAgree = false;

    // From each capture's own trailer. At M4 no conditions are declared, so this is 0 = 0 and
    // is reported as vacuous rather than as a pass — the line becomes substantive at M6 with
    // CR-AS-3, and it exists now so that M6 has a seam rather than a new requirement.
    long long verdictsA = 0;
    long long verdictsB = 0;
    bool verdictsAgree = false;
    bool verdictsVacuous = false;
};

// --- The whole thing ---------------------------------------------------------------------------

enum class GateBasis { Content, Bytes };
const char* name(GateBasis b);
bool parseGateBasis(const std::string& text, GateBasis& out);

// What the comparison is FOR. The machinery is identical either way — the same alignment, the
// same digest, the same coverage floor, the same twelve refusals. What differs is the question
// being asked, and therefore what the answer means.
//
//   SelfTest      two runs of ONE configuration. They are expected to agree, and a divergence is
//                 a determinism finding. This is CR-DET-1's gate.
//   ChangedInput  two runs at DIFFERENT inputs. They are expected to diverge, and the question is
//                 **where** — [B]: *"change one input and show exactly where the two runs
//                 diverged."* Calling that a gate would be wrong in both directions: a divergence
//                 is not a failure, and agreement is not a pass. Agreement here means the input
//                 did not take effect, which is the shape this project keeps finding.
//
// Keeping them apart is deliberate. `n8ro-campaign` can only ever produce a self-test pair — two
// copies of one RunConfig — so nothing in a campaign can hand the gate a changed-input pair by
// accident. This flag is how a person asks for the other question on purpose.
enum class Purpose { SelfTest, ChangedInput };

const char* name(Purpose p);

struct CompareOptions {
    Purpose purpose = Purpose::SelfTest;

    // Measured, not chosen: the worst unmatched rate over all 190 pairs of M2's twenty runs was
    // 0.4192%, so a 99% floor sits about 2.4x clear of anything this machine has produced. It is
    // a guard against a collapsed intersection, not a tolerance on the platform's behaviour.
    double coverageFloor = 0.99;

    // CR-DET-3 wants the first differing record, not all of them.
    std::size_t maxDifferences = 8;

    // Which comparison decides the gate. Content is ADR-1's decision and this project's, not
    // the client's; Bytes is [B]'s strictest reading. **Both always run and both are always
    // reported** — the basis chooses which one the gate reads, and nothing else. OQ-2 is the
    // ruling that would settle which is right, and it is unanswered.
    GateBasis gateBasis = GateBasis::Content;
};

struct ComparisonResult {
    std::string pathA, pathB;
    std::string labelA, labelB;

    Refusal refusal = Refusal::None;
    std::string refusalDetail;

    ContentResult content;
    ByteResult bytes;
    ResultEquality results;

    GateBasis gateBasis = GateBasis::Content;
    // What this comparison was for. The gate below is computed either way — the machinery is
    // identical — but under `ChangedInput` the report does not print it, because a divergence
    // between two configurations is an answer and not a failure.
    Purpose purpose = Purpose::SelfTest;
    Verdict gate = Verdict::Refused;
    std::string gateReason;

    // True only when the gate passed AND the result-equality check did not disagree. A campaign
    // proceeds on this and on nothing else.
    [[nodiscard]] bool passed() const;
};

// Compare two captures. `outcomeA`/`outcomeB` are the two runs' four-state outcomes where the
// caller knows them (the self-test does; a bare `n8ro-compare` of two stored files does not),
// and an empty string means "not supplied" rather than "no outcome" — absence is reported as
// absence throughout this project.
ComparisonResult compareCaptures(const std::string& pathA,
                                 const std::string& pathB,
                                 const std::string& labelA,
                                 const std::string& labelB,
                                 const std::string& outcomeA,
                                 const std::string& outcomeB,
                                 const CompareOptions& options = {});

// Render a comparison as the report block that goes into the campaign report and the log. It is
// a pure function of the result: run it twice on one result and the two strings are identical,
// which is how CR-DET-2's "no timestamp in compared output" is tested rather than asserted.
std::string renderReport(const ComparisonResult& result);

} // namespace ext17::compare

// EXT-17 — evaluating declared conditions over a stored capture. CR-AS-2, CR-AS-3, CR-AS-4,
// CR-CAP-1, CR-REP-2.
//
// **Links nothing** — not EXT-08, not the N8RO SDK, not a third-party library. It links
// `src/capture/`, `src/common/JsonParse` and `src/assert/Conditions`, all of which link nothing
// either. `tools/n8ro-judge/build.cmd` is where that is visible in one file.
//
// ## The input is a stored capture, always — and that is how CR-CAP-1 is met
//
// There is **one evaluator and one kind of input**. The live campaign judges the capture it has
// just written and read back; a re-judge judges the same file a week later. Nothing here can
// start a host, load a scenario, subscribe to a bus or write into a capture — not by discipline
// but because this translation unit has no way to. So CR-CAP-1's second acceptance criterion,
// *"verdicts produced by re-judging a stored capture are identical to those produced during the
// live run"*, holds **by construction** rather than by comparison, and is then demonstrated
// anyway by byte-comparing the two verdict files. Both, because a structural argument nobody
// checked is how a structural argument stops being true.
//
// ## Never throws (constraint C3), and never formats a number through a locale
//
// Every failure is a state on a verdict, with a reason. Distances are rendered by this file's
// own fixed formatter rather than by the C library's, whose fixed-point conversions read the
// ambient decimal separator, so CR-DET-2's locale hazard has no foothold here and the build
// searches to keep it that way — the same rule `src/compare/` lives under.
//
// ## What a verdict is entitled to say — CR-AS-4, per form rather than per kind
//
// A `Met` verdict is always sound: it is computed from records that are **present**. The whole
// question is what a not-met verdict may claim, and classifying per *kind* turns out to be the
// wrong granularity — `terminal_state`'s two forms differ completely. The classification, with
// the mechanism that licenses each, is:
//
//   `proximity`                   decidable when the closest observed approach clears the
//                                 threshold by more than the largest possible unobserved
//                                 excursion, and both tracks are bounded over the segment.
//                                 Mechanism: continuity over present samples.
//
//   `area`                        the same, measured to the region's boundary.
//
//   `terminal_state`              decidable when every occupancy of the named entity either
//     + `removal_reason`          carries a removal record stating some *other* reason, or
//                                 carries a sample at the segment's last sampled instant.
//                                 Mechanism: `capture-format-v1.md` §8.1, normative — *"Within
//                                 one (entity, occupancy) pair, no sample ever appears after
//                                 that pair's entity_remove. That invariant does hold."* So a
//                                 sample carrying (E, k) at t is POSITIVE evidence that
//                                 occupancy k had not been removed at t, and a sampling gap
//                                 does not weaken it: a re-created entity carries a *higher*
//                                 occupancy. This is what makes [B]'s own dangerous example —
//                                 *"did anything reach a terminal state it should not have"* —
//                                 answerable here, because the entity kept publishing rather
//                                 than because nothing said it did.
//
//   `terminal_state`              **never** decidable. Nothing in the format bounds a string
//     + `field` + `equals`        field's rate of change, so the value could be taken and left
//                                 inside one sampling gap. Measured across the committed sweep:
//                                 `health` moved through nominal -> degraded -> disabled ->
//                                 wrecked -> destroyed with zero regressions in seven runs — a
//                                 direction, not a guarantee, and this classification
//                                 deliberately does not lean on it.
//
// The continuity bound is `(v_a + v_b) * dt_max + 0.5 * a_max * dt_max^2`, over the largest gap
// observed in either track. Measured over the committed sweep: the largest gap is 0.1000 s in
// all seven runs — exactly one missed frame at the platform's 0.05 s period, which is F-11
// showing through at the size F-11 measured it — and the tightest not-met margin clears its
// bound by a factor of 65. The acceleration term uses F-21's measured 20 m/s^2 platform clamp,
// and exists so that the bound for two static entities is 0.10 m rather than exactly zero: a
// velocity is read from a sample field, and a field can change between samples.
//
// ## Segments
//
// Verdicts are evaluated over **Running** segments only, the same rule the comparison lives
// under and for a reason that applies here too: in a `Frozen` segment `sim_time_s` no longer
// orders the samples, so "the first moment it was satisfied" has no meaning and the gap the
// bound is computed from is not measurable. A run with no Running segment is not judgeable, and
// that is reported as such rather than as a run in which everything passed — R14 makes this
// roughly 1 run in 9 under parameterisation, so it is a path that gets exercised.
#pragma once

#include "Conditions.h"
#include "../capture/Capture.h"

#include <cstddef>
#include <string>
#include <vector>

namespace ext17::assertion {

// F-21, measured at M5: the platform walks an over-commanded entity down at exactly 20 m/s^2,
// 1 m/s per 0.05 s frame. Used only to widen a continuity bound, never to predict a trajectory.
constexpr double kPlatformAccelerationClampMs2 = 20.0;

// CR-AS-4's three states. `Indeterminate` is a **verdict** state and never a fifth **run**
// outcome — [B] fixes the run vocabulary at four, and keeping the two apart is what makes its
// acceptance criterion 5 stay exactly satisfied.
enum class State { Met, NotMet, Indeterminate };

const char* toString(State s);

// Whether the fact the verdict states is the one the campaign asserted. Kept separate from
// `State` on purpose: the state is what the run did, and this is whether that was welcome. A
// verdict file therefore still records EXT-08-comparable facts, and the expectation is EXT-17's
// layer on top of them rather than a change to what a verdict means.
//
//   Satisfied     the state is the expected one.
//   Violated      the state is decided and is NOT the expected one. This, and only this, makes
//                 a run `fail` (CR-EX-5).
//   Undetermined  the state is Indeterminate, so nothing is asserted either way. Never folded
//                 into pass or fail - CR-AS-4's whole point.
enum class Outcome { Satisfied, Violated, Undetermined };

const char* toString(Outcome o);

// Why a verdict reached the state it did, as a stable snake_case name. A report prints these and
// the tests assert on them, so they are as much a part of the interface as the numbers.
enum class Because {
    None = 0,
    // Met
    ThresholdReached,          // proximity: two samples within the threshold
    RegionTestSatisfied,       // area: a sample satisfied inside/outside
    RemovalReasonMatched,      // terminal_state: an entity_remove carried the reason
    FieldValueMatched,         // terminal_state: a sample's field carried the value
    // NotMet, soundly
    ClearedByContinuityBound,  // the margin exceeds the largest possible unobserved excursion
    EveryOccupancyAccounted,   // §8.1: every occupancy is closed by a record, or ran to the end
    // Indeterminate
    NoRunningSegment,          // nothing in this capture can be judged (R14's shape)
    EntityNeverSampled,        // the condition names an entity this capture has no sample for
    TrackNotBounded,           // an unobserved window this evaluator cannot bound
    MarginWithinBound,         // it did not happen in what was recorded, and might have between
    FieldAbsenceNotBoundable,  // `field`+`equals`: never decidable in the negative
    TooFewSamplesForGap,       // fewer than two samples, so no gap can be measured
};

const char* toString(Because b);

// One entity a verdict rests on, with everything needed to find its record by hand — CR-REP-2's
// *"go and look"*. The occupancy is not decoration: identity is the pair (§8.1).
struct EntityRef {
    std::string entity;
    long long occupancy = 0;
    std::string simTimeText;   // verbatim, straight out of the file. Never reformatted
    std::size_t line = 0;      // 1-based line in the capture
};

// CR-AS-2: one verdict per declared condition per run, saying what was checked, on what data,
// and why it failed.
struct Verdict {
    std::string conditionId;
    Kind kind = Kind::Proximity;
    State state = State::Indeterminate;
    Because because = Because::None;

    // What was asserted, and whether the state honoured it.
    Expect expect = Expect::Met;
    Outcome outcome = Outcome::Undetermined;

    // Where it was decided.
    bool hasSegment = false;
    capture::SegmentKey segment;
    std::string capturePath;

    // The entities and the instants that decided it.
    std::vector<EntityRef> entities;
    std::string decidingSimTimeText;

    // The values that decided it, named. `measuredName` is e.g. "distance_m"; both are text
    // because a report prints them and CR-AS-2 asks that they be reproducible by hand.
    std::string measuredName;
    std::string measuredText;
    std::string thresholdName;
    std::string thresholdText;

    // CR-AS-4's soundness accounting, carried on every verdict rather than only when it bit.
    bool absenceDependent = false;   // would a naive reading have concluded from absence?
    bool boundApplied = false;
    std::string marginText;          // metres by which the observation cleared the threshold
    std::string boundText;           // metres it could have moved unobserved
    std::string largestGapText;      // seconds

    // Prose, one line, always naming the values. This is what a person reads.
    std::string reason;
};

// What judging one capture produced.
struct JudgeResult {
    std::string capturePath;

    // The capture could not be read at all, or was read and something was wrong with it. Both
    // route to `infrastructure_error` at the campaign level (CR-EX-5) rather than to `fail`.
    bool rejected = false;
    std::string rejectReason;
    bool conformant = false;
    std::vector<std::string> captureDiagnostics;

    // False when no segment classified Running — R14's shape. Every verdict is then
    // Indeterminate and the run is not a test result.
    bool judgeable = false;
    std::string notJudgeableReason;

    std::vector<Verdict> verdicts;

    // The facts.
    long long met = 0;
    long long notMet = 0;
    long long indeterminate = 0;

    // What the facts mean for the run.
    long long satisfied = 0;
    long long violated = 0;
    long long undetermined = 0;

    [[nodiscard]] long long decided() const { return met + notMet; }
};

// Judge one stored capture. Reads it, evaluates every condition over it, and produces exactly
// one verdict per condition — including for conditions naming entities the capture never
// mentions, which are `Indeterminate` with `EntityNeverSampled` rather than silently absent. A
// reader seeing fewer verdicts than conditions is entitled to treat the run as cut short.
bool judgeCapture(const std::string& capturePath,
                  const ConditionFile& conditions,
                  JudgeResult& out);

// The same over a capture already in memory. The tests use this so that a condition evaluated
// against a hand-written eight-line capture needs no fixture file and no simulator.
bool judgeLines(const std::vector<std::string>& lines,
                const std::string& label,
                const ConditionFile& conditions,
                JudgeResult& out);

// Render a verdict as one line, in the shape EXT-08's referee prints and this project's report
// reuses. Stable, locale-free, and byte-identical between a live judgement and a re-judgement —
// which is what makes CR-CAP-1's identity check a file comparison.
std::string verdictLine(const Verdict& v);

// The same verdict as one JSON object on one line. **This lives here, and only here.** The live
// campaign and `n8ro-judge` both call it, because CR-CAP-1's identity check compares the two
// files byte for byte and two implementations of a renderer eventually differ by a space.
std::string verdictJson(const Verdict& v);

// A fixed-precision, locale-free rendering of a double. Exposed because the report and the
// verdict must agree to the digit, and two implementations would eventually not.
std::string fixed(double value, int decimals);

} // namespace ext17::assertion

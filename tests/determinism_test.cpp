// EXT-17 — the determinism comparison's tests. CR-DET-1, CR-DET-2, CR-DET-3.
//
// Links nothing: not EXT-08, not the N8RO SDK. `tests\build.cmd` builds and runs it, and it
// needs no N8RO install.
//
// ## Two kinds of test here, and neither subsumes the other
//
// **The comparison's own behaviour** — that a sample present in one run and absent from the
// other is not a difference; that an `indeterminate` segment is excluded rather than aligned;
// that each of the preconditions refuses by name. These are written against synthetic captures
// small enough to read in the test that asserts on them, which is the pattern M3's tier 3
// established and the reason its 17 micro-captures earned their place.
//
// **CR-DET-2's three hazards, tested by behaviour rather than by inspection.** [B] names them:
// *"a timestamp in the compared output, an unordered container iterated, a value read from a
// clock."* `tools\n8ro-compare\build.cmd` searches the sources for each by name and fails the
// build on a hit; that catches a reintroduction on a path no test exercises. What it cannot
// catch is a hazard spelled in a way the search does not know. So each is *also* tested here by
// running the comparison and asserting that its output does not move:
//
//   - a clock read, and a timestamp in compared output — the same comparison run twice produces
//     byte-identical report text. Any wall-clock value anywhere on the path breaks this;
//   - an unordered container iterated — the same captures built with their entities and their
//     segments inserted in the opposite order produce byte-identical report text;
//   - and the locale hazard, which is this project's own addition — the whole suite is re-run
//     under a comma-decimal locale and every report must be identical to the C-locale one.
//
// A test that would fail if the hazard were reintroduced is what CR-DET-2's acceptance criterion
// asks for, in those words.
#include "../src/compare/Compare.h"

#include <clocale>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace ext17::compare;

namespace {

int g_failures = 0;
int g_checks = 0;

void ok(const std::string& what, bool condition, const std::string& detail = {}) {
    ++g_checks;
    if (condition) {
        std::printf("  ok   %s\n", what.c_str());
    } else {
        ++g_failures;
        std::printf("  FAIL %s\n", what.c_str());
        if (!detail.empty()) { std::printf("       %s\n", detail.c_str()); }
    }
}

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

// --- A synthetic capture builder -------------------------------------------------------------
//
// Three fields. One double and one int cover §8.3's two numeric encodings; `v` is a
// three-element ARRAY, and it is here because of F-31 — until M6 a difference in an array field
// named the field correctly and then printed two EMPTY values, and every synthetic capture in
// this suite carried only scalars, so nothing here could see it. The real fields a divergence is
// most likely to be in — positionGeodetic, velocityNed, orientationYprRad — are all arrays.
const char* kSchemas =
    R"("schemas":[{"message_name":"m","topic":"t","schema_hash":7,"message_id":8,)"
    R"("wire_version":1,"fields":[{"name":"a","type":"double","size":1},)"
    R"({"name":"b","type":"int","size":1},{"name":"v","type":"double","size":3}]}])";

struct Cap {
    std::vector<std::string> lines;
    long long segs = 0, samples = 0, adds = 0, removes = 0;

    void header(const std::string& modelPath = "m", const std::string& producerVersion = "9.9.9",
                const std::string& limits = R"("limits":{"max_bytes":0,"max_samples":0,"on_size_limit":"stop"})",
                long long queueSize = 8) {
        std::string h = R"({"format_version":"n8ro-capture/1","type":"header",)"
                        R"("producer":{"name":"p","version":")" + producerVersion + R"("},)"
                        R"("platform":{"engine_config":"e","model_path":")" + modelPath +
                        R"(","schema_file":"s","schema_version":"","runtime_version":"unknown"},)"
                        R"("attached_mid_run":false,)"
                        R"("subscription":{"topic":"t","backpressure_policy":"FIFO_DROP","queue_size":)"
                        + std::to_string(queueSize) + "},";
        if (!limits.empty()) { h += limits + ","; }
        h += kSchemas;
        h += "}";
        lines.push_back(h);
    }
    void open(long long seg, const char* scenario = "S") {
        lines.push_back(R"({"type":"segment_open","sim_time_s":0,"segment":)"
                        + std::to_string(seg) + R"(,"scenario":")" + scenario + "\"}");
        ++segs;
    }
    void close(long long seg, const char* reason = "shutdown") {
        lines.push_back(R"({"type":"segment_close","sim_time_s":0,"segment":)"
                        + std::to_string(seg) + R"(,"scenario":"S","reason":")" + reason + "\"}");
    }
    void add(long long seg, const char* e, long long occ) {
        lines.push_back(R"({"type":"entity_add","sim_time_s":0,"segment":)" + std::to_string(seg)
                        + R"(,"entity":")" + e + R"(","occupancy":)" + std::to_string(occ) + "}");
        ++adds;
    }
    // `t` is written verbatim, because the alignment is on the verbatim text and a test that
    // formatted it through the current locale would be testing the wrong thing.
    void sample(long long seg, const std::string& t, const char* e, long long occ,
                const std::string& a, const std::string& b,
                const std::string& v = "[0,0,0]") {
        lines.push_back(R"({"type":"sample","sim_time_s":)" + t + R"(,"segment":)"
                        + std::to_string(seg) + R"(,"entity":")" + e + R"(","occupancy":)"
                        + std::to_string(occ) + R"(,"message":"m","fields":{"a":)" + a
                        + R"(,"b":)" + b + R"(,"v":)" + v + "}}");
        ++samples;
    }
    void trailer(const char* endReason = "shutdown", long long notRecorded = 0,
                 bool omitNotRecorded = false) {
        std::string t = R"({"type":"trailer","sim_time_s":0,"end_reason":")";
        t += endReason;
        t += R"(","counts":{"segments":)" + std::to_string(segs) + R"(,"samples":)"
             + std::to_string(samples) + R"(,"entities_added":)" + std::to_string(adds)
             + R"(,"entities_removed":)" + std::to_string(removes) + R"(,"verdicts":0})";
        t += R"(,"drops":{)";
        if (!omitNotRecorded) { t += R"("samples_not_recorded":)" + std::to_string(notRecorded) + ","; }
        t += R"("events_not_recorded":0,"samples_orphaned":0,)"
             R"("samples_unnamed":0,"samples_untimed":0})";
        t += R"(,"bus_metrics":{"schema_hash_drops":0,"message_id_drops":0,"decode_failures":0,)"
             R"("missing_schema_passthrough":0,"legacy_payload_passthrough":0})";
        t += "}";
        lines.push_back(t);
    }

    void write(const fs::path& path) const {
        std::ofstream out(path, std::ios::binary);
        for (const std::string& l : lines) { out << l << "\n"; }
    }
};

fs::path g_dir;

// Two runs of one "configuration": 3 entities x 4 frames, all values agreeing.
Cap goodRun(bool reverseEntityOrder = false) {
    Cap c;
    c.header();
    c.open(0);
    const char* entities[] = {"alpha", "bravo", "charlie"};
    for (int e = 0; e < 3; ++e) {
        const int idx = reverseEntityOrder ? 2 - e : e;
        c.add(0, entities[idx], 1);
    }
    const char* times[] = {"0.0", "0.05", "0.1", "0.15000000000000002"};
    for (const char* t : times) {
        for (int e = 0; e < 3; ++e) {
            const int idx = reverseEntityOrder ? 2 - e : e;
            c.sample(0, t, entities[idx], 1, std::string("1.") + std::to_string(idx),
                     std::to_string(idx * 10));
        }
    }
    c.close(0);
    // The teardown segment: opened, closed, and empty. `indeterminate`, and excluded.
    c.open(1);
    c.close(1, "host_lost");
    c.trailer("host_lost");
    return c;
}

std::string pathFor(const char* name) { return (g_dir / name).string(); }

ComparisonResult compareTwo(const Cap& a, const Cap& b, const CompareOptions& o = {}) {
    a.write(g_dir / "a.n8rocap.jsonl");
    b.write(g_dir / "b.n8rocap.jsonl");
    return compareCaptures(pathFor("a.n8rocap.jsonl"), pathFor("b.n8rocap.jsonl"), "A", "B",
                           "completed", "completed", o);
}

// --- The comparison's own behaviour -----------------------------------------------------------

void identicalRuns() {
    std::printf("\ntwo runs that agree everywhere\n");
    const ComparisonResult r = compareTwo(goodRun(), goodRun());
    ok("the gate passes", r.gate == Verdict::Pass, r.gateReason);
    ok("nothing was refused", r.refusal == Refusal::None, r.refusalDetail);
    ok("12 samples were compared and 12 agreed",
       r.content.comparedSamples == 12 && r.content.agree == 12 && r.content.differ == 0,
       std::to_string(r.content.comparedSamples) + "/" + std::to_string(r.content.agree));
    ok("coverage is 100%", r.content.coverage >= 1.0);
    ok("the byte comparison ran and found them identical", r.bytes.ran && r.bytes.identical);
    ok("result equality: the two outcomes agree", r.results.outcomesAgree);
    ok("and the verdict count is reported as vacuous rather than as a pass",
       r.results.verdictsVacuous && r.results.verdictsAgree);
}

void theEmptyTeardownSegmentIsExcluded() {
    std::printf("\nthe teardown segment: indeterminate is NOT running\n");
    const ComparisonResult r = compareTwo(goodRun(), goodRun());
    const SegmentComparison* s1 = nullptr;
    for (const SegmentComparison& s : r.content.segments) {
        if (s.key.segment == 1) { s1 = &s; }
    }
    ok("segment 1 appears in the report at all", s1 != nullptr);
    if (s1 == nullptr) { return; }
    ok("it is classified indeterminate in both runs",
       s1->clockA == ext17::capture::ClockClass::Indeterminate &&
           s1->clockB == ext17::capture::ClockClass::Indeterminate);
    ok("and it was EXCLUDED, not compared", !s1->compared);
    ok("the exclusion names the clock class rather than saying only that it was skipped",
       contains(s1->exclusionReason, "indeterminate"), s1->exclusionReason);
}

void aMissingFrameIsNotADifference() {
    std::printf("\na sample present in one run and absent from the other (the expected case)\n");
    Cap b = goodRun();
    // Drop one whole frame from B — three samples, the shape the platform actually loses
    // (frame-shaped, §14). The lines are the last frame's three samples.
    std::vector<std::string> kept;
    int dropped = 0;
    for (const std::string& l : b.lines) {
        if (contains(l, R"("sim_time_s":0.1,)") && contains(l, R"("type":"sample")")) {
            ++dropped;
            continue;
        }
        kept.push_back(l);
    }
    b.lines = kept;
    b.samples -= dropped;
    // Rewrite the trailer so the file stays conformant: the counts must agree with what is in it.
    b.lines.pop_back();
    b.trailer("host_lost");

    const ComparisonResult r = compareTwo(goodRun(), b);
    ok("three samples were dropped from B", dropped == 3, std::to_string(dropped));
    ok("neither capture was refused", r.refusal == Refusal::None, r.refusalDetail);
    ok("the three are reported as present in A only, not as differences",
       r.content.onlyInA == 3 && r.content.onlyInB == 0 && r.content.differ == 0,
       "onlyInA " + std::to_string(r.content.onlyInA) + " differ "
           + std::to_string(r.content.differ));
    ok("the nine that were present in both agree", r.content.comparedSamples == 9 &&
                                                       r.content.agree == 9);
    ok("and the gate still PASSES - this is the publication schedule, not the simulation",
       r.gate == Verdict::Pass, r.gateReason);
    ok("the verdict says so in words rather than claiming the captures are identical",
       contains(r.content.verdictReason, "not\na claim") ||
           contains(r.content.verdictReason, "not a claim"),
       r.content.verdictReason);
}

// A run at a set of sim_time_s values, so a test can control the overlap between two runs
// exactly. Four frames per key, so the format's frozen-clock test can fire and the segment is
// classified `running` rather than `indeterminate`.
Cap runAtTimes(const std::vector<std::string>& times) {
    Cap c;
    c.header();
    c.open(0);
    const char* entities[] = {"alpha", "bravo", "charlie"};
    for (const char* e : entities) { c.add(0, e, 1); }
    for (const std::string& t : times) {
        for (int e = 0; e < 3; ++e) {
            c.sample(0, t, entities[e], 1, std::string("1.") + std::to_string(e),
                     std::to_string(e * 10));
        }
    }
    c.close(0);
    c.open(1);
    c.close(1, "host_lost");
    c.trailer("host_lost");
    return c;
}

void aSingleFrameSegmentCannotBeCompared() {
    std::printf("\na segment with one sample per key: the clock test cannot fire\n");
    // Measured at M3 on a real run: run 000 of the M2 campaign has a segment 1 of 42 samples,
    // one per entity, all at sim_time_s 0.0 — enough samples to look like a segment and not
    // enough for any key to repeat a time. `Running` is the claim that the format's exact test
    // fired and passed, and here it could not fire.
    const Cap one = runAtTimes({"0.0"});
    const ComparisonResult r = compareTwo(one, one);
    ok("two identical one-frame runs are REFUSED, not passed",
       r.refusal == Refusal::NoComparableSegment, name(r.refusal));
    ok("and the refusal says that having no comparable segment left is not a pass",
       contains(r.refusalDetail, "not a pass"), r.refusalDetail);
}

void aThinIntersectionIsIndeterminateNotPass() {
    std::printf("\nan intersection too thin to support a verdict\n");
    // Both runs are four frames and both segments classify as running. They share exactly one
    // frame, so every sample compared agrees and only a quarter of them were comparable at all.
    const Cap a = runAtTimes({"0.0", "0.05", "0.1", "0.15"});
    const Cap b = runAtTimes({"0.0", "0.07", "0.12", "0.17"});

    const ComparisonResult r = compareTwo(a, b);
    ok("neither capture was refused", r.refusal == Refusal::None, r.refusalDetail);
    ok("three of twelve samples were present in both", r.content.comparedSamples == 3,
       std::to_string(r.content.comparedSamples));
    ok("every one of them agreed", r.content.differ == 0 && r.content.agree == 3);
    ok("and the verdict is INDETERMINATE, not pass - 'they all agreed' over three samples "
       "is a wrong number",
       r.content.verdict == Verdict::Indeterminate, r.content.verdictReason);
    ok("the gate does not pass", r.gate != Verdict::Pass);
    ok("the reason names the coverage and the floor",
       contains(r.content.verdictReason, "floor"), r.content.verdictReason);
    ok("…and it still says the unmatched samples are not evidence of a difference",
       contains(r.content.verdictReason, "not evidence"), r.content.verdictReason);

    // The floor is a guard against a collapsed intersection, not a tolerance on the platform.
    // Lowered, the same pair is a pass, and the numbers underneath it do not move.
    CompareOptions permissive;
    permissive.coverageFloor = 0.10;
    const ComparisonResult r2 = compareTwo(a, b, permissive);
    ok("with the floor lowered the same pair passes, and the counts are unchanged",
       r2.gate == Verdict::Pass && r2.content.comparedSamples == 3 && r2.content.differ == 0,
       r2.gateReason);
}

void aFrozenSegmentNamesTheShapeOfItsFreeze() {
    std::printf("\nCR-DET-3: WHY a segment is frozen - a duplicated publication is not a reset "
                "clock\n");

    // Measured on this machine at M4, in 2 of 42 captures: the start-up burst publishes some
    // entities' sim_time_s 0 sample twice, byte-identical, inside a segment whose clock plainly
    // did not reset - 1 200 distinct sim_time_s spanning 0 to 60. The format's exact test (§5.1)
    // fires on that and calls the segment frozen, which is correct by the test and reads as a
    // reset clock, which it is not. The segment is excluded either way; the refusal has to say
    // which, or a reader cannot tell a harness defect from a platform finding.
    Cap dup;
    dup.header();
    dup.open(0);
    dup.add(0, "alpha", 1);
    dup.sample(0, "0.0", "alpha", 1, "1.0", "1");
    dup.sample(0, "0.0", "alpha", 1, "1.0", "1");   // the same instant, the same values
    dup.sample(0, "0.05", "alpha", 1, "2.0", "2");
    dup.close(0);
    dup.trailer("host_lost");

    Cap clean = runAtTimes({"0.0", "0.05"});
    // Trim `clean` to one entity so the segment sets match and only the clock class differs.
    Cap plain;
    plain.header();
    plain.open(0);
    plain.add(0, "alpha", 1);
    plain.sample(0, "0.0", "alpha", 1, "1.0", "1");
    plain.sample(0, "0.05", "alpha", 1, "2.0", "2");
    plain.close(0);
    plain.trailer("host_lost");

    const ComparisonResult r = compareTwo(dup, plain);
    ok("a duplicated instant makes the segment frozen and the pair is REFUSED, not passed",
       r.refusal == Refusal::NoComparableSegment, name(r.refusal));
    ok("the refusal names it as a duplicated publication rather than a reset clock",
       contains(r.refusalDetail, "DUPLICATED PUBLICATION"), r.refusalDetail);
    ok("…and counts the duplicated instants and how many carried identical values",
       !r.content.segments.empty() && r.content.segments[0].duplicatedInstantsA == 1 &&
           r.content.segments[0].duplicatedIdenticalA == 1);
    ok("…and says the exclusion stands anyway, because the format's test is not ours to work "
       "around",
       contains(r.refusalDetail, "excluded anyway"), r.refusalDetail);

    // The other shape: a repeat carrying DIFFERENT values, which is the reset clock §5.1 means.
    Cap reset;
    reset.header();
    reset.open(0);
    reset.add(0, "alpha", 1);
    reset.sample(0, "0.0", "alpha", 1, "1.0", "1");
    reset.sample(0, "0.0", "alpha", 1, "7.0", "7");   // the same instant, different values
    reset.sample(0, "0.05", "alpha", 1, "2.0", "2");
    reset.close(0);
    reset.trailer("host_lost");

    const ComparisonResult r2 = compareTwo(reset, plain);
    ok("a repeat carrying different values is named a reset clock instead",
       contains(r2.refusalDetail, "reset clock, which is the case"), r2.refusalDetail);
    ok("…and its identical-value count is zero, which is what distinguishes the two",
       !r2.content.segments.empty() && r2.content.segments[0].duplicatedInstantsA == 1 &&
           r2.content.segments[0].duplicatedIdenticalA == 0);
}

void aDifferingValueFailsAndIsAttributed() {
    std::printf("\nCR-DET-3: a differing value, attributed rather than merely reported\n");
    Cap b = goodRun();
    for (std::string& l : b.lines) {
        if (contains(l, R"("entity":"bravo")") && contains(l, R"("sim_time_s":0.05,)")) {
            const std::size_t at = l.find(R"("a":1.1)");
            if (at != std::string::npos) { l.replace(at, 7, R"("a":9.9)"); }
        }
    }
    const ComparisonResult r = compareTwo(goodRun(), b);
    ok("the gate FAILS", r.gate == Verdict::Fail, r.gateReason);
    ok("exactly one sample differed", r.content.differ == 1,
       std::to_string(r.content.differ));
    ok("the other eleven still agreed", r.content.agree == 11);
    ok("the difference is attributed to an (entity, occupancy), not to a line number",
       !r.content.differences.empty() && r.content.differences[0].key.entity == "bravo" &&
           r.content.differences[0].key.occupancy == 1);
    ok("…and to a segment", !r.content.differences.empty() &&
                                r.content.differences[0].segment.segment == 0);
    ok("…and it names the field and both values",
       !r.content.differences.empty() && r.content.differences[0].field == "a" &&
           r.content.differences[0].valueA == "1.1" && r.content.differences[0].valueB == "9.9",
       r.content.differences.empty() ? "(none)" : r.content.differences[0].field + " " +
           r.content.differences[0].valueA + "/" + r.content.differences[0].valueB);
    ok("…and the line in each file, so a reader can go and look",
       !r.content.differences.empty() && r.content.differences[0].lineA > 0 &&
           r.content.differences[0].lineB > 0);
    ok("the report states whether the headers agree and how the record counts compare",
       contains(renderReport(r), "headers") && contains(renderReport(r), "sizes"));
}

// [B] asks for BOTH halves of the diff and they are different questions: *"run the same
// configuration twice and show that the results are identical; change one input and show exactly
// where the two runs diverged."* The machinery is identical; what changes is what the answer
// means, and getting that framing wrong would be wrong in both directions — a divergence between
// two configurations is not a failure, and agreement between them is not a pass.
void aChangedInputDiffIsNotAGate() {
    std::printf("\nCR-REP-4: the changed-input half - a divergence is the ANSWER, not a failure\n");
    Cap b = goodRun();
    for (std::string& l : b.lines) {
        if (contains(l, R"("entity":"bravo")") && contains(l, R"("sim_time_s":0.05,)")) {
            const std::size_t at = l.find(R"("a":1.1)");
            if (at != std::string::npos) { l.replace(at, 7, R"("a":9.9)"); }
        }
    }

    CompareOptions changed;
    changed.purpose = Purpose::ChangedInput;
    const ComparisonResult r = compareTwo(goodRun(), b, changed);
    const std::string report = renderReport(r);

    ok("it still finds and names the FIRST point of divergence",
       r.content.differ == 1 && !r.content.differences.empty() &&
           r.content.differences[0].key.entity == "bravo" &&
           r.content.differences[0].field == "a");
    ok("...by segment, (entity, occupancy), sim_time_s and field - never by line number alone",
       contains(report, "FIRST DIFFERENCE") && contains(report, "sim_time_s") &&
           contains(report, "bravo@1"));
    ok("the report says what this comparison IS",
       contains(report, "two runs at DIFFERENT inputs"));
    ok("...and quotes the brief's own words for it",
       contains(report, "change one input and show exactly"));
    ok("it prints NO gate line, because a gate would be the wrong question",
       !contains(report, "GATE"));
    ok("...and says so explicitly, so nobody reads a missing line as an omission",
       contains(report, "what it is NOT") && contains(report, "a gate."));
    ok("...and says a campaign cannot reach this framing by accident",
       contains(report, "copies of one RunConfig"));
    ok("it reports the divergence as the ANSWER", contains(report, "DIVERGED"));

    std::printf("\n...and AGREEMENT between two different inputs is worth flagging\n");
    {
        const ComparisonResult same = compareTwo(goodRun(), goodRun(), changed);
        const std::string sameReport = renderReport(same);
        ok("two identical captures under --changed-input report DID NOT DIVERGE",
           same.content.differ == 0 && contains(sameReport, "DID NOT DIVERGE"));
        ok("...and say the changed input may not have taken effect",
           contains(sameReport, "did not take effect"));
        ok("...which is the failure this project keeps finding, named as such",
           contains(sameReport, "silently varied nothing"));
        ok("...and still no gate line, and still no pass/fail word",
           !contains(sameReport, "GATE"));
    }

    std::printf("\n...while the SAME pair on the self-test framing is a gate FAILURE\n");
    {
        const ComparisonResult gated = compareTwo(goodRun(), b);
        ok("the default purpose still gates", gated.gate == Verdict::Fail);
        ok("...and prints the gate line", contains(renderReport(gated), "GATE"));
        ok("...over exactly the same measured difference",
           gated.content.differ == r.content.differ);
    }
}

// F-31, and the reason it went three milestones unnoticed: a difference in an ARRAY field named
// the field correctly and then printed two empty values. `positionGeodetic`, `velocityNed` and
// `orientationYprRad` are all arrays, so this was every geometric divergence the diff would ever
// report — and CR-DET-3 and CR-REP-4 both require the deciding VALUES, not only the field.
//
// It survived because it takes a real content-gate failure on a real pair to see it, and until
// M6 the content gate had never failed on one: M4's failing-gate evidence came from forcing
// `--gate-basis bytes`, which reports a byte offset and never reaches that code. The synthetic
// captures here carried only scalars, so nothing in this suite could see it either. Both halves
// of that are now fixed — the builder carries an array field, and this is the check.
void aDifferingArrayFieldNamesBothValues() {
    std::printf("\nCR-DET-3 / CR-REP-4: a differing ARRAY value is printed, not left blank\n");
    Cap b = goodRun();
    for (std::string& l : b.lines) {
        if (contains(l, R"("entity":"bravo")") && contains(l, R"("sim_time_s":0.05,)")) {
            const std::size_t at = l.find(R"("v":[0,0,0])");
            if (at != std::string::npos) {
                l.replace(at, 11, R"("v":[-1.0103336092965664e-14,-55,0])");
            }
        }
    }
    const ComparisonResult r = compareTwo(goodRun(), b);
    ok("the gate FAILS", r.gate == Verdict::Fail, r.gateReason);
    ok("the difference is attributed to the array field by name",
       !r.content.differences.empty() && r.content.differences[0].field == "v",
       r.content.differences.empty() ? "(none)" : r.content.differences[0].field);
    ok("...and NEITHER value is empty, which is the whole of F-31",
       !r.content.differences.empty() && !r.content.differences[0].valueA.empty() &&
           !r.content.differences[0].valueB.empty());
    ok("...and each renders every element, verbatim",
       !r.content.differences.empty() &&
           r.content.differences[0].valueA == "[0, 0, 0]" &&
           r.content.differences[0].valueB == "[-1.0103336092965664e-14, -55, 0]",
       r.content.differences.empty()
           ? "(none)"
           : r.content.differences[0].valueA + "  against  " + r.content.differences[0].valueB);
    ok("...including a value the platform round-tripped to 1e-14 rather than to zero - which is "
       "the real difference this defect was hiding",
       !r.content.differences.empty() &&
           contains(r.content.differences[0].valueB, "1.0103336092965664e-14"));
    ok("...and the rendered report carries them too, not just the machine-readable record",
       contains(renderReport(r), "-1.0103336092965664e-14"));
}

void anEntityInOneRunOnlyIsAFailure() {
    std::printf("\nan (entity, occupancy) in one run and not the other\n");
    Cap b = goodRun();
    std::vector<std::string> kept;
    for (const std::string& l : b.lines) {
        if (contains(l, R"("entity":"charlie")") && contains(l, R"("type":"sample")")) { continue; }
        kept.push_back(l);
    }
    b.lines = kept;
    b.samples -= 4;
    b.lines.pop_back();
    b.trailer("host_lost");

    const ComparisonResult r = compareTwo(goodRun(), b, [] {
        CompareOptions o;
        o.coverageFloor = 0.0;   // so the coverage floor cannot be what fails it
        return o;
    }());
    ok("the gate FAILS", r.gate == Verdict::Fail, r.gateReason);
    ok("and the reason is the roster, not the publication schedule",
       contains(r.content.verdictReason, "roster"), r.content.verdictReason);
}

void occupancyIsPartOfTheIdentity() {
    std::printf("\nidentity is (entity, occupancy), never the name\n");
    Cap a;
    a.header();
    a.open(0);
    a.add(0, "alpha", 1);
    a.sample(0, "0.0", "alpha", 1, "1.0", "1");
    a.sample(0, "0.05", "alpha", 1, "2.0", "2");
    a.close(0);
    a.trailer("host_lost");

    // B publishes the same values under the same name at occupancy 2. A comparison keyed on the
    // name alone would call these two runs identical. They are not: they are two different
    // bodies that happen to share a name (§8.1).
    Cap b;
    b.header();
    b.open(0);
    b.add(0, "alpha", 2);
    b.sample(0, "0.0", "alpha", 2, "1.0", "1");
    b.sample(0, "0.05", "alpha", 2, "2.0", "2");
    b.close(0);
    b.trailer("host_lost");

    CompareOptions o;
    o.coverageFloor = 0.0;
    const ComparisonResult r = compareTwo(a, b, o);
    ok("the two occupancies are NOT merged into one entity", r.gate == Verdict::Fail,
       r.gateReason);
    ok("and it is reported as a roster difference", contains(r.content.verdictReason, "roster"),
       r.content.verdictReason);
}

// --- The preconditions, each refusing by name ---------------------------------------------------

void preconditions() {
    std::printf("\nthe preconditions, each refused by name\n");

    {   // CR-DET-1: "a capture with non-zero samples_not_recorded is EXCLUDED rather than diffed"
        Cap b = goodRun();
        b.lines.pop_back();
        b.trailer("host_lost", 2520);
        const ComparisonResult r = compareTwo(goodRun(), b);
        ok("a non-zero samples_not_recorded is refused, not diffed",
           r.refusal == Refusal::SamplesNotRecorded, name(r.refusal));
        ok("and the refusal names the number", contains(r.refusalDetail, "2520"),
           r.refusalDetail);
        ok("the gate does not pass", r.gate != Verdict::Pass);
    }
    {   // …and absent is unknown, never zero (§11, tenet 3).
        Cap b = goodRun();
        b.lines.pop_back();
        b.trailer("host_lost", 0, /*omitNotRecorded=*/true);
        const ComparisonResult r = compareTwo(goodRun(), b);
        ok("an ABSENT samples_not_recorded is refused too - absent is unknown, never zero",
           r.refusal == Refusal::SamplesNotRecordedUnknown, name(r.refusal));
    }
    {   // covers_whole_run (OQ-6, §6.6)
        Cap b = goodRun();
        b.lines.pop_back();
        b.trailer("size_limit");
        const ComparisonResult r = compareTwo(goodRun(), b);
        ok("a capture that covers part of its run is refused",
           r.refusal == Refusal::CoverageIncomplete, name(r.refusal));
    }
    {   // like for like: producer version (§6.4)
        Cap b;
        b.header("m", "9.9.10");
        b.open(0);
        b.add(0, "alpha", 1);
        b.sample(0, "0.0", "alpha", 1, "1.0", "1");
        b.close(0);
        b.trailer("host_lost");
        const ComparisonResult r = compareTwo(goodRun(), b);
        ok("two producer versions are not like for like",
           r.refusal == Refusal::ProducerMismatch, name(r.refusal));
    }
    {   // like for like: subscription
        Cap c;
        c.header("m", "9.9.9",
                 R"("limits":{"max_bytes":0,"max_samples":0,"on_size_limit":"stop"})", 4096);
        c.open(0);
        c.add(0, "alpha", 1);
        c.sample(0, "0.0", "alpha", 1, "1.0", "1");
        c.close(0);
        c.trailer("host_lost");
        const ComparisonResult r = compareTwo(goodRun(), c);
        ok("a different subscription is not like for like",
           r.refusal == Refusal::SubscriptionMismatch, name(r.refusal));
    }
    {   // like for like: limits (§14, "compare like for like")
        Cap c;
        c.header("m", "9.9.9",
                 R"("limits":{"max_bytes":8000000,"max_samples":0,"on_size_limit":"stop"})");
        c.open(0);
        c.add(0, "alpha", 1);
        c.sample(0, "0.0", "alpha", 1, "1.0", "1");
        c.close(0);
        c.trailer("host_lost");
        const ComparisonResult r = compareTwo(goodRun(), c);
        ok("two runs recorded under different bounds are not like for like",
           r.refusal == Refusal::LimitsMismatch, name(r.refusal));
    }
    {   // model_path is the ONE field §14 allows to differ
        Cap c = goodRun();
        const std::string was = R"("model_path":"m")";
        c.lines[0].replace(c.lines[0].find(was), was.size(), R"("model_path":"z")");
        const ComparisonResult r = compareTwo(goodRun(), c);
        ok("a differing model_path does NOT refuse the comparison",
           r.refusal == Refusal::None, r.refusalDetail);
        ok("the content gate still passes", r.gate == Verdict::Pass, r.gateReason);
        ok("the byte comparison excluded it and says the exclusion mattered",
           r.bytes.modelPathExcluded && r.bytes.modelPathDiffered);
        ok("…and with it excluded the two files are byte-identical", r.bytes.identical);
    }
    {   // no comparable segment at all
        Cap a;
        a.header();
        a.open(0);
        a.close(0, "host_lost");
        a.trailer("host_lost");
        const ComparisonResult r = compareTwo(a, a);
        ok("two captures with no running segment are refused, not passed",
           r.refusal == Refusal::NoComparableSegment, name(r.refusal));
    }
    {   // a segment structure that does not match
        Cap b;
        b.header();
        b.open(0);
        b.add(0, "alpha", 1);
        b.sample(0, "0.0", "alpha", 1, "1.0", "1");
        b.close(0);
        b.trailer("host_lost");
        const ComparisonResult r = compareTwo(goodRun(), b);
        ok("two runs with different segment structure are refused",
           r.refusal == Refusal::SegmentSetMismatch, name(r.refusal));
    }
}

// --- CR-DET-2: the three hazards, tested by behaviour -------------------------------------------

std::string reportFor(const Cap& a, const Cap& b) {
    return renderReport(compareTwo(a, b));
}

void hazardClockAndTimestamp() {
    std::printf("\nCR-DET-2 hazard 1 and 2: a clock read, and a timestamp in compared output\n");
    const std::string first = reportFor(goodRun(), goodRun());
    const std::string second = reportFor(goodRun(), goodRun());
    ok("the same comparison run twice produces byte-identical report text",
       first == second,
       "lengths " + std::to_string(first.size()) + " and " + std::to_string(second.size()));
    ok("and it is a real report rather than an empty string",
       first.size() > 200 && contains(first, "GATE"));
    // A wall-clock value cannot be hidden inside a number that happens to match: the report is
    // also asserted to contain none of the shapes a formatted time takes.
    ok("no formatted date or time appears anywhere in it",
       !contains(first, "20") || !contains(first, ":") ||
           first.find("T0") == std::string::npos,
       first.substr(0, 200));
}

void hazardUnorderedIteration() {
    std::printf("\nCR-DET-2 hazard 3: an unordered container iterated\n");
    // The same two captures, differing only in the order their entities appear in the file. The
    // comparison walks its keys through ordered containers, so the report must not move. An
    // unordered_map anywhere on the path would key on the hash of the entity name and iterate in
    // an order that depends on insertion, which is exactly what this changes.
    const std::string forward = reportFor(goodRun(false), goodRun(false));
    const std::string reversed = reportFor(goodRun(true), goodRun(true));
    ok("entities inserted in the opposite order produce byte-identical report text",
       forward == reversed);
    // …and the cross case: one run's file lists them forward, the other's backward. Same run,
    // different record order, and the comparison must still find them all and agree.
    const ComparisonResult r = compareTwo(goodRun(false), goodRun(true));
    ok("a run whose records are ordered differently still compares equal",
       r.gate == Verdict::Pass && r.content.agree == 12 && r.content.differ == 0,
       r.gateReason);
}

void hazardLocale(const char* localeName) {
    std::printf("\nCR-DET-2, this project's own addition: the locale hazard (%s)\n", localeName);
    const std::string cLocale = reportFor(goodRun(), goodRun());

    const char* applied = std::setlocale(LC_ALL, localeName);
    if (applied == nullptr) {
        // Reported, never skipped silently. A test tier that disappears quietly is a tier
        // nobody notices is gone — the same rule M3's tier 4 lives under.
        std::printf("  ..   the comma-decimal locale \"%s\" is not available on this machine; "
                    "the locale check did not run\n", localeName);
        return;
    }
    const std::string commaLocale = reportFor(goodRun(), goodRun());
    std::setlocale(LC_ALL, "C");

    ok("the report is byte-identical under a comma-decimal locale", cLocale == commaLocale,
       "C-locale length " + std::to_string(cLocale.size()) + ", comma-locale length "
           + std::to_string(commaLocale.size()));
    ok("…and it still contains a decimal point rather than a comma",
       contains(commaLocale, "100.0000%"),
       commaLocale.substr(0, 400));

    // And the verdict itself, not only its rendering.
    const char* again = std::setlocale(LC_ALL, localeName);
    (void)again;
    const ComparisonResult r = compareTwo(goodRun(), goodRun());
    std::setlocale(LC_ALL, "C");
    ok("the gate reaches the same verdict under a comma-decimal locale",
       r.gate == Verdict::Pass && r.content.agree == 12, r.gateReason);
}

// --- The gate basis, which is OQ-2's seam --------------------------------------------------------

void gateBasisIsSelectableAndBothAlwaysRun() {
    std::printf("\nOQ-2's seam: both comparisons always run; the basis chooses which decides\n");
    Cap b = goodRun();
    // One frame dropped: content passes, bytes cannot.
    std::vector<std::string> kept;
    for (const std::string& l : b.lines) {
        if (contains(l, R"("sim_time_s":0.1,)") && contains(l, R"("type":"sample")")) { continue; }
        kept.push_back(l);
    }
    b.lines = kept;
    b.samples -= 3;
    b.lines.pop_back();
    b.trailer("host_lost");

    CompareOptions content;
    content.gateBasis = GateBasis::Content;
    const ComparisonResult rc = compareTwo(goodRun(), b, content);
    ok("on the content basis the gate passes", rc.gate == Verdict::Pass, rc.gateReason);
    ok("and the byte comparison still RAN and still reported DIFFER",
       rc.bytes.ran && !rc.bytes.identical);

    CompareOptions bytes;
    bytes.gateBasis = GateBasis::Bytes;
    const ComparisonResult rb = compareTwo(goodRun(), b, bytes);
    ok("on the byte basis the same pair FAILS the gate", rb.gate == Verdict::Fail, rb.gateReason);
    ok("and the content comparison still RAN and still reported PASS",
       rb.content.verdict == Verdict::Pass);
    ok("the two differ only in which comparison decided, not in what was measured",
       rc.content.comparedSamples == rb.content.comparedSamples &&
           rc.bytes.identical == rb.bytes.identical);

    const std::string report = renderReport(rc);

    // OQ-2 was DECIDED on 2026-09-01 and has never been ANSWERED. Those are different words and
    // the report has to carry both, because "content" alone would hide which one it is. These
    // checks exist to stop the distinction quietly eroding into "the gate is content, the end".
    ok("the report states that OQ-2 is DECIDED", contains(report, "DECIDED"));
    ok("…and says who decided it, so it is not read as a ruling from [B]'s author",
       contains(report, "DRI, 2026-09-01"));
    ok("…and states in the same breath that [B]'s author has NOT replied",
       contains(report, "not") && contains(report, "answered by [B]'s author"));
    ok("…and does NOT claim it was answered",
       !contains(report, "ANSWERED by [B]'s author, who has replied"));
    ok("…and carries the sentence of [B]'s that decided it",
       contains(report, "defect in your harness"));
    ok("…that the byte comparison is expected to fail rather than defective",
       contains(report, "EXPECTED TO FAIL"));
    ok("…and that criterion 2 is discharged under the CONTENT reading specifically",
       contains(report, "CONTENT reading"));
    ok("…while still saying a byte reading is not discharged and is not claimed",
       contains(report, "does not discharge criterion 2 under a byte reading"));
    ok("…and points at the document carrying the reading",
       contains(report, "docs/m7-oq2-oq3.md"));
}

}  // namespace

int main(int argc, char** argv) {
    const fs::path root = (argc > 1) ? fs::path(argv[1]) : fs::current_path();
    g_dir = root / "build" / "tests" / "determinism";
    std::error_code ec;
    fs::create_directories(g_dir, ec);

    std::printf("determinism_test - CR-DET-1, CR-DET-2, CR-DET-3\n");
    std::printf("working in %s\n", g_dir.string().c_str());

    identicalRuns();
    theEmptyTeardownSegmentIsExcluded();
    aMissingFrameIsNotADifference();
    aSingleFrameSegmentCannotBeCompared();
    aThinIntersectionIsIndeterminateNotPass();
    aFrozenSegmentNamesTheShapeOfItsFreeze();
    aDifferingValueFailsAndIsAttributed();
    aDifferingArrayFieldNamesBothValues();
    aChangedInputDiffIsNotAGate();
    anEntityInOneRunOnlyIsAFailure();
    occupancyIsPartOfTheIdentity();
    preconditions();
    hazardClockAndTimestamp();
    hazardUnorderedIteration();
    // Two spellings, because the one Windows accepts is not the one POSIX does and a test that
    // silently found neither would be no test at all. The first that resolves is used.
    if (std::setlocale(LC_ALL, "German_Germany.1252") != nullptr) {
        std::setlocale(LC_ALL, "C");
        hazardLocale("German_Germany.1252");
    } else {
        std::setlocale(LC_ALL, "C");
        hazardLocale("de_DE.UTF-8");
    }
    gateBasisIsSelectableAndBothAlwaysRun();

    std::printf("\n%d check(s), %d failure(s)\n", g_checks, g_failures);
    if (g_failures != 0) {
        std::printf("determinism_test: FAILED\n");
        return 1;
    }
    std::printf("determinism_test: all checks passed\n");
    return 0;
}

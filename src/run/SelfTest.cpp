// EXT-17 — the same-configuration determinism self-test. See SelfTest.h.
#include "SelfTest.h"

#include "../common/Json.h"
#include "../common/Log.h"
#include "../proc/Process.h"

#include <string>

namespace ext17::run {
namespace {

using log::line;

std::string joinPath(const std::string& dir, const std::string& leaf) {
    if (dir.empty()) { return leaf; }
    const char back = dir.back();
    if (back == '\\' || back == '/') { return dir + leaf; }
    return dir + "\\" + leaf;
}

// The capture the run just produced, as a path the comparison can open. A run that recorded
// nothing has no path, and that is already an `infrastructure_error` from M3 — so this returning
// empty is a condition the caller has already ruled out, and it says so rather than assuming it.
std::string capturePathOf(const RunRecord& r, const std::string& runDir) {
    if (r.capturePath.empty()) { return {}; }
    return joinPath(runDir, r.capturePath);
}

}  // namespace

const char* toString(SelfTestOutcome outcome) {
    switch (outcome) {
        case SelfTestOutcome::Passed:              return "passed";
        case SelfTestOutcome::Failed:              return "failed";
        case SelfTestOutcome::InfrastructureError: return "infrastructure_error";
    }
    return "unknown";
}

SelfTestResult runSelfTest(const SelfTestConfig& config) {
    SelfTestResult result;

    const std::string runsDir = joinPath(config.selfTestDir, "runs");
    proc::createDirectories(runsDir);

    line("self-test", "CR-DET-1: the same configuration, run twice, compared. [B] step 4 makes "
                      "this a hard stop, so no campaign run is attempted until it passes.");
    line("self-test", "its two runs are NOT campaign runs. They land in " + runsDir
                          + " and are never counted among the campaign's outcomes.");

    const std::string dirA = joinPath(runsDir, "000");
    const std::string dirB = joinPath(runsDir, "001");

    RunConfig a = config.run;
    a.runId = "000";
    a.runDir = dirA;
    RunConfig b = config.run;
    b.runId = "001";
    b.runDir = dirB;

    line("self-test", "run 000 of 2");
    result.runA = executeRun(a);
    line("self-test", "run 001 of 2");
    result.runB = executeRun(b);

    // CR-EX-5 and tenet 2: infrastructure is never a test result. A self-test whose host would
    // not start has established nothing about determinism, and calling that a determinism failure
    // sends someone to investigate the wrong system.
    if (result.runA.outcome != RunOutcome::Completed ||
        result.runB.outcome != RunOutcome::Completed) {
        result.outcome = SelfTestOutcome::InfrastructureError;
        result.detail = std::string("a self-test run did not complete (000 ")
                        + toString(result.runA.outcome) + ", 001 " + toString(result.runB.outcome)
                        + "). Nothing has been established about determinism either way, so this "
                          "is NOT a determinism failure — it is the harness, and the campaign "
                          "stops because the gate is unproven rather than because it failed.";
        line("self-test", result.detail);
        return result;
    }

    const std::string pathA = capturePathOf(result.runA, dirA);
    const std::string pathB = capturePathOf(result.runB, dirB);
    if (pathA.empty() || pathB.empty()) {
        result.outcome = SelfTestOutcome::InfrastructureError;
        result.detail = "a self-test run produced no capture, so there is nothing to compare. "
                        "The gate is unproven, not failed.";
        line("self-test", result.detail);
        return result;
    }

    result.comparison = compare::compareCaptures(pathA, pathB, "000", "001",
                                                 toString(result.runA.outcome),
                                                 toString(result.runB.outcome), config.compare);
    result.comparisonRan = true;

    // A refusal is a precondition that was not met — an incomplete capture, a pair that is not
    // like for like. It is not a determinism failure, and it is not a pass either. The campaign
    // stops, and the record says which of the two it was.
    if (result.comparison.refusal != compare::Refusal::None) {
        result.outcome = SelfTestOutcome::InfrastructureError;
        result.detail = std::string("the comparison was refused: ")
                        + compare::name(result.comparison.refusal) + " — "
                        + result.comparison.refusalDetail;
    } else if (result.comparison.passed()) {
        result.outcome = SelfTestOutcome::Passed;
        result.detail = result.comparison.gateReason;
    } else {
        result.outcome = SelfTestOutcome::Failed;
        result.detail = result.comparison.gateReason;
    }

    // The whole report goes into the log verbatim, because CR-DET-3's attribution is only useful
    // where somebody will read it, and a campaign log at 3 a.m. is where they will.
    const std::string report = compare::renderReport(result.comparison);
    std::size_t start = 0;
    while (start < report.size()) {
        const std::size_t nl = report.find('\n', start);
        const std::size_t end = (nl == std::string::npos) ? report.size() : nl;
        line("self-test", report.substr(start, end - start));
        if (nl == std::string::npos) { break; }
        start = nl + 1;
    }

    return result;
}

std::string SelfTestResult::toJson() const {
    json::Writer w;
    w.beginObject();
    writeMembers(w);
    w.endObject();
    return w.str();
}

void SelfTestResult::writeMembers(json::Writer& w) const {
    w.member("schema", std::string("ext17-self-test/1"));
    w.member("requirement", std::string("CR-DET-1"));
    w.member("outcome", std::string(toString(outcome)));
    w.member("detail", detail);

    // The deviation, stated in the machine-readable record and not only in the prose. A campaign
    // report that a reader takes at face value has to carry the fact that its gate is not the
    // gate the brief asked for and that the question is open.
    w.beginObject("gate");
    w.member("basis", std::string(compare::name(comparison.gateBasis)));
    w.member("basis_is_a_named_deviation", comparison.gateBasis == compare::GateBasis::Content);
    w.member("verdict", std::string(compare::name(comparison.gate)));
    w.member("oq2_ruling", std::string("unanswered"));
    w.member("oq2_note",
             std::string("Whether the gate is keyed on content or on bytes is out with the owner "
                         "of the brief and has not been ruled on. A pass here does NOT discharge "
                         "the brief's acceptance criterion 2 as written; it discharges it under "
                         "the content reading (ADR-1)."));
    w.endObject();

    w.beginArray("runs");
    for (const RunRecord* r : {&runA, &runB}) {
        w.beginObject();
        w.member("run_id", r->runId);
        w.member("outcome", std::string(toString(r->outcome)));
        w.member("capture", r->capturePath);
        w.member("capture_covers_whole_run", r->captureCoversWholeRun);
        w.member("capture_conformant", r->captureConformant);
        w.member("samples", static_cast<std::int64_t>(r->captureSamples));
        w.endObject();
    }
    w.endArray();

    if (!comparisonRan) {
        w.memberNull("refused");
        w.memberNull("content");
        w.memberNull("bytes");
        w.memberNull("result_equality");
        return;
    }

    if (comparison.refusal != compare::Refusal::None) {
        w.beginObject("refused");
        w.member("code", std::string(compare::name(comparison.refusal)));
        w.member("detail", comparison.refusalDetail);
        w.endObject();
    } else {
        w.memberNull("refused");
    }

    w.beginObject("content");
    w.member("verdict", std::string(compare::name(comparison.content.verdict)));
    w.member("reason", comparison.content.verdictReason);
    w.member("compared_samples", static_cast<std::int64_t>(comparison.content.comparedSamples));
    w.member("agree", static_cast<std::int64_t>(comparison.content.agree));
    w.member("differ", static_cast<std::int64_t>(comparison.content.differ));
    w.member("only_in_000", static_cast<std::int64_t>(comparison.content.onlyInA));
    w.member("only_in_001", static_cast<std::int64_t>(comparison.content.onlyInB));
    w.member("only_in_one_is_not_a_difference", true);
    w.member("coverage", comparison.content.coverage, 6);
    w.member("coverage_floor", comparison.content.coverageFloor, 6);
    w.beginArray("segments");
    for (const compare::SegmentComparison& s : comparison.content.segments) {
        w.beginObject();
        w.member("part", static_cast<std::int64_t>(s.key.part));
        w.member("segment", static_cast<std::int64_t>(s.key.segment));
        w.member("clock_000", std::string(capture::name(s.clockA)));
        w.member("clock_001", std::string(capture::name(s.clockB)));
        w.member("compared", s.compared);
        if (!s.compared) { w.member("excluded_because", s.exclusionReason); }
        w.member("samples_000", static_cast<std::int64_t>(s.samplesA));
        w.member("samples_001", static_cast<std::int64_t>(s.samplesB));
        w.member("compared_samples", static_cast<std::int64_t>(s.comparedSamples));
        w.member("agree", static_cast<std::int64_t>(s.agree));
        w.member("differ", static_cast<std::int64_t>(s.differ));
        w.member("only_in_000", static_cast<std::int64_t>(s.onlyInA));
        w.member("only_in_001", static_cast<std::int64_t>(s.onlyInB));
        // What made a frozen segment frozen. Measured in 2 of 42 captures here: a duplicated
        // publication of identical values, not a reset clock. Both are excluded; only one of
        // them is what the format's §5.1 describes, and a reader needs to know which.
        w.member("duplicated_instants_000", static_cast<std::int64_t>(s.duplicatedInstantsA));
        w.member("duplicated_instants_001", static_cast<std::int64_t>(s.duplicatedInstantsB));
        w.member("duplicated_identical_000", static_cast<std::int64_t>(s.duplicatedIdenticalA));
        w.member("duplicated_identical_001", static_cast<std::int64_t>(s.duplicatedIdenticalB));
        w.endObject();
    }
    w.endArray();
    // CR-DET-3: a failure that names where and in what shape the two runs parted.
    w.beginArray("differences");
    for (const compare::Difference& d : comparison.content.differences) {
        w.beginObject();
        w.member("part", static_cast<std::int64_t>(d.segment.part));
        w.member("segment", static_cast<std::int64_t>(d.segment.segment));
        w.member("entity", d.key.entity);
        w.member("occupancy", static_cast<std::int64_t>(d.key.occupancy));
        w.member("sim_time_s", d.simTimeText);
        w.member("field", d.field);
        w.member("value_000", d.valueA);
        w.member("value_001", d.valueB);
        w.member("line_000", static_cast<std::int64_t>(d.lineA));
        w.member("line_001", static_cast<std::int64_t>(d.lineB));
        w.endObject();
    }
    w.endArray();
    w.endObject();

    w.beginObject("bytes");
    w.member("identical", comparison.bytes.identical);
    w.member("expected_to_fail", true);
    w.member("note", std::string("Run and reported alongside the content comparison, never "
                                 "engineered to pass. The one exclusion is platform.model_path, "
                                 "which the format names as the single host-dependent field."));
    w.member("bytes_000", static_cast<std::int64_t>(comparison.bytes.bytesA));
    w.member("bytes_001", static_cast<std::int64_t>(comparison.bytes.bytesB));
    w.member("headers_identical", comparison.bytes.headersIdentical);
    w.member("model_path_excluded", comparison.bytes.modelPathExcluded);
    w.member("model_path_differed", comparison.bytes.modelPathDiffered);
    w.member("first_differing_offset",
             static_cast<std::int64_t>(comparison.bytes.firstDifferingOffset));
    w.member("first_differing_line",
             static_cast<std::int64_t>(comparison.bytes.firstDifferingLine));
    w.endObject();

    // [B] paragraph 9: "show that the results are identical". A distinct check from the capture
    // comparison, and reported as its own line.
    w.beginObject("result_equality");
    w.member("outcome_000", comparison.results.outcomeA);
    w.member("outcome_001", comparison.results.outcomeB);
    w.member("outcomes_agree", comparison.results.outcomesAgree);
    w.member("verdicts_000", static_cast<std::int64_t>(comparison.results.verdictsA));
    w.member("verdicts_001", static_cast<std::int64_t>(comparison.results.verdictsB));
    w.member("verdicts_agree", comparison.results.verdictsAgree);
    w.member("verdicts_vacuous", comparison.results.verdictsVacuous);
    w.member("verdicts_note",
             std::string("At M4 no conditions are declared, so both runs produce zero verdicts "
                         "and this line is vacuous. It becomes substantive at M6 with CR-AS-3."));
    w.endObject();
}

}  // namespace ext17::run

// EXT-17 — the same-configuration determinism self-test. CR-DET-1, and [B]'s step 4.
//
// > "run the same configuration twice, capture both, and show they match. Keep that as a
// > self-test you run every time, not as something you checked once."
//
// So it is not a command anybody has to remember: `n8ro-campaign repeat` runs it before run 000,
// and a campaign whose self-test does not pass does not execute a single campaign run. [B] makes
// step 4 a hard stop — *"Do not build further until it passes"* — and the honest implementation
// of a hard stop is a stop.
//
// **Its two runs are not campaign runs.** They land in `<out-dir>/selftest/runs/000` and `001`,
// never in `<out-dir>/runs/`, and they are never counted among the campaign's outcomes. A
// campaign that reported twenty-two runs because two of them were the self-test would be
// reporting a number that is wrong, which is tenet 1's whole subject.
//
// The comparison itself is in `src/compare/`, which links nothing. This file links the run path,
// which links the SDK, and that is the allowed direction — the requirement is that the
// comparison links no SDK, not that nothing linking the SDK may link the comparison.
#pragma once

#include "RunOnce.h"
#include "RunRecord.h"
#include "../common/Json.h"
#include "../compare/Compare.h"

#include <string>

namespace ext17::run {

// Why a self-test did not pass. Kept distinct from the comparison's own verdict because
// CR-EX-5 and tenet 2 both say infrastructure is never a test result: a self-test whose host
// would not start has found nothing about determinism, and reporting that as a determinism
// failure would send someone to investigate the wrong system.
enum class SelfTestOutcome {
    Passed,               // the gate passed and the two runs agreed on their outcome
    Failed,               // the gate did not pass. This is a finding about the platform or us
    InfrastructureError,  // a self-test run did not complete. Nothing was established
};

const char* toString(SelfTestOutcome outcome);

struct SelfTestConfig {
    RunConfig run;                        // the configuration to execute twice
    std::string selfTestDir;              // <out-dir>\selftest
    compare::CompareOptions compare;
};

struct SelfTestResult {
    SelfTestOutcome outcome = SelfTestOutcome::InfrastructureError;
    std::string detail;

    RunRecord runA;
    RunRecord runB;
    bool comparisonRan = false;
    compare::ComparisonResult comparison;

    [[nodiscard]] bool passed() const { return outcome == SelfTestOutcome::Passed; }

    // The seam M6 picks up: this object goes into `campaign.json` as `self_test`, so the
    // campaign report can carry the self-test's result without the reporting code knowing
    // anything about how a comparison is made.
    //
    // `writeMembers` writes into an object scope the caller has already opened, which is what
    // lets the same document be both a file of its own and a member of the campaign summary
    // without either one re-deriving it or splicing raw text into the other.
    void writeMembers(json::Writer& w) const;
    [[nodiscard]] std::string toJson() const;
};

// Execute the two runs and compare them. Never throws; every path out returns a result whose
// outcome is the answer.
SelfTestResult runSelfTest(const SelfTestConfig& config);

} // namespace ext17::run

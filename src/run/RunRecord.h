// EXT-17 — the per-run record: one JSON file per run, written whatever the outcome.
//
// It exists because two acceptance criteria need somewhere to point. CR-EX-3 requires that
// "the value it evaluated to is recorded in each per-run record", and CR-EX-4 requires that a
// timed-out run "appears in the report as timeout". A record written only on success would
// satisfy neither, so RunOnce writes one on every path out, including the ones where the host
// never came up.
//
// Two rules govern its contents.
//
//   - **Nothing that varies between identical runs appears outside `diagnostics`.** Wall-clock
//     durations are real evidence for an operator and useless for a comparison, so they are
//     fenced into one object that is named, in the file itself, as excluded from comparison.
//     CR-DET-2's "nothing of ours varies between runs" is then checkable by reading the file
//     rather than by trusting the author.
//   - **The four outcomes are never collapsed** (tenet 2). At M2 there are three — a run that
//     completes is `completed`, not `pass`, because nothing has judged it yet. `pass` and
//     `fail` replace `completed` at M6 when conditions exist. Naming a completed run `pass`
//     now would be the exact collapse the tenet forbids, one milestone early.
#pragma once

#include "StopPredicate.h"
#include "../control/EngineControl.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ext17::run {

enum class RunOutcome {
    Completed,            // the stop predicate was satisfied and teardown was clean
    Timeout,              // the run timeout expired before the predicate was satisfied
    InfrastructureError,  // the harness, the host or the scenario load failed
};

const char* toString(RunOutcome outcome);

struct ProcessRecord {
    std::string role;                          // "host" | "recorder"
    std::string exePath;
    std::uint32_t pid = 0;
    std::optional<std::uint32_t> exitCode;
    bool startedByUs = false;
    bool exitedOnItsOwn = false;
    bool terminatedByHandle = false;
    bool gracefulStopRequested = false;
};

struct RunRecord {
    // /2 at M5: the record gained `parameter` (CR-PAR-1) and the running segment's roster
    // counts. Every addition is additive, and the version moves anyway - a consumer written
    // against /1 has not seen the parameter, and a shape that grew without saying so is how a
    // reader comes to believe it read everything there was.
    std::string schema = "ext17-run-record/2";
    std::string runId;
    RunOutcome outcome = RunOutcome::InfrastructureError;

    // Populated when outcome is InfrastructureError. `stage` names where the run stopped, so
    // the runbook entry for that fault is one lookup rather than a log read.
    std::string errorStage;
    std::string errorDetail;

    std::string scenario;
    std::string modelName;

    std::optional<StopPredicate> predicate;
    StopEvaluation evaluation;
    bool predicateSatisfied = false;

    int runTimeoutMs = 0;
    bool runTimeoutExpired = false;

    std::vector<control::WaitOutcome> waits;
    std::vector<ProcessRecord> processes;

    std::string capturePath;
    std::optional<std::uint64_t> captureBytes;
    // Every part of a rotated set, in name order; one entry for an ordinary capture. A run's
    // capture is a set of files when --on-size-limit is rotate (format 6.7).
    std::vector<std::string> captureParts;
    std::uint64_t captureTotalBytes = 0;

    // What the campaign's own reader made of the capture it just produced. This is the M3
    // deliverable used on the run it was built for, and it exists to answer one question the
    // run record could not otherwise answer: **does this capture cover the run?**
    //
    // With --on-size-limit stop, a run that overruns its byte bound yields a capture that is
    // complete, valid and a third of the run - and the run still reaches its stop predicate and
    // still reports `completed`. Measured at M3: a 1200-frame run bounded at 8 MB recorded to
    // sim_time_s 19.5 of 60.0, conformantly. Nothing outside the file said so. A stored run that
    // M6 will later judge has to state its own coverage, or the judgement is of a third of a run
    // reported as the whole of one.
    bool captureRead = false;
    std::string captureEndReason;
    bool captureCoversWholeRun = false;
    bool captureConformant = false;
    std::string captureDiagnostics;      // "" when there were none
    long long captureSamples = 0;
    long long captureSegmentKeys = 0;
    long long captureRunSegments = 0;   // segment keys minus those cut by a rotation

    // Roster lifecycle in the run's FIRST RUNNING SEGMENT, which is the run proper. Not the
    // whole set: a capture's second segment is the teardown reload, and M4 measured it carrying
    // samples in only five runs of twenty - a count summed over both would move for a reason
    // that has nothing to do with anything the campaign varied.
    //
    // These exist for CR-PAR-2. A sweep needs a per-run result that varies with the parameter,
    // and until M6 declares conditions there are no verdicts to show. `entityAdds` is the
    // honest stand-in: it counts entities the run created - weapons fired, in the committed
    // example - and M2 measured the roster lifecycle agreeing exactly across twenty identical
    // runs, so unlike `samples` it carries no publication-schedule spread.
    long long captureEntityKeys = 0;
    long long captureEntityAdds = 0;
    bool captureRunSegmentFound = false;
    bool recorderAttached = false;
    std::uint64_t captureMaxBytes = 0;
    std::string onSizeLimit;

    std::string hostExe;
    std::string recorderExe;
    std::string simConfigHost;
    std::string simConfigClient;
    std::string modelPath;
    std::string schemaFile;
    std::string n8roRelease;
    std::string pathPrepend;
    std::string hostLoggerCopy;
    std::uint64_t hostLoggerOffset = 0;
    std::optional<std::uint64_t> hostLoggerBytes;

    // CR-PAR-1: "each run's parameter value appears in its per-run record and in the report".
    // `parameterValueText` is the value as the campaign file declared it, character for
    // character; there is no double here on purpose (see RunConfig).
    //
    // `parameterEntitiesMissing` is the check that the axis did anything. The campaign publishes
    // an entity update per named entity and a publish returning true means only that the message
    // reached the bus. So the capture read-back collects which of the named entities actually
    // carried samples, and a name that carried none is listed here - a mistyped entity is then a
    // reported fault rather than a sweep that quietly varied nothing.
    std::string parameterName;
    std::string parameterValueText;
    std::string parameterAppliesTo;
    std::string parameterUnits;
    std::vector<std::string> parameterEntities;
    std::vector<std::string> parameterEntitiesMissing;

    // The final engine snapshot seen after teardown, for the record.
    control::EngineSnapshot finalSnapshot;

    [[nodiscard]] std::string toJson() const;
};

// Write the record to <runDir>\run.json. Returns false and logs on failure; never throws.
bool writeRunRecord(const RunRecord& record, const std::string& path);

} // namespace ext17::run

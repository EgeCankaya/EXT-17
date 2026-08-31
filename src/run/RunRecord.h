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
    std::string schema = "ext17-run-record/1";
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

    // The final engine snapshot seen after teardown, for the record.
    control::EngineSnapshot finalSnapshot;

    [[nodiscard]] std::string toJson() const;
};

// Write the record to <runDir>\run.json. Returns false and logs on failure; never throws.
bool writeRunRecord(const RunRecord& record, const std::string& path);

} // namespace ext17::run
